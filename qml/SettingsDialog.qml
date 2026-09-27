// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// Connection settings. Fields are edited locally and applied together, so
// the collectors reconnect once per Save rather than per keystroke.
Tk.Dialog {
    id: dialog

    property var settings

    title: qsTr("Settings")
    subtitle: qsTr("Where the llama.cpp server is and how to reach its host")
    primaryText: qsTr("Save")
    secondaryText: qsTr("Cancel")
    dialogWidth: 520
    primaryEnabled: urlField.text.trim().length > 0

    function load() {
        urlField.text = dialog.settings.serverUrl
        keyField.text = dialog.settings.apiKeyFile
        intervalField.value = dialog.settings.intervalMs
        sshField.text = dialog.settings.sshHost
        unitField.text = dialog.settings.journalUnit
        sudoBox.checked = dialog.settings.journalSudo
    }

    onAboutToShow: dialog.load()
    onAccepted: dialog.settings.apply(urlField.text, sshField.text, keyField.text,
                                      intervalField.value, unitField.text, sudoBox.checked)

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.sm

        Tk.SectionHeader { title: qsTr("llama.cpp server") }
        Tk.Grid {
            columns: "auto 1fr"
            columnGap: Tk.Theme.space.md
            rowGap: Tk.Theme.space.xs
            alignItems: Tk.Grid.Center

            Tk.Caption { text: qsTr("Server URL") }
            Tk.TextField {
                id: urlField
                objectName: "serverUrlField"
                mono: true
                placeholderText: "http://qinda:8100"
                tooltip: qsTr("Root URL of llama-server; a trailing /v1 is accepted")
            }
            Tk.Caption { text: qsTr("API key file") }
            Tk.TextField {
                id: keyField
                mono: true
                placeholderText: "~/.config/qinda-llm/api-key"
                tooltip: qsTr("File whose first line is the server's API key; empty for none")
                error: text.trim().length > 0 && !dialog.settings.apiKeyFound && text === dialog.settings.apiKeyFile
            }
            Tk.Caption { text: qsTr("Refresh (ms)") }
            Tk.NumberField {
                id: intervalField
                from: 250
                to: 10000
                stepSize: 250
                tooltip: qsTr("How often the server and host are sampled")
            }
        }

        Tk.SectionHeader { title: qsTr("Server host (CPU, GPU, RAM, VRAM, journal)") }
        Tk.Grid {
            columns: "auto 1fr"
            columnGap: Tk.Theme.space.md
            rowGap: Tk.Theme.space.xs
            alignItems: Tk.Grid.Center

            Tk.Caption { text: qsTr("ssh destination") }
            Tk.TextField {
                id: sshField
                objectName: "sshHostField"
                mono: true
                placeholderText: "qinda"
                tooltip: qsTr("An ssh host with key login (BatchMode); “local” for this machine; empty turns host telemetry off")
            }
            Tk.Caption { text: qsTr("systemd unit") }
            Tk.TextField {
                id: unitField
                mono: true
                placeholderText: "qinda-llm.service"
                tooltip: qsTr("The llama.cpp service whose journal holds request timings")
            }
            Item { implicitWidth: 1; implicitHeight: 1 }
            Tk.CheckBox {
                id: sudoBox
                text: qsTr("Read the journal with sudo -n")
                tooltip: qsTr("Only needed when the ssh user is not in the host's systemd-journal group")
            }
        }

        Tk.Caption {
            text: qsTr("The sampler runs a small read-only shell loop on the host over one ssh session; it reads /proc and /sys and writes nothing.")
            wrapMode: Text.WordWrap
            Tk.Flex.alignSelf: Tk.Flex.Stretch
        }
    }
}
