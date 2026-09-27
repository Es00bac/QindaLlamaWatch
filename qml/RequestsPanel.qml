// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "Format.js" as Format

// Completed requests with their prompt-read and generation speed, newest
// first, from the server journal (llm-top's RECENT REQUESTS box).
Tk.Panel {
    id: panel

    property var journal
    readonly property string journalState: panel.journal ? panel.journal.state : "off"

    title: qsTr("Recent requests")
    iconName: "list"
    collapsible: false

    actions: Tk.Badge {
        text: panel.journalState === "live"
              ? qsTr("%1 since boot").arg(panel.journal.requestTotal)
              : panel.journalState
        variant: panel.journalState === "live" ? "default"
                 : panel.journalState === "denied" || panel.journalState === "retrying" ? "warning" : "default"
        tooltip: panel.journal && panel.journal.error ? panel.journal.error : ""
    }

    Tk.Flex {
        direction: Tk.Flex.Column
        gap: Tk.Theme.space.xs

        Tk.Notice {
            visible: panel.journalState === "denied" || panel.journalState === "retrying"
            variant: "warning"
            title: panel.journalState === "denied" ? qsTr("Server journal not readable")
                                                    : qsTr("Server journal unavailable")
            text: (panel.journal ? panel.journal.error : "")
                  + (panel.journalState === "denied"
                     ? qsTr(" — add the ssh user to the systemd-journal group on the host, or enable “read the journal with sudo -n” in Settings.")
                     : "")
        }

        Tk.DataTable {
            objectName: "requestsTable"
            tooltip: qsTr("Recent requests")
            implicitHeight: Tk.Theme.size.header + Tk.Theme.size.row * 8
            Tk.Flex.grow: 1
            model: panel.journal ? panel.journal.requests : []
            emptyText: panel.journalState === "off"
                       ? qsTr("Set the server host in Settings to read request timings from its journal")
                       : qsTr("No completed requests since boot")
            columns: [
                Tk.TableColumn { key: "when"; title: qsTr("time"); width: 72; mono: true; sortable: false },
                Tk.TableColumn { key: "model"; title: qsTr("model"); flex: 1; minWidth: 80; sortable: false },
                Tk.TableColumn {
                    key: "promptTokens"; title: qsTr("prompt"); width: 76; mono: true; sortable: false
                    align: Qt.AlignRight; formatter: function (v) { return Format.count(v) }
                },
                Tk.TableColumn {
                    key: "promptTps"; title: qsTr("read t/s"); width: 76; mono: true; sortable: false
                    align: Qt.AlignRight; formatter: function (v) { return Format.number(v, 1) }
                },
                Tk.TableColumn {
                    key: "promptSeconds"; title: qsTr("read"); width: 60; mono: true; sortable: false
                    align: Qt.AlignRight; muted: true; formatter: function (v) { return Format.number(v, 1) + "s" }
                },
                Tk.TableColumn {
                    key: "genTokens"; title: qsTr("output"); width: 68; mono: true; sortable: false
                    align: Qt.AlignRight; formatter: function (v) { return Format.count(v) }
                },
                Tk.TableColumn {
                    key: "genTps"; title: qsTr("gen t/s"); width: 68; mono: true; sortable: false
                    align: Qt.AlignRight; formatter: function (v) { return Format.number(v, 1) }
                },
                Tk.TableColumn {
                    key: "genSeconds"; title: qsTr("gen"); width: 60; mono: true; sortable: false
                    align: Qt.AlignRight; muted: true; formatter: function (v) { return Format.number(v, 1) + "s" }
                }
            ]
        }
    }
}
