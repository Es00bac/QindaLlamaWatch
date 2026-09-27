// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "Format.js" as Format

// The in-window crash alert: shown when a model instance dies while the
// monitor is following the journal, and kept until dismissed, so a crash
// that happened while nobody watched is still there when someone looks.
// Crashes found in the boot history on connect do not raise it; they are
// counted in the Model panel and the status bar instead.
Tk.Notice {
    id: banner

    property var journal
    // The newest live crash ({when, model, status, reason}), or null.
    property var crash: null

    signal showLogRequested()

    visible: banner.crash !== null
    variant: "danger"
    dismissible: true
    title: banner.crash
           ? qsTr("%1 crashed at %2").arg(banner.crash.model).arg(banner.crash.when)
           : ""
    text: banner.crash
          ? qsTr("%1. The router restarted it, and its cached prompt is gone: the next request re-reads "
                 + "the whole conversation. %2 this boot.").arg(banner.crash.reason)
                .arg(Format.plural(banner.journal ? banner.journal.crashTotal : 0, qsTr("crash"), qsTr("crashes")))
          : ""

    onDismissed: banner.crash = null

    actions: Tk.Button {
        small: true
        variant: "ghost"
        iconName: "scroll-text"
        text: qsTr("Show log")
        onClicked: banner.showLogRequested()
    }

    Connections {
        target: banner.journal
        ignoreUnknownSignals: true
        function onCrashDetected(crash) { banner.crash = crash }
    }
}
