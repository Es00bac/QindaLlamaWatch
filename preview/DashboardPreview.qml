// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QindaTK as Tk
import "../qml"

// qtk-preview harness: the real dashboard views driven by fake collectors
// with the same property shapes as LlamaClient, HostProbe and ServerJournal
// (see the AGENT-CONTRACT comments in src/). Needs no build:
//   qtk-preview preview/DashboardPreview.qml --grab out.png --size 1180x860
// Demo data only; literals are allowed here (QindaTK rule 2 exempts demos).
Rectangle {
    id: root
    width: 1180
    height: 860
    color: Tk.Theme.color.bg

    property int tick: 0

    QtObject {
        id: fakeClient
        signal sampled()
        property string state: "online"
        property string error: ""
        property string baseUrl: "http://qinda:8100"
        property bool routerMode: true
        property string activeModel: "ornith"
        property string notice: ""
        property var models: [
            { id: "ornith", status: "loaded" },
            { id: "qwen3-coder", status: "unloaded" }
        ]
        property var server: ({
            modelPath: "/var/lib/qinda-llm/models/Ornith-1.5-35B-A3B-Q4_K_M.gguf",
            ctx: 131072, totalSlots: 2, build: "b6512-8f2e4c1",
            sampling: [ { label: "temp", value: "0.6" }, { label: "top-p", value: "0.95" },
                        { label: "top-k", value: "20" }, { label: "min-p", value: "0" } ]
        })
        property var slotList: [
            { id: 0, busy: true, phase: "generate", promptTokens: 18342, promptProgress: 18342,
              decoded: 412 + root.tick * 48, ctx: 65536, used: 18754 + root.tick * 48 },
            { id: 1, busy: root.tick % 6 < 3, phase: root.tick % 6 < 3 ? "prompt" : "idle",
              promptTokens: 9120, promptProgress: 2048 + (root.tick % 3) * 2400, decoded: 0,
              ctx: 65536, used: 9120 }
        ]
        property var throughput: ({
            generateLive: 44 + 6 * Math.sin(root.tick / 3), promptLive: root.tick % 6 < 3 ? 1180 + 90 * Math.cos(root.tick) : 0,
            generateAvg: 41.7, promptAvg: 1096, promptTotal: 4821933, cachedTotal: 3312040,
            generatedTotal: 211874, processing: 1 + (root.tick % 6 < 3 ? 1 : 0), deferred: 0, kvUsage: 0.31
        })
        function loadModel(id) {}
        function unloadModel(id) {}
        function loadNextModel() {}
        function refresh() {}
    }

    QtObject {
        id: fakeProbe
        signal sampled()
        property string state: "live"
        property string error: ""
        property string host: "qinda"
        property var reading: {
            var cores = []
            for (var i = 0; i < 24; ++i)
                cores.push(Math.max(0, Math.min(100, 30 + 60 * Math.sin(i * 0.7 + root.tick / 2))))
            return {
                host: "qinda", cpu: 38 + 10 * Math.sin(root.tick / 4), cores: cores,
                load: "9.12 8.40 7.95", mhz: 4312, cpuTemp: 71,
                memTotal: 64 * 1073741824, memUsed: 21.4 * 1073741824, memCached: 30.2 * 1073741824,
                memFree: 9.1 * 1073741824, swapUsed: 0,
                gpuName: "AMD Radeon RX 7900 XTX", gpuBusy: 88 + 8 * Math.sin(root.tick / 2),
                vramUsed: 22.6 * 1073741824, vramTotal: 24 * 1073741824, gttUsed: 1.3 * 1073741824,
                sclk: "2480Mhz", mclk: "1249Mhz", gpuTemp: 74, gpuHotspot: 91, gpuWatts: 318, gpuFanRpm: 1870,
                llamaProcesses: 2, llamaRss: 3.8 * 1073741824, llamaCpu: 11.5,
                netIf: "tailscale0", netRx: 18400, netTx: 96100, uptime: 412345
            }
        }
    }

    QtObject {
        id: fakeJournal
        signal sampled()
        property string state: "live"
        property string error: ""
        property string unit: "qinda-llm.service"
        property int requestTotal: 4
        property int crashTotal: 3
        property var crashes: [
            { when: "17:28:17", model: "ornith", status: 1, reason: "vk::Queue::submit: ErrorDeviceLost" },
            { when: "17:08:21", model: "ornith", status: 1, reason: "vk::Queue::submit: ErrorDeviceLost" },
            { when: "16:49:35", model: "ornith", status: 1, reason: "vk::Queue::submit: ErrorDeviceLost" }
        ]
        property var facts: ({ layers: "41/41", gpu_buf: "19876.2", cpu_buf: "412.5", kv: "2048", fa: "on" })
        property var requests: [
            { when: "15:54:10", model: "ornith", promptTokens: 18342, promptTps: 1210.4, promptSeconds: 15.2, genTokens: 1024, genTps: 43.9, genSeconds: 23.3 },
            { when: "15:51:37", model: "ornith", promptTokens: 2210, promptTps: 988.1, promptSeconds: 2.2, genTokens: 356, genTps: 45.2, genSeconds: 7.9 },
            { when: "15:49:02", model: "ornith", promptTokens: 40931, promptTps: 1034.9, promptSeconds: 39.6, genTokens: 2048, genTps: 38.1, genSeconds: 53.8 },
            { when: "15:40:44", model: "qwen3-coder", promptTokens: 812, promptTps: 1460.0, promptSeconds: 0.6, genTokens: 96, genTps: 61.0, genSeconds: 1.6 }
        ]
        property var logLines: [
            "2026-09-27T15:54:10-0500 qinda llama-server[812]: [8101] prompt eval time =  15153.10 ms / 18342 tokens (    0.83 ms per token,  1210.44 tokens per second)",
            "2026-09-27T15:54:33-0500 qinda llama-server[812]: [8101]        eval time =  23325.51 ms /  1024 tokens (   22.78 ms per token,    43.90 tokens per second)"
        ]
    }

    Timer {
        interval: 120
        repeat: true
        running: true
        onTriggered: {
            root.tick++
            fakeClient.sampled()
            fakeProbe.sampled()
        }
    }

    Tk.Flex {
        anchors.fill: parent
        direction: Tk.Flex.Column
        gap: 0

        Tk.ToolBar {
            compact: true
            Tk.Badge { dot: true; variant: "success" }
            Tk.Mono { text: fakeClient.baseUrl; color: Tk.Theme.color.textMuted }
            Tk.Label { text: "serving ornith"; accent: true }
            Tk.Spacer {}
            Tk.Segmented { small: true; model: ["Dashboard", "Log"]; currentIndex: 0 }
        }

        CrashBanner {
            objectName: "crashBanner"
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            Tk.Flex.shrink: 0
            journal: fakeJournal
            crash: fakeJournal.crashes[0]
        }

        DashboardView {
            objectName: "dashboard"
            Tk.Flex.grow: 1
            Tk.Flex.basis: 0
            Tk.Flex.alignSelf: Tk.Flex.Stretch
            client: fakeClient
            probe: fakeProbe
            journal: fakeJournal
        }
    }
}
