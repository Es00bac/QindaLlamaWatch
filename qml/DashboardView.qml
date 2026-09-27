// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// The llm-top dashboard as one scrolling column: model, throughput beside
// slots, the host, recent requests. Collectors arrive as properties (never
// as context-property lookups) so qtk-preview can drive the view with fakes.
Tk.Scroll {
    id: view

    property var client
    property var probe
    property var journal

    Connections {
        target: view.client
        ignoreUnknownSignals: true
        function onSampled() { throughput.sample() }
    }
    Connections {
        target: view.probe
        ignoreUnknownSignals: true
        function onSampled() { system.sample() }
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm
        padding: Tk.Theme.space.sm

        ModelPanel {
            objectName: "modelPanel"
            client: view.client
            journal: view.journal
            probe: view.probe
        }

        Tk.Flex {
            wrap: Tk.Flex.Wrap
            gap: Tk.Theme.space.sm

            ThroughputPanel {
                id: throughput
                objectName: "throughputPanel"
                client: view.client
                Tk.Flex.basis: 440
                Tk.Flex.grow: 3
                Tk.Flex.minWidth: 0
            }
            SlotsPanel {
                objectName: "slotsPanel"
                client: view.client
                Tk.Flex.basis: 320
                Tk.Flex.grow: 2
                Tk.Flex.minWidth: 0
            }
        }

        SystemPanel {
            id: system
            objectName: "systemPanel"
            probe: view.probe
        }

        RequestsPanel {
            objectName: "requestsPanel"
            journal: view.journal
        }
    }
}
