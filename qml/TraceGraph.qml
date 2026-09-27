// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk

// A Tk.Graph with a grid, a caption for the current top of scale and an
// optional corner label. Mirrors the QindaQt System Monitor's TraceGraph.
Item {
    id: trace

    property alias graph: plot
    property alias capacity: plot.capacity
    property alias maxValue: plot.maxValue
    property alias autoScale: plot.autoScale
    property alias autoScaleFloor: plot.autoScaleFloor
    // AGENT-GUARD: must stay the default property, or GraphSeries children
    // land in Item.data and the graph draws nothing.
    default property alias series: plot.series
    // Formats the top-of-scale caption; receives plot.effectiveMax.
    property var scaleFormatter: function (v) { return Math.round(v) + "%" }
    property string cornerText: ""

    implicitWidth: Tk.Theme.size.panelMinWidth
    implicitHeight: Tk.Theme.size.graphMinHeight * 2

    Tk.Graph {
        id: plot
        anchors.fill: parent
        capacity: 180
        maxValue: 100
        gridRows: 3
        gridColor: Tk.Theme.color.divider
        baselineColor: Tk.Theme.color.border
    }

    Tk.Caption {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 2
        text: trace.scaleFormatter(plot.effectiveMax)
        color: Tk.Theme.color.textDisabled
    }
    Tk.Caption {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 2
        visible: trace.cornerText.length > 0
        text: trace.cornerText
        color: Tk.Theme.color.textDisabled
    }
}
