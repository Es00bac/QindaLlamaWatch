// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk
import "Format.js" as Format

// What is loaded and how it sits on the machine: llm-top's MODEL box.
// Placement comes from three sources: the journal's load facts (layers,
// buffer sizes), the host probe (llama-server RSS, and VRAM when DRM fdinfo
// is readable) and /props (context, slots).
Tk.Panel {
    id: panel

    property var client
    property var journal
    property var probe

    readonly property var server: panel.client ? panel.client.server : ({})
    readonly property var facts: panel.journal ? panel.journal.facts : ({})
    readonly property var reading: panel.probe ? panel.probe.reading : ({})

    title: qsTr("Model")
    iconName: "sparkles"
    collapsible: false

    function placement() {
        var parts = []
        var f = panel.facts
        var r = panel.reading
        if (f.layers)
            parts.push(qsTr("GPU layers %1").arg(String(f.layers).replace("/", " of ")))
        if (Format.known(r.llamaVram))
            parts.push(qsTr("%1 VRAM + %2 GTT").arg(Format.bytes(r.llamaVram)).arg(Format.bytes(r.llamaGtt)))
        if (Format.known(r.llamaRss) && r.llamaProcesses > 0)
            parts.push(qsTr("%1 resident in RAM").arg(Format.bytes(r.llamaRss)))
        if (f.n_cpu_moe)
            parts.push(qsTr("experts of %1 layers on CPU").arg(f.n_cpu_moe))
        if (f.gpu_buf)
            parts.push(qsTr("weights %1 GiB GPU").arg((Number(f.gpu_buf) / 1024).toFixed(1))
                       + (f.cpu_buf ? qsTr(" + %1 GiB CPU").arg((Number(f.cpu_buf) / 1024).toFixed(1)) : ""))
        if (f.fa)
            parts.push(qsTr("flash-attn %1").arg(f.fa))
        return parts.length > 0 ? parts.join("  ·  ") : Format.dash
    }

    function sampling() {
        var list = panel.server.sampling || []
        var parts = []
        for (var i = 0; i < list.length; ++i)
            parts.push(list[i].label + " " + list[i].value)
        return parts.length > 0 ? parts.join("  ·  ") : Format.dash
    }

    actions: Tk.Flex {
        align: Tk.Flex.Center
        gap: Tk.Theme.space.xs
        Tk.Button {
            small: true
            variant: "ghost"
            iconName: "skip-forward"
            text: qsTr("Next model")
            tooltip: qsTr("Load the model after the active one (Ctrl+M)")
            visible: panel.client && panel.client.routerMode
            onClicked: panel.client.loadNextModel()
        }
        Tk.Button {
            small: true
            variant: "ghost"
            iconName: "circle-off"
            text: qsTr("Unload")
            tooltip: qsTr("Unload the active model (Ctrl+U)")
            visible: panel.client && panel.client.routerMode && panel.client.activeModel.length > 0
            onClicked: panel.client.unloadModel(panel.client.activeModel)
        }
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.xs

        Tk.Flex {
            objectName: "modelChips"
            wrap: Tk.Flex.Wrap
            gap: Tk.Theme.space.xs
            align: Tk.Flex.Center

            Repeater {
                model: panel.client ? panel.client.models : []
                delegate: Tk.Chip {
                    required property var modelData
                    readonly property bool loaded: modelData.status === "loaded"
                    small: true
                    checkable: false
                    active: loaded
                    emphasis: loaded ? "solid" : "plain"
                    iconName: loaded ? "circle-dot"
                                     : modelData.status === "loading" ? "loader" : "circle"
                    text: modelData.id + "  " + modelData.status
                    tooltip: loaded || !panel.client.routerMode
                             ? qsTr("%1 is %2").arg(modelData.id).arg(modelData.status)
                             : qsTr("Load %1").arg(modelData.id)
                    onClicked: {
                        if (!loaded && panel.client.routerMode)
                            panel.client.loadModel(modelData.id)
                    }
                }
            }
            Tk.Label {
                visible: !panel.client || panel.client.models.length === 0
                text: panel.client && panel.client.state === "online"
                      ? qsTr("The server lists no models")
                      : qsTr("Server unreachable")
                muted: true
            }
        }

        Tk.Grid {
            columns: "auto 1fr"
            columnGap: Tk.Theme.space.md
            rowGap: 0
            alignItems: Tk.Grid.Center

            Tk.Caption { text: qsTr("file") }
            Tk.Mono {
                objectName: "modelFile"
                text: Format.basename(panel.server.modelPath)
                elide: Text.ElideMiddle
            }
            Tk.Caption { text: qsTr("context") }
            Tk.Mono {
                objectName: "modelContext"
                text: qsTr("%1 tokens  ·  %2 slots%3")
                      .arg(Format.count(panel.server.ctx))
                      .arg(Format.number(panel.server.totalSlots))
                      .arg(panel.facts.kv ? qsTr("  ·  KV cache %1 MiB").arg(Math.round(Number(panel.facts.kv))) : "")
            }
            Tk.Caption { text: qsTr("placement") }
            Tk.Mono {
                objectName: "modelPlacement"
                text: panel.placement()
                elide: Text.ElideRight
            }
            Tk.Caption { text: qsTr("sampling") }
            Tk.Mono {
                text: panel.sampling()
                color: Tk.Theme.color.textMuted
                elide: Text.ElideRight
            }
            Tk.Caption { text: qsTr("build") }
            Tk.Mono {
                text: panel.server.build ? "llama.cpp " + panel.server.build : Format.dash
                color: Tk.Theme.color.textMuted
                elide: Text.ElideRight
            }
        }
    }
}
