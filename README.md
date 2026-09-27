# QindaLlamaWatch

A QindaTK desktop monitor for a **llama.cpp server running on another
machine**: the desktop counterpart of `qinda-llm-top` (the curses console
from `sci-ml/qinda-llm-server`), as a standalone app for the QindaQt desktop.
It is separate from the QindaQt System Monitor, which watches the local
machine.

| Panel | Shows | Source |
| --- | --- | --- |
| Model | models and their state (click one to load it), file, context, slots, KV cache, GPU layers / VRAM / RAM placement, sampling defaults, build | `/models`, `/props`, server journal, host sampler |
| Throughput | generate and prompt tokens/s now (with history traces) and over the server's lifetime; token totals; queue; KV-cache use | `/slots` deltas, `/metrics` |
| Slots | per slot: idle / reading prompt n of m / generating n tokens, and context fill | `/slots` |
| System | host CPU (+ per core), RAM (programs vs page cache), GPU busy/clocks/power/temps/fan, VRAM, GTT, the llama-server processes' CPU and memory, network; CPU/GPU and RAM/VRAM history | host sampler over ssh |
| Recent requests | time, model, prompt tokens and read speed, output tokens and generation speed | server journal over ssh |
| Log (Ctrl+2) | the tail of the service journal, following new lines | server journal over ssh |

Server menu (also in the global menu): **Load Next Model** (Ctrl+M), **Unload
Model** (Ctrl+U), **Restart Server** (Ctrl+Shift+R, asks first) — the
`m` / `u` / `r` keys of `qinda-llm-top`.

## How it reaches the server

* **HTTP** — `LlamaClient` polls the llama.cpp API (`/models`, then
  `/props`, `/slots`, `/metrics` for the loaded model) once per interval,
  with the API key from `~/.config/qinda-llm/api-key` (or
  `$QINDA_LLM_API_KEY`) as a Bearer token. Works against a router
  (`--models-preset`, several models) or a plain single-model
  `llama-server`. `/metrics` needs `--metrics`; without it the lifetime
  figures show dashes.
* **ssh** — llama.cpp does not report the machine it runs on, so
  `HostProbe` pipes `src/host/sampler.sh` to `sh -s` on the host over one
  long-lived `ssh -T -o BatchMode=yes` session. The script is a read-only
  loop over `/proc` and `/sys` (shell builtins, a `cat` or two per sample)
  that prints one framed sample per interval. `ServerJournal` follows
  `journalctl -u qinda-llm.service -b -f` over a second session for request
  timings, load facts and the log. Both reconnect with backoff.
* **ssh prerequisites** — key login to the host
  (`ssh qinda true` must work without a prompt). For the journal, the ssh
  user must be in the host's `systemd-journal` group, or enable
  *Read the journal with sudo -n* with a NOPASSWD rule for
  `journalctl`. Restart needs a NOPASSWD rule for
  `systemctl restart qinda-llm.service`. llama-server's own VRAM/GTT
  (DRM fdinfo) is readable only as the service user or root; without it the
  Model panel shows RAM residency and the System panel device-wide VRAM.

Defaults: server `http://qinda:8100`, ssh host `qinda`, unit
`qinda-llm.service`, 1 s refresh. Change them in **File → Settings**
(saved to `~/.config/QindaQt/qindallamawatch.conf`), or per run with
`--server URL` and `--ssh HOST` (`--ssh local` samples this machine,
`--ssh ""` turns host telemetry off).

## Build

Requirements: Qt 6.11 (Core, Gui, Network, Qml, Quick, QuickControls2,
DBus, Test), CMake 3.24+, `gui-wm/qindaqt-desktop` (AppShell and global
menu archives), `dev-libs/qindatk` at run time.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
build/qindallamawatch
```

Portage: `packaging/gentoo/gui-apps/qindallamawatch/` (git-r3 from the
qinda hub; pin `EGIT_COMMIT` for a release).

## Verifying the UI without a build

The panels take their collectors as properties, so `qtk-preview` can drive
the real views with fake data:

```
qtk-preview preview/DashboardPreview.qml --grab dash.png --size 1180x860 --wait 4000
qtk-preview preview/PartsPreview.qml --grab parts.png --size 1000x640
```

`--ssh local` runs the sampler on this machine; to see its raw frames:
`sh -s -- 1 < src/host/sampler.sh`.

## Layout

```
src/common/    RemoteCommand (ssh argv + quoting), StreamProcess (supervised line stream)
src/host/      sampler.sh, HostSampleParser (frames), HostStats (rates), HostProbe (QML face)
src/journal/   JournalParser (timings, load facts), ServerJournal (QML face, restart)
src/llama/     metrics parser, SlotRates (live t/s), LlamaClient (QML face)
src/settings/  MonitorSettings (QSettings)
src/app/       the AppShell action catalog
qml/           Main (Tk.AppWindow) and one file per panel
preview/       qtk-preview harnesses with fake collectors
tests/         QtTest: sampler contract + rates, journal, metrics/slots/quoting
```
