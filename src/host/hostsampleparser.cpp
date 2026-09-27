// SPDX-License-Identifier: GPL-3.0-or-later

#include "host/hostsampleparser.h"

#include <utility>

namespace QindaLlamaWatch {

namespace {

QStringList fields(const QString &line)
{
    return line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

} // namespace

qint64 cpuTicksFromProcStat(const QString &statLine)
{
    // The command name is parenthesised and may itself contain spaces or
    // parentheses; fields are counted from the last ')'.
    const qsizetype close = statLine.lastIndexOf(QLatin1Char(')'));
    if (close < 0) {
        return -1;
    }
    const QStringList rest = fields(statLine.mid(close + 1));
    // rest[0] is field 3 (state); utime is field 14, stime field 15.
    if (rest.size() < 13) {
        return -1;
    }
    bool okU = false;
    bool okS = false;
    const qint64 utime = rest.at(11).toLongLong(&okU);
    const qint64 stime = rest.at(12).toLongLong(&okS);
    return okU && okS ? utime + stime : -1;
}

QString activeDpmLevel(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.endsWith(QLatin1Char('*'))) {
        return {};
    }
    const QStringList parts = fields(trimmed);
    return parts.size() >= 2 ? parts.at(1) : QString();
}

void HostSampleParser::reset()
{
    m_identity.clear();
    m_frame = {};
    m_section.clear();
    m_inFrame = false;
}

std::optional<HostFrame> HostSampleParser::feed(const QString &rawLine)
{
    const QString line = rawLine.trimmed();
    if (line.startsWith(QLatin1String("@@"))) {
        const QString marker = line.mid(2);
        if (marker == QLatin1String("hello")) {
            m_identity.clear();
            m_inFrame = false;
            m_section = marker;
        } else if (marker == QLatin1String("begin")) {
            m_frame = {};
            m_inFrame = true;
            m_section.clear();
        } else if (marker == QLatin1String("end")) {
            if (!m_inFrame) {
                return std::nullopt;
            }
            m_inFrame = false;
            m_section.clear();
            return std::exchange(m_frame, {});
        } else if (marker.startsWith(QLatin1String("proc "))) {
            ProcReading proc;
            proc.pid = marker.mid(5).trimmed().toLongLong();
            m_frame.procs.append(proc);
            m_section = QStringLiteral("proc");
        } else {
            m_section = marker;
        }
        return std::nullopt;
    }
    if (!line.isEmpty()) {
        takeLine(line);
    }
    return std::nullopt;
}

void HostSampleParser::takeLine(const QString &line)
{
    if (m_section == QLatin1String("hello")) {
        if (line.startsWith(QLatin1Char('='))) {
            const qsizetype space = line.indexOf(QLatin1Char(' '));
            const QString key = line.mid(1, space < 0 ? -1 : space - 1);
            m_identity.insert(key, space < 0 ? QString() : line.mid(space + 1).trimmed());
        }
        return;
    }
    if (!m_inFrame) {
        return;
    }

    if (line.startsWith(QLatin1Char('='))) {
        const qsizetype space = line.indexOf(QLatin1Char(' '));
        const QString key = line.mid(1, space < 0 ? -1 : space - 1);
        const QString value = space < 0 ? QString() : line.mid(space + 1).trimmed();
        if (m_section == QLatin1String("proc") && !m_frame.procs.isEmpty()) {
            ProcReading &proc = m_frame.procs.last();
            if (key == QLatin1String("rss_kib")) {
                proc.rssKiB = value.toLongLong();
            } else if (key == QLatin1String("pstat")) {
                proc.cpuTicks = cpuTicksFromProcStat(value);
            } else if (key == QLatin1String("drm")) {
                const QStringList parts = fields(value);
                if (!parts.isEmpty()) {
                    DrmClient client;
                    client.id = parts.at(0);
                    client.vramKiB = parts.value(1).toLongLong();
                    client.gttKiB = parts.value(2).toLongLong();
                    proc.drm.append(client);
                }
            }
            return;
        }
        // An empty value (a sysfs file that does not exist) is "absent".
        if (!value.isEmpty()) {
            m_frame.scalars.insert(key, value);
        }
        return;
    }

    if (m_section == QLatin1String("stat")) {
        const QStringList parts = fields(line);
        if (parts.size() >= 5 && parts.first().startsWith(QLatin1String("cpu"))) {
            QList<qint64> row;
            // user nice system idle iowait irq softirq steal; guest time is
            // already inside user/nice, so it is not summed twice.
            for (qsizetype i = 1; i < parts.size() && i <= 8; ++i) {
                row.append(parts.at(i).toLongLong());
            }
            m_frame.cpuRows.append(row);
        }
    } else if (m_section == QLatin1String("meminfo")) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon > 0) {
            const QStringList parts = fields(line.mid(colon + 1));
            if (!parts.isEmpty()) {
                const qint64 factor = parts.value(1) == QLatin1String("kB") ? 1024 : 1;
                m_frame.meminfo.insert(line.left(colon), parts.first().toLongLong() * factor);
            }
        }
    } else if (m_section == QLatin1String("freq")) {
        bool ok = false;
        const qint64 khz = line.toLongLong(&ok);
        if (ok) {
            m_frame.freqKHz.append(khz);
        }
    } else if (m_section == QLatin1String("sclk")) {
        if (const QString level = activeDpmLevel(line); !level.isEmpty()) {
            m_frame.sclk = level;
        }
    } else if (m_section == QLatin1String("mclk")) {
        if (const QString level = activeDpmLevel(line); !level.isEmpty()) {
            m_frame.mclk = level;
        }
    } else if (m_section == QLatin1String("nvidia")) {
        if (m_frame.nvidiaCsv.isEmpty()) {
            m_frame.nvidiaCsv = line;
        }
    }
}

} // namespace QindaLlamaWatch
