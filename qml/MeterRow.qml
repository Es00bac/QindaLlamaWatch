// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// One labelled bar: name, meter, figure, then free-form detail text. Same
// shape as the QindaQt System Monitor's MeterRow, so the two apps read alike;
// the figure takes the meter's own ramp colour so the two never disagree.
Tk.Flex {
    id: row

    property string label: ""
    property string value: ""
    property string detail: ""
    property real amount: 0
    property real from: 0
    property real to: 100
    // False while the reading is unavailable: the bar stays empty and the
    // figure shows a dash instead of pretending to be 0.
    property bool known: true
    property var ramp: null
    property int segments: 0
    property real labelWidth: 44
    property real meterWidth: 180
    property real valueWidth: 64

    align: Tk.Flex.Center
    gap: Tk.Theme.space.sm
    implicitHeight: Tk.Theme.size.row

    Tk.Mono {
        text: row.label
        width: row.labelWidth
        elide: Text.ElideRight
        color: Tk.Theme.color.textMuted
        Tk.Flex.shrink: 0
    }
    Tk.Meter {
        id: bar
        Tk.Flex.basis: row.meterWidth
        Tk.Flex.shrink: 1
        Tk.Flex.minWidth: 40
        implicitHeight: Tk.Theme.size.meter
        value: row.known ? row.amount : row.from
        from: row.from
        to: row.to
        ramp: row.ramp
        segments: row.segments
        radius: Tk.Theme.radius.xs
        tooltip: row.label + " " + row.value
    }
    Tk.Mono {
        text: row.value
        width: row.valueWidth
        horizontalAlignment: Text.AlignRight
        color: row.known && row.ramp !== null ? bar.valueColor : Tk.Theme.color.text
        Tk.Flex.shrink: 0
    }
    Tk.Mono {
        text: row.detail
        visible: row.detail.length > 0
        elide: Text.ElideRight
        color: Tk.Theme.color.textMuted
        Tk.Flex.grow: 1
        Tk.Flex.shrink: 1
        Tk.Flex.minWidth: 0
    }
}
