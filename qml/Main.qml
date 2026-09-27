// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import QindaTK.QindaQt

// The QindaLlamaWatch window: QindaTK chrome around the dashboard.
//
// Collectors are context properties set in main.cpp: `llama` (HTTP API),
// `hostProbe` (CPU/GPU/RAM/VRAM over ssh), `serverJournal` (request timings
// and log over ssh), `monitorSettings`, and `coordinator` (AppShell actions).
// Menu authority is the coordinator's catalog (src/app/action_catalog.cpp):
// the global menu, the in-window MonitorMenuBar, the Shortcut items and the
// toolbar buttons all dispatch through window.dispatch() by the same ids.
Tk.AppWindow {
    id: window

    // Written only by the menu export's fail-closed callback (main.cpp).
    property bool inWindowMenuVisible: true
    // 0 dashboard, 1 server log.
    property int page: 0
    property string actionNote: ""

    width: 1180
    height: 880
    minimumWidth: 640
    minimumHeight: 420
    visible: true
    title: llama.activeModel.length > 0
           ? qsTr("%1 — QindaLlamaWatch").arg(llama.activeModel)
           : qsTr("QindaLlamaWatch")

    QindaQtTheme {}

    function dispatch(actionId) {
        switch (actionId) {
        case "file.settings": settingsDialog.open(); break
        case "file.quit": Qt.quit(); break
        case "view.dashboard": window.page = 0; break
        case "view.log": window.page = 1; break
        case "view.refresh": llama.refresh(); break
        case "server.load-next": llama.loadNextModel(); break
        case "server.unload": llama.unloadModel(llama.activeModel); break
        case "server.restart": restartDialog.open(); break
        case "help.about": aboutDialog.open(); break
        }
    }

    function note(text) {
        window.actionNote = text
        noteTimer.restart()
    }

    Connections {
        target: coordinator
        function onActionRequested(actionId) { window.dispatch(actionId) }
    }
    Connections {
        target: serverJournal
        function onServiceActionFinished(ok, message) { window.note(message) }
    }
    Timer {
        id: noteTimer
        interval: 6000
        onTriggered: window.actionNote = ""
    }

    menuBar: MonitorMenuBar {
        visible: window.inWindowMenuVisible
        menusModel: coordinator.menus
        onActivated: function (actionId) { coordinator.activateAction(actionId) }
    }

    toolBars: [
        Tk.ToolBar {
            compact: true

            Tk.Badge {
                dot: true
                variant: llama.state === "online" ? "success"
                         : llama.state === "offline" ? "danger" : "warning"
            }
            Tk.Mono {
                objectName: "serverUrl"
                text: llama.baseUrl.length > 0 ? llama.baseUrl : qsTr("no server configured")
                color: Tk.Theme.color.textMuted
            }
            Tk.Label {
                objectName: "serverState"
                text: llama.state === "online"
                      ? (llama.activeModel.length > 0 ? qsTr("serving %1").arg(llama.activeModel)
                                                      : qsTr("idle — no model loaded"))
                      : llama.state === "offline" ? qsTr("unreachable: %1").arg(llama.error)
                      : llama.state === "off" ? qsTr("set a server in Settings") : qsTr("connecting…")
                accent: llama.state === "online" && llama.activeModel.length > 0
                Tk.Flex.shrink: 1
                Tk.Flex.minWidth: 0
            }
            Tk.Spacer {}
            Tk.Segmented {
                objectName: "pageSwitch"
                small: true
                model: [
                    { text: qsTr("Dashboard"), iconName: "layout-dashboard", tooltip: qsTr("Dashboard (Ctrl+1)") },
                    { text: qsTr("Log"), iconName: "scroll-text", tooltip: qsTr("Server log (Ctrl+2)") }
                ]
                currentIndex: window.page
                onActivated: function (index) { window.page = index }
            }
            Tk.IconButton {
                iconName: "refresh-cw"
                small: true
                tooltip: qsTr("Refresh now (F5)")
                onClicked: coordinator.activateAction("view.refresh")
            }
            Tk.IconButton {
                iconName: "settings"
                small: true
                tooltip: qsTr("Settings (Ctrl+,)")
                onClicked: coordinator.activateAction("file.settings")
            }
        }
    ]

    statusBar: Tk.StatusBar {
        Tk.StatusField {
            actionable: false
            iconName: "database"
            text: llama.routerMode ? qsTr("router · %n model(s)", "", llama.models.length)
                                   : llama.state === "online" ? qsTr("single model") : llama.state
        }
        Tk.StatusField {
            iconName: "cpu"
            text: hostProbe.state === "off" ? qsTr("host telemetry off")
                                            : qsTr("host %1 · %2").arg(hostProbe.host).arg(hostProbe.state)
            tooltip: hostProbe.error
            onClicked: coordinator.activateAction("file.settings")
        }
        Tk.StatusField {
            iconName: "scroll-text"
            text: qsTr("journal · %1").arg(serverJournal.state)
            tooltip: serverJournal.error
            onClicked: window.page = 1
        }
        Tk.Spacer {}
        Tk.StatusField {
            actionable: false
            visible: text.length > 0
            iconName: "info"
            text: llama.notice.length > 0 ? llama.notice : window.actionNote
        }
    }

    Item {
        DashboardView {
            anchors.fill: parent
            visible: window.page === 0
            client: llama
            probe: hostProbe
            journal: serverJournal
        }
        LogView {
            anchors.fill: parent
            anchors.margins: Tk.Theme.space.sm
            visible: window.page === 1
            journal: serverJournal
        }
    }

    SettingsDialog {
        id: settingsDialog
        settings: monitorSettings
    }

    Tk.Dialog {
        id: restartDialog
        title: qsTr("Restart the server?")
        subtitle: qsTr("systemctl restart %1 on %2").arg(serverJournal.unit).arg(hostProbe.host)
        primaryText: qsTr("Restart")
        secondaryText: qsTr("Cancel")
        destructive: true
        onAccepted: serverJournal.restartService()
        Tk.Label {
            text: qsTr("Requests in flight are dropped and the loaded model is read from disk again. "
                       + "This runs “sudo -n systemctl restart” over ssh and needs a matching NOPASSWD rule on the host.")
            wrapMode: Text.WordWrap
        }
    }

    Tk.Dialog {
        id: aboutDialog
        title: qsTr("QindaLlamaWatch %1").arg(Qt.application.version)
        subtitle: qsTr("Live monitor for a remote llama.cpp server")
        primaryText: qsTr("Close")
        Tk.Label {
            text: qsTr("Model, throughput, slots and request timings from the llama.cpp API and the "
                       + "server's journal; CPU, GPU, RAM and VRAM from the server host over ssh. "
                       + "The desktop counterpart of qinda-llm-top.")
            wrapMode: Text.WordWrap
        }
    }

    // ---- Shortcuts: sole key authority (menus carry display strings).
    // Every one mirrors an action id in src/app/action_catalog.cpp.
    Shortcut { sequence: "Ctrl+,"; onActivated: coordinator.activateAction("file.settings") }
    Shortcut { sequence: "Ctrl+Q"; onActivated: coordinator.activateAction("file.quit") }
    Shortcut { sequence: "Ctrl+1"; onActivated: coordinator.activateAction("view.dashboard") }
    Shortcut { sequence: "Ctrl+2"; onActivated: coordinator.activateAction("view.log") }
    Shortcut { sequence: "F5"; onActivated: coordinator.activateAction("view.refresh") }
    Shortcut { sequence: "Ctrl+M"; onActivated: coordinator.activateAction("server.load-next") }
    Shortcut { sequence: "Ctrl+U"; onActivated: coordinator.activateAction("server.unload") }
    Shortcut { sequence: "Ctrl+Shift+R"; onActivated: coordinator.activateAction("server.restart") }
    Shortcut { sequence: "F1"; onActivated: coordinator.activateAction("help.about") }
}
