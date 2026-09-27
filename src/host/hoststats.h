// SPDX-License-Identifier: GPL-3.0-or-later
//
// Derives display figures from consecutive HostFrames: CPU and per-core
// busy percentages, network and llama-server CPU rates, memory breakdown and
// GPU readings. Pure; one instance per connection (it keeps the previous
// frame's counters).

#pragma once

#include "host/hostsampleparser.h"

#include <QVariantMap>

namespace QindaLlamaWatch {

class HostStats final
{
public:
    // AGENT-CONTRACT: the returned keys are read by qml/SystemPanel.qml,
    // qml/ModelPanel.qml and Main.qml (status bar). A reading the host did
    // not provide is ABSENT from the map, never 0, so QML shows a dash.
    //   cpu (%), cores (list of %), load (string "1 5 15"), mhz, cpuTemp (°C),
    //   memTotal, memUsed (programs), memCached, memFree, swapUsed (bytes),
    //   gpuName, gpuBusy (%), vramUsed, vramTotal, gttUsed (bytes), sclk, mclk,
    //   gpuTemp, gpuHotspot (°C), gpuWatts, gpuFanRpm, gpuFanPercent,
    //   llamaProcesses, llamaRss, llamaVram, llamaGtt (bytes; VRAM/GTT only
    //   when DRM fdinfo was readable), llamaCpu (% of the whole machine),
    //   netIf, netRx, netTx (bytes/s), uptime (s), host.
    [[nodiscard]] QVariantMap update(const HostFrame &frame, const QHash<QString, QString> &identity);

    void reset();

private:
    QList<QList<qint64>> m_prevCpu;
    double m_prevUptime = -1;
    qint64 m_prevRx = -1;
    qint64 m_prevTx = -1;
    qint64 m_prevLlamaTicks = -1;
};

} // namespace QindaLlamaWatch
