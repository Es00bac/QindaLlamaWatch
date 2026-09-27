// SPDX-License-Identifier: GPL-3.0-or-later
//
// The complete menu catalog: one list feeds the exported global menu, the
// in-window fallback menu and the shortcut hints in both.
//
// AGENT-CONTRACT: ActionRegistry rejects any entry without a shortcut and has
// no separators. Dispatch lives in qml/Main.qml (onActionRequested) and its
// Shortcut items; keep the ids there in sync with this list.

#pragma once

#include <qindaqt/app_shell/app_shell_types.h>

#include <QList>

namespace QindaLlamaWatch::App {

[[nodiscard]] QList<QindaQt::AppShell::ActionSpec> buildMonitorActions();

} // namespace QindaLlamaWatch::App
