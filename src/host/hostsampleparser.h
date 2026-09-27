// SPDX-License-Identifier: GPL-3.0-or-later
//
// Turns the sampler's framed text (src/host/sampler.sh) into HostFrame
// values. Pure and incremental: feed it lines in any batch size.

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace QindaLlamaWatch {

struct DrmClient final {
    QString id;
    qint64 vramKiB = 0;
    qint64 gttKiB = 0;
};

struct ProcReading final {
    qint64 pid = 0;
    qint64 rssKiB = -1;
    qint64 cpuTicks = -1;       // utime + stime from /proc/<pid>/stat
    QList<DrmClient> drm;
};

// One raw sample. Absent readings are simply missing from the maps; a zero
// is a real zero.
struct HostFrame final {
    QHash<QString, QString> scalars;           // "=key value" lines
    QList<QList<qint64>> cpuRows;              // /proc/stat cpu, cpu0, cpu1, ...
    QHash<QString, qint64> meminfo;            // bytes
    QList<qint64> freqKHz;
    QString sclk;                              // active DPM level, e.g. "2400Mhz"
    QString mclk;
    QString nvidiaCsv;
    QList<ProcReading> procs;
};

class HostSampleParser final
{
public:
    // Returns a frame when `line` completes one ("@@end").
    [[nodiscard]] std::optional<HostFrame> feed(const QString &line);

    // Facts printed once per connection ("@@hello"): host, netif, gpu_name,
    // clk_tck, version.
    [[nodiscard]] const QHash<QString, QString> &identity() const { return m_identity; }

    void reset();

private:
    void takeLine(const QString &line);

    QHash<QString, QString> m_identity;
    HostFrame m_frame;
    QString m_section;
    bool m_inFrame = false;
};

// Exposed for tests: /proc/<pid>/stat -> utime + stime, or -1.
[[nodiscard]] qint64 cpuTicksFromProcStat(const QString &statLine);
// "0: 500Mhz\n1: 2400Mhz *" style DPM tables -> "2400Mhz".
[[nodiscard]] QString activeDpmLevel(const QString &line);

} // namespace QindaLlamaWatch
