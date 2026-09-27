// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "Format.js" as Format

// Tokens per second now (from slot progress between polls) and over the
// server's lifetime (from /metrics), with a history trace for each:
// llm-top's THROUGHPUT box.
Tk.Panel {
    id: panel

    property var client
    readonly property var t: panel.client ? panel.client.throughput : ({})

    // Called by the owner once per llama poll; history lives in the graphs.
    function sample() {
        generateTrace.graph.append(Format.known(panel.t.generateLive) ? panel.t.generateLive : 0)
        promptTrace.graph.append(Format.known(panel.t.promptLive) ? panel.t.promptLive : 0)
    }

    title: qsTr("Throughput")
    iconName: "gauge"
    collapsible: false

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Tk.Grid {
            columns: "auto auto 1fr"
            columnGap: Tk.Theme.space.md
            rowGap: Tk.Theme.space.xs
            alignItems: Tk.Grid.Center

            Tk.Caption { text: qsTr("generate") }
            Tk.Flex {
                direction: Tk.Flex.Column
                gap: 0
                Tk.Mono {
                    objectName: "generateLive"
                    text: Format.tps(panel.t.generateLive)
                    font.pixelSize: Tk.Theme.font.title
                    font.weight: Font.DemiBold
                    color: Tk.Theme.color.accent
                }
                Tk.Caption {
                    text: qsTr("avg %1").arg(Format.tps(panel.t.generateAvg))
                }
            }
            TraceGraph {
                id: generateTrace
                Tk.Grid.alignSelf: Tk.Grid.Stretch
                autoScale: true
                autoScaleFloor: 10
                scaleFormatter: function (v) { return Math.round(v) + " t/s" }
                Tk.GraphSeries { color: Tk.Theme.color.accent; label: qsTr("generate") }
            }

            Tk.Caption { text: qsTr("prompt") }
            Tk.Flex {
                direction: Tk.Flex.Column
                gap: 0
                Tk.Mono {
                    objectName: "promptLive"
                    text: Format.tps(panel.t.promptLive, 0)
                    font.pixelSize: Tk.Theme.font.title
                    font.weight: Font.DemiBold
                    color: Tk.Theme.color.warning
                }
                Tk.Caption {
                    text: qsTr("avg %1").arg(Format.tps(panel.t.promptAvg, 0))
                }
            }
            TraceGraph {
                id: promptTrace
                Tk.Grid.alignSelf: Tk.Grid.Stretch
                autoScale: true
                autoScaleFloor: 100
                scaleFormatter: function (v) { return Math.round(v) + " t/s" }
                Tk.GraphSeries { color: Tk.Theme.color.warning; label: qsTr("prompt") }
            }
        }

        Tk.Divider {}

        Tk.Grid {
            columns: "auto 1fr auto 1fr"
            columnGap: Tk.Theme.space.md
            rowGap: 0
            alignItems: Tk.Grid.Center

            Tk.Caption { text: qsTr("prompt") }
            Tk.Mono { text: Format.count(panel.t.promptTotal) }
            Tk.Caption { text: qsTr("generated") }
            Tk.Mono { text: Format.count(panel.t.generatedTotal) }
            Tk.Caption { text: qsTr("cached") }
            Tk.Mono { text: Format.count(panel.t.cachedTotal) }
            Tk.Caption { text: qsTr("queue") }
            Tk.Mono {
                objectName: "queue"
                text: qsTr("%1 running · %2 waiting")
                      .arg(Format.number(panel.t.processing)).arg(Format.number(panel.t.deferred))
                color: Number(panel.t.deferred) > 0 ? Tk.Theme.color.warning : Tk.Theme.color.text
            }
        }

        MeterRow {
            visible: Format.known(panel.t.kvUsage)
            label: qsTr("KV")
            known: Format.known(panel.t.kvUsage)
            amount: Number(panel.t.kvUsage) * 100
            value: Format.percent(Number(panel.t.kvUsage) * 100)
            ramp: Tk.Theme.ramp.memory
            detail: qsTr("KV cache in use")
        }
    }
}
