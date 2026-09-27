// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "../qml"

// qtk-preview harness for the log page and the settings dialog:
//   qtk-preview preview/PartsPreview.qml --grab out.png --size 1000x640
// Demo data only.
Rectangle {
    width: 1000
    height: 640
    color: Tk.Theme.color.bg

    QtObject {
        id: fakeJournal
        property string state: "denied"
        property string error: "the journal of qinda-llm.service is not readable as the ssh user: Hint: You are currently not seeing messages from other users and the system."
        property string unit: "qinda-llm.service"
        property var facts: ({})
        property var requests: []
        property int requestTotal: 0
        property int crashTotal: 0
        property var crashes: []
        property var logLines: {
            var out = []
            for (var i = 0; i < 40; ++i)
                out.push("2026-09-27T15:" + (10 + i) + ":02-0500 qinda llama-server[812]: [8101] slot update_slots: id  0 | task " + (900 + i) + " | prompt processing progress, n_past = " + (2048 * i))
            out.push("2026-09-27T15:51:02-0500 qinda llama-server[812]: warning: failed to munlock buffer")
            return out
        }
    }

    QtObject {
        id: fakeSettings
        property string serverUrl: "http://qinda:8100"
        property string sshHost: "qinda"
        property string apiKeyFile: "~/.config/qinda-llm/api-key"
        property int intervalMs: 1000
        property string journalUnit: "qinda-llm.service"
        property bool journalSudo: false
        property bool apiKeyFound: true
        function apply(url, host, key, interval, unit, sudo) {}
    }

    Tk.Flex {
        anchors.fill: parent
        padding: Tk.Theme.space.sm
        gap: Tk.Theme.space.sm

        LogView {
            objectName: "logView"
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            journal: fakeJournal
        }
        RequestsPanel {
            objectName: "requestsDenied"
            Tk.Flex.basis: 420
            Tk.Flex.alignSelf: Tk.Flex.Start
            journal: fakeJournal
        }
    }

    SettingsDialog {
        id: settings
        settings: fakeSettings
        Component.onCompleted: settings.open()
    }
}
