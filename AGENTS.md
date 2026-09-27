# QindaLlamaWatch — Agent Instructions

A standalone QindaQt desktop app (C++20 + QML on QindaTK) that monitors a
llama.cpp server on another machine. Read `README.md` for what each panel
shows and where its data comes from.

## Rules

1. **The watched host is busy serving a model.** `src/host/sampler.sh` runs
   there every interval: keep its loop body to shell builtins plus the few
   forks it has (cat, pidof, sleep). Never add a per-sample `ps`, `find`,
   `awk`, `python`, or anything that writes on the host.
2. **Unavailable is not zero.** A reading the host or server did not report
   is absent from the published maps and renders as a dash (`Format.dash`).
3. **Cross-boundary contracts are marked** `AGENT-CONTRACT:` — the sampler
   frame format (sampler.sh ↔ HostSampleParser ↔ tst_hostsampleparser), the
   QVariantMap keys each collector publishes ↔ the QML panel that reads
   them, the action ids (action_catalog.cpp ↔ Main.qml). Change both sides
   and the test in one commit.
4. **Remote arguments are always shell-quoted** (`remoteCommand()`); a
   settings value must never reach the remote shell unquoted.
5. **Panels receive collectors as properties** (`client`, `probe`,
   `journal`), never by context-property name, so `preview/*.qml` can drive
   them with fakes. Only `Main.qml` touches the context properties.
6. QindaTK rules apply to the QML: `import QindaTK as Tk`, theme roles only
   (no literal colours or sizes outside `preview/`), Flex/Grid layout.
7. Files stay under 500 non-blank lines; SPDX header in every file; use
   `AGENT-NOTE:` / `AGENT-GUARD:` / `AGENT-CONTRACT:` markers as in
   container-wm.

## Verify

```
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build
qtk-preview preview/DashboardPreview.qml --grab dash.png --size 1180x860 --wait 4000
```

Look at the PNG before calling visual work done. Never reduce build
parallelism (the user's MAKEOPTS apply).
