// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk
import "Format.js" as Format

// The server host's CPU, memory, GPU, VRAM and network, plus what the
// llama-server processes themselves hold (llm-top's SYSTEM box), with
// history traces for utilisation and memory.
Tk.Panel {
    id: panel

    property var probe
    readonly property var r: panel.probe ? panel.probe.reading : ({})
    readonly property string probeState: panel.probe ? panel.probe.state : "off"
    readonly property bool hasReading: panel.r.cpu !== undefined || panel.r.memTotal !== undefined
    readonly property bool hasGpu: Format.known(panel.r.vramTotal) || Format.known(panel.r.gpuBusy)

    function share(used, total) {
        return Format.known(used) && Format.known(total) && total > 0 ? 100 * used / total : undefined
    }

    // Called by the owner on every host sample.
    function sample() {
        if (!panel.hasReading)
            return
        var cpu = Format.known(panel.r.cpu) ? panel.r.cpu : 0
        var gpu = Format.known(panel.r.gpuBusy) ? panel.r.gpuBusy : 0
        loadTrace.graph.appendRow([cpu, gpu])
        var ram = panel.share(panel.r.memUsed, panel.r.memTotal)
        var vram = panel.share(panel.r.vramUsed, panel.r.vramTotal)
        memoryTrace.graph.appendRow([ram === undefined ? 0 : ram, vram === undefined ? 0 : vram])
    }

    title: panel.r.host ? qsTr("System · %1").arg(panel.r.host) : qsTr("System")
    iconName: "cpu"
    collapsible: false

    actions: Tk.Badge {
        text: panel.probeState === "live" ? qsTr("live")
              : panel.probeState === "off" ? qsTr("off") : panel.probeState
        variant: panel.probeState === "live" ? "success"
                 : panel.probeState === "retrying" ? "warning" : "default"
        tooltip: panel.probe && panel.probe.error ? panel.probe.error : ""
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Tk.Notice {
            visible: panel.probeState === "retrying" || (panel.probeState === "connecting" && panel.probe.error.length > 0)
            variant: "warning"
            title: qsTr("No host telemetry from %1").arg(panel.probe ? panel.probe.host : "")
            text: panel.probe ? panel.probe.error : ""
        }
        Tk.Notice {
            visible: panel.probeState === "off"
            variant: "info"
            title: qsTr("Host telemetry is off")
            text: qsTr("Set the server host's ssh destination in Settings to see its CPU, GPU, RAM and VRAM.")
        }

        Tk.Flex {
            visible: panel.hasReading
            wrap: Tk.Flex.Wrap
            gap: Tk.Theme.space.md

            // ---- meters
            Tk.Flex {
                direction: Tk.Flex.Column
                gap: 0
                Tk.Flex.basis: 460
                Tk.Flex.grow: 2
                Tk.Flex.minWidth: 0

                MeterRow {
                    objectName: "cpuRow"
                    label: "CPU"
                    known: Format.known(panel.r.cpu)
                    amount: Number(panel.r.cpu)
                    value: Format.percent(panel.r.cpu, 1)
                    ramp: Tk.Theme.ramp.load
                    detail: qsTr("load %1  ·  %2 MHz  ·  %3")
                            .arg(panel.r.load || Format.dash).arg(Format.number(panel.r.mhz))
                            .arg(Format.celsius(panel.r.cpuTemp))
                }
                Tk.Flex {
                    objectName: "coreStrip"
                    visible: (panel.r.cores || []).length > 0
                    implicitHeight: Tk.Theme.size.row
                    align: Tk.Flex.Center
                    gap: 1
                    Tk.Mono {
                        text: qsTr("cores")
                        width: 44
                        color: Tk.Theme.color.textMuted
                        Tk.Flex.shrink: 0
                    }
                    Repeater {
                        model: panel.r.cores || []
                        delegate: Tk.Meter {
                            required property var modelData
                            required property int index
                            vertical: true
                            implicitWidth: Tk.Theme.size.meter
                            implicitHeight: Tk.Theme.size.row - Tk.Theme.space.xs
                            Tk.Flex.shrink: 1
                            value: modelData
                            ramp: Tk.Theme.ramp.load
                            radius: 1
                            tooltip: qsTr("core %1: %2").arg(index).arg(Format.percent(modelData))
                        }
                    }
                }
                MeterRow {
                    objectName: "ramRow"
                    label: "RAM"
                    // The model is memory-mapped into the page cache, which
                    // the kernel calls "available": the bar shows everything
                    // not free, the text splits programs from cache.
                    known: Format.known(panel.r.memTotal)
                    amount: panel.share(Number(panel.r.memTotal) - Number(panel.r.memFree), panel.r.memTotal) || 0
                    value: Format.bytes(panel.r.memUsed)
                    ramp: Tk.Theme.ramp.memory
                    detail: qsTr("programs + %1 cache of %2  ·  swap %3")
                            .arg(Format.bytes(panel.r.memCached)).arg(Format.bytes(panel.r.memTotal))
                            .arg(Format.bytes(panel.r.swapUsed))
                }
                MeterRow {
                    objectName: "gpuRow"
                    visible: panel.hasGpu
                    label: "GPU"
                    known: Format.known(panel.r.gpuBusy)
                    amount: Number(panel.r.gpuBusy)
                    value: Format.percent(panel.r.gpuBusy)
                    ramp: Tk.Theme.ramp.load
                    detail: [
                        (panel.r.sclk || Format.dash) + " / mem " + (panel.r.mclk || Format.dash),
                        Format.number(panel.r.gpuWatts) + " W",
                        Format.celsius(panel.r.gpuTemp)
                            + (Format.known(panel.r.gpuHotspot) ? qsTr(" (hotspot %1)").arg(Format.celsius(panel.r.gpuHotspot)) : ""),
                        Format.known(panel.r.gpuFanRpm) ? qsTr("fan %1 rpm").arg(Format.number(panel.r.gpuFanRpm))
                            : Format.known(panel.r.gpuFanPercent) ? qsTr("fan %1").arg(Format.percent(panel.r.gpuFanPercent)) : ""
                    ].filter(function (s) { return s.length > 0 }).join("  ·  ")
                }
                MeterRow {
                    objectName: "vramRow"
                    visible: panel.hasGpu
                    label: "VRAM"
                    known: Format.known(panel.r.vramTotal)
                    amount: panel.share(panel.r.vramUsed, panel.r.vramTotal) || 0
                    value: Format.bytes(panel.r.vramUsed)
                    ramp: Tk.Theme.ramp.memory
                    detail: qsTr("of %1").arg(Format.bytes(panel.r.vramTotal))
                            + (Format.known(panel.r.gttUsed) ? qsTr("  ·  GTT %1").arg(Format.bytes(panel.r.gttUsed)) : "")
                            + (panel.r.gpuName ? "  ·  " + panel.r.gpuName : "")
                }
                MeterRow {
                    objectName: "llamaRow"
                    label: "llama"
                    known: Format.known(panel.r.llamaCpu)
                    amount: Number(panel.r.llamaCpu)
                    value: Format.percent(panel.r.llamaCpu, 1)
                    ramp: Tk.Theme.ramp.load
                    detail: panel.r.llamaProcesses > 0
                            ? qsTr("%n llama-server process(es)  ·  %1 resident", "", panel.r.llamaProcesses)
                                  .arg(Format.bytes(panel.r.llamaRss))
                              + (Format.known(panel.r.llamaVram)
                                 ? qsTr("  ·  %1 VRAM").arg(Format.bytes(panel.r.llamaVram)) : "")
                            : qsTr("llama-server is not running")
                }
                MeterRow {
                    objectName: "netRow"
                    label: "NET"
                    known: Format.known(panel.r.netRx)
                    // Throughput has no natural ceiling; scale to 1 Gbit/s.
                    amount: Number(panel.r.netRx) + Number(panel.r.netTx)
                    to: 125000000
                    ramp: Tk.Theme.ramp.network
                    valueWidth: 96
                    value: "↓ " + Format.rate(panel.r.netRx)
                    detail: qsTr("↑ %1  ·  %2  ·  host up %3")
                            .arg(Format.rate(panel.r.netTx)).arg(panel.r.netIf || Format.dash)
                            .arg(Format.duration(panel.r.uptime))
                }
            }

            // ---- history
            Tk.Flex {
                direction: Tk.Flex.Column
                gap: Tk.Theme.space.xs
                Tk.Flex.basis: 280
                Tk.Flex.grow: 1
                Tk.Flex.minWidth: 0

                TraceGraph {
                    id: loadTrace
                    Tk.Flex.alignSelf: Tk.Flex.Stretch
                    cornerText: qsTr("CPU · GPU %")
                    Tk.GraphSeries { ramp: Tk.Theme.ramp.load; label: "CPU" }
                    Tk.GraphSeries { color: Tk.Theme.color.info; fill: false; label: "GPU" }
                }
                TraceGraph {
                    id: memoryTrace
                    Tk.Flex.alignSelf: Tk.Flex.Stretch
                    cornerText: qsTr("RAM · VRAM %")
                    Tk.GraphSeries { ramp: Tk.Theme.ramp.memory; label: "RAM" }
                    Tk.GraphSeries { color: Tk.Theme.color.info; fill: false; label: "VRAM" }
                }
            }
        }
    }
}
