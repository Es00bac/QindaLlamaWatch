// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QindaTK as Tk
import "Format.js" as Format

// One row per server slot: what it is doing and how full its context is
// (llm-top's SLOTS box).
Tk.Panel {
    id: panel

    property var client
    // ServerJournal: its promptProgress gives a prompt's real length and
    // progress, which /slots does not report on the router build.
    property var journal
    readonly property var slotList: panel.client ? panel.client.slotList : []
    readonly property var progress: panel.journal && panel.journal.promptProgress
                                    ? panel.journal.promptProgress : ({})

    // "reading 30,322 of ~101k (30%) · ~9m 04s left", or the plain
    // count until the journal has reported this task's first batch.
    function readingText(slot) {
        var p = panel.progress[String(slot.id)]
        if (!p || p.task !== slot.task)
            return qsTr("reading · %1 tokens").arg(Format.count(slot.promptProgress))
        var text = qsTr("reading %1").arg(Format.count(p.tokens))
        if (Format.known(p.total)) {
            // decimal thousands: a token count, unlike the binary "128k" of a context size
            text += qsTr(" of ~%1k (%2)").arg(Math.round(p.total / 1000)).arg(Format.percent(p.fraction * 100))
            var rate = panel.client && panel.client.throughput ? panel.client.throughput.promptLive : 0
            if (rate > 0 && p.total > p.tokens)
                text += qsTr(" · ~%1 left").arg(Format.duration((p.total - p.tokens) / rate))
        }
        return text
    }

    title: qsTr("Slots")
    iconName: "layers"
    collapsible: false

    actions: Tk.Badge {
        readonly property int busy: {
            var n = 0
            for (var i = 0; i < panel.slotList.length; ++i)
                if (panel.slotList[i].busy) n++
            return n
        }
        text: qsTr("%1 / %2 busy").arg(busy).arg(panel.slotList.length)
        variant: busy > 0 ? "accent" : "default"
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: 0

        Repeater {
            model: panel.slotList
            delegate: Tk.Flex {
                id: slotRow
                required property var modelData
                readonly property string phase: modelData.phase
                align: Tk.Flex.Center
                gap: Tk.Theme.space.sm
                implicitHeight: Tk.Theme.size.row

                Tk.Badge {
                    dot: true
                    variant: slotRow.phase === "generate" ? "success"
                             : slotRow.phase === "prompt" ? "warning" : "default"
                    Tk.Flex.shrink: 0
                }
                Tk.Mono {
                    text: "#" + slotRow.modelData.id
                    color: Tk.Theme.color.textMuted
                    Tk.Flex.shrink: 0
                }
                Tk.Mono {
                    objectName: "slotPhase_" + slotRow.modelData.id
                    Tk.Flex.grow: 1
                    Tk.Flex.shrink: 1
                    Tk.Flex.minWidth: 0
                    elide: Text.ElideRight
                    text: slotRow.phase === "prompt"
                          // AGENT-NOTE: no "of N": the router's llama-server reports
                          // n_prompt_tokens as the end of the batch being read (always
                          // processed + 2048 on qinda), not the prompt's length.
                          ? panel.readingText(slotRow.modelData)
                          : slotRow.phase === "generate"
                            ? qsTr("generating  %1 tokens").arg(Format.count(slotRow.modelData.decoded))
                            : qsTr("idle")
                    color: slotRow.phase === "generate" ? Tk.Theme.color.accent
                           : slotRow.phase === "prompt" ? Tk.Theme.color.warning
                           : Tk.Theme.color.textMuted
                }
                Tk.Mono {
                    visible: slotRow.modelData.ctx > 0
                    text: Format.count(slotRow.modelData.used) + " / " + Format.kilo(slotRow.modelData.ctx)
                    color: Tk.Theme.color.textMuted
                    Tk.Flex.shrink: 0
                }
                Tk.Meter {
                    visible: slotRow.modelData.ctx > 0
                    Tk.Flex.basis: 96
                    Tk.Flex.shrink: 0
                    implicitHeight: Tk.Theme.size.meter
                    from: 0
                    to: Math.max(1, slotRow.modelData.ctx)
                    value: slotRow.modelData.used
                    ramp: Tk.Theme.ramp.memory
                    radius: Tk.Theme.radius.xs
                    tooltip: qsTr("Context used by slot %1").arg(slotRow.modelData.id)
                }
            }
        }

        Tk.Label {
            visible: panel.slotList.length === 0
            muted: true
            text: !panel.client || panel.client.activeModel.length === 0
                  ? qsTr("No model loaded")
                  : qsTr("Waiting for /slots (the server may run with --no-slots)")
        }
    }
}
