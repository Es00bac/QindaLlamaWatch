// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import QindaTK as Tk

// The tail of the server's journal (llm-top's `l` view). Follows the end
// unless the reader has scrolled up.
Tk.Panel {
    id: panel

    property var journal
    readonly property var lines: panel.journal ? panel.journal.logLines : []

    title: panel.journal ? qsTr("Log · journalctl -u %1").arg(panel.journal.unit) : qsTr("Log")
    iconName: "scroll-text"
    collapsible: false

    Item {
        implicitHeight: Tk.Theme.size.row * 12

        ListView {
            id: list
            objectName: "logList"
            anchors.fill: parent
            clip: true
            model: panel.lines
            boundsBehavior: Flickable.StopAtBounds
            // AGENT-NOTE: "follow" is decided before the model grows: a reader
            // who scrolled up keeps their place while lines keep arriving.
            property bool follow: true
            onMovementEnded: list.follow = list.atYEnd
            onCountChanged: if (list.follow) list.positionViewAtEnd()

            T.ScrollBar.vertical: Tk.ScrollBar {}

            delegate: Tk.Mono {
                required property string modelData
                width: ListView.view.width
                text: modelData
                elide: Text.ElideRight
                color: /error|failed|fatal/i.test(modelData) ? Tk.Theme.color.danger
                       : /warn/i.test(modelData) ? Tk.Theme.color.warning : Tk.Theme.color.text
            }
        }

        Tk.EmptyState {
            anchors.centerIn: parent
            visible: panel.lines.length === 0
            iconName: "scroll-text"
            title: qsTr("No log lines")
            text: panel.journal && panel.journal.error ? panel.journal.error
                  : qsTr("The server journal appears here once the host is reachable over ssh.")
        }
    }
}
