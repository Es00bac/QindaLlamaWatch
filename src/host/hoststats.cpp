// SPDX-License-Identifier: GPL-3.0-or-later

#include "host/hoststats.h"

#include <QSet>
#include <QVariantList>

#include <algorithm>
#include <numeric>
#include <utility>

namespace QindaLlamaWatch {

namespace {

qint64 sum(const QList<qint64> &row)
{
    return std::accumulate(row.cbegin(), row.cend(), qint64{0});
}

double busyPercent(const QList<qint64> &before, const QList<qint64> &after)
{
    if (before.size() < 5 || after.size() < 5) {
        return 0.0;
    }
    // idle + iowait count as not busy, as in qinda-llm-top.
    const qint64 idle = (after.at(3) + after.at(4)) - (before.at(3) + before.at(4));
    const qint64 total = sum(after) - sum(before);
    if (total <= 0) {
        return 0.0;
    }
    return std::clamp(100.0 * (1.0 - double(idle) / double(total)), 0.0, 100.0);
}

bool scalarInt(const HostFrame &frame, const char *key, qint64 *out)
{
    const auto it = frame.scalars.constFind(QString::fromLatin1(key));
    if (it == frame.scalars.cend()) {
        return false;
    }
    bool ok = false;
    *out = it.value().toLongLong(&ok);
    return ok;
}

void putInt(QVariantMap &map, const HostFrame &frame, const char *key, const char *as,
            double scale = 1.0)
{
    qint64 value = 0;
    if (scalarInt(frame, key, &value)) {
        map.insert(QString::fromLatin1(as), double(value) * scale);
    }
}

void takeNvidia(QVariantMap &map, const QString &csv)
{
    // utilization.gpu, memory.used MiB, memory.total MiB, temperature.gpu,
    // power.draw W, fan.speed %, clocks.sm MHz, clocks.mem MHz, name
    const QStringList parts = csv.split(QLatin1Char(','));
    if (parts.size() < 9) {
        return;
    }
    auto number = [&parts](int i, double *out) {
        bool ok = false;
        *out = parts.at(i).trimmed().toDouble(&ok);
        return ok;
    };
    double v = 0;
    if (number(0, &v)) map.insert(QStringLiteral("gpuBusy"), v);
    if (number(1, &v)) map.insert(QStringLiteral("vramUsed"), v * 1024 * 1024);
    if (number(2, &v)) map.insert(QStringLiteral("vramTotal"), v * 1024 * 1024);
    if (number(3, &v)) map.insert(QStringLiteral("gpuTemp"), v);
    if (number(4, &v)) map.insert(QStringLiteral("gpuWatts"), v);
    if (number(5, &v)) map.insert(QStringLiteral("gpuFanPercent"), v);
    if (number(6, &v)) map.insert(QStringLiteral("sclk"), QStringLiteral("%1Mhz").arg(v));
    if (number(7, &v)) map.insert(QStringLiteral("mclk"), QStringLiteral("%1Mhz").arg(v));
    map.insert(QStringLiteral("gpuName"), parts.mid(8).join(QLatin1Char(',')).trimmed());
}

} // namespace

void HostStats::reset()
{
    *this = HostStats();
}

QVariantMap HostStats::update(const HostFrame &frame, const QHash<QString, QString> &identity)
{
    QVariantMap out;
    out.insert(QStringLiteral("host"), identity.value(QStringLiteral("host")));

    // Elapsed time comes from the host's own uptime, so ssh/network jitter
    // in when a frame arrives never distorts a rate.
    double uptime = -1;
    if (const auto it = frame.scalars.constFind(QStringLiteral("uptime")); it != frame.scalars.cend()) {
        uptime = it.value().section(QLatin1Char(' '), 0, 0).toDouble();
        out.insert(QStringLiteral("uptime"), uptime);
    }
    const double dt = (m_prevUptime >= 0 && uptime > m_prevUptime) ? uptime - m_prevUptime : 0.0;

    // ---- CPU
    if (!frame.cpuRows.isEmpty()) {
        if (m_prevCpu.size() == frame.cpuRows.size()) {
            out.insert(QStringLiteral("cpu"), busyPercent(m_prevCpu.first(), frame.cpuRows.first()));
            QVariantList cores;
            for (qsizetype i = 1; i < frame.cpuRows.size(); ++i) {
                cores.append(busyPercent(m_prevCpu.at(i), frame.cpuRows.at(i)));
            }
            out.insert(QStringLiteral("cores"), cores);
        }
        m_prevCpu = frame.cpuRows;
    }
    if (const auto it = frame.scalars.constFind(QStringLiteral("loadavg")); it != frame.scalars.cend()) {
        out.insert(QStringLiteral("load"), it.value().section(QLatin1Char(' '), 0, 2));
    }
    if (!frame.freqKHz.isEmpty()) {
        out.insert(QStringLiteral("mhz"), double(sum(frame.freqKHz)) / frame.freqKHz.size() / 1000.0);
    }
    putInt(out, frame, "cpu_temp", "cpuTemp", 1e-3);

    // ---- Memory. The model file is memory-mapped: it lives in the page
    // cache, which the kernel counts as available, so programs and cache are
    // reported separately (qinda-llm-top does the same).
    const qint64 memTotal = frame.meminfo.value(QStringLiteral("MemTotal"), -1);
    if (memTotal > 0) {
        const qint64 available = frame.meminfo.value(QStringLiteral("MemAvailable"), 0);
        out.insert(QStringLiteral("memTotal"), double(memTotal));
        out.insert(QStringLiteral("memUsed"), double(memTotal - available));
        out.insert(QStringLiteral("memCached"), double(frame.meminfo.value(QStringLiteral("Cached"), 0)));
        out.insert(QStringLiteral("memFree"), double(frame.meminfo.value(QStringLiteral("MemFree"), 0)));
        out.insert(QStringLiteral("swapUsed"),
                   double(frame.meminfo.value(QStringLiteral("SwapTotal"), 0)
                          - frame.meminfo.value(QStringLiteral("SwapFree"), 0)));
    }

    // ---- GPU (AMD sysfs, or one nvidia-smi CSV row)
    if (const QString name = identity.value(QStringLiteral("gpu_name")); !name.isEmpty()) {
        out.insert(QStringLiteral("gpuName"), name);
    }
    putInt(out, frame, "gpu_busy", "gpuBusy");
    putInt(out, frame, "vram_used", "vramUsed");
    putInt(out, frame, "vram_total", "vramTotal");
    putInt(out, frame, "gtt_used", "gttUsed");
    putInt(out, frame, "gpu_temp", "gpuTemp", 1e-3);
    putInt(out, frame, "gpu_hot", "gpuHotspot", 1e-3);
    putInt(out, frame, "gpu_power", "gpuWatts", 1e-6);
    putInt(out, frame, "gpu_fan", "gpuFanRpm");
    if (!frame.sclk.isEmpty()) out.insert(QStringLiteral("sclk"), frame.sclk);
    if (!frame.mclk.isEmpty()) out.insert(QStringLiteral("mclk"), frame.mclk);
    if (!frame.nvidiaCsv.isEmpty()) takeNvidia(out, frame.nvidiaCsv);

    // ---- llama-server processes (the router and one child per model)
    out.insert(QStringLiteral("llamaProcesses"), frame.procs.size());
    if (!frame.procs.isEmpty()) {
        qint64 rss = 0;
        qint64 ticks = 0;
        bool ticksKnown = true;
        QHash<QString, DrmClient> clients;   // one count per DRM client, as llm-top
        for (const ProcReading &proc : frame.procs) {
            rss += std::max<qint64>(proc.rssKiB, 0) * 1024;
            ticksKnown = ticksKnown && proc.cpuTicks >= 0;
            ticks += std::max<qint64>(proc.cpuTicks, 0);
            for (const DrmClient &client : proc.drm) {
                clients.insert(client.id, client);
            }
        }
        out.insert(QStringLiteral("llamaRss"), double(rss));
        if (!clients.isEmpty()) {
            qint64 vram = 0;
            qint64 gtt = 0;
            for (const DrmClient &client : std::as_const(clients)) {
                vram += client.vramKiB;
                gtt += client.gttKiB;
            }
            out.insert(QStringLiteral("llamaVram"), double(vram) * 1024);
            out.insert(QStringLiteral("llamaGtt"), double(gtt) * 1024);
        }
        const double hz = identity.value(QStringLiteral("clk_tck"), QStringLiteral("100")).toDouble();
        const qsizetype cpuCount = std::max<qsizetype>(frame.cpuRows.size() - 1, 1);
        // AGENT-GUARD: a restarted server has fewer ticks than the previous
        // frame; the negative delta must be dropped, not shown as 0 %.
        if (ticksKnown && m_prevLlamaTicks >= 0 && dt > 0 && hz > 0 && ticks >= m_prevLlamaTicks) {
            const double coresBusy = double(ticks - m_prevLlamaTicks) / hz / dt;
            out.insert(QStringLiteral("llamaCpu"), std::clamp(100.0 * coresBusy / double(cpuCount), 0.0, 100.0));
        }
        m_prevLlamaTicks = ticksKnown ? ticks : -1;
    } else {
        m_prevLlamaTicks = -1;
    }

    // ---- Network (tailnet interface if present, else the default route)
    const QString netIf = identity.value(QStringLiteral("netif"));
    if (!netIf.isEmpty()) {
        out.insert(QStringLiteral("netIf"), netIf);
    }
    qint64 rx = -1;
    qint64 tx = -1;
    if (scalarInt(frame, "net_rx", &rx) && scalarInt(frame, "net_tx", &tx)) {
        if (m_prevRx >= 0 && dt > 0 && rx >= m_prevRx && tx >= m_prevTx) {
            out.insert(QStringLiteral("netRx"), double(rx - m_prevRx) / dt);
            out.insert(QStringLiteral("netTx"), double(tx - m_prevTx) / dt);
        }
        m_prevRx = rx;
        m_prevTx = tx;
    }

    m_prevUptime = uptime;
    return out;
}

} // namespace QindaLlamaWatch
