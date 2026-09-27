// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/action_catalog.h"

namespace QindaLlamaWatch::App {

namespace {

using QindaQt::AppShell::ActionSpec;

ActionSpec action(const char *id, const char *menuId, const char *menuLabel, int menuOrder,
                  const char *label, const char *shortcut, int order,
                  bool checkable = false, bool destructive = false)
{
    ActionSpec spec;
    // AGENT-GUARD: labels contain non-ASCII ("Settings…"); fromLatin1 would
    // double-encode the UTF-8 source bytes on the exported dbusmenu.
    spec.id = QString::fromUtf8(id);
    spec.menuId = QString::fromUtf8(menuId);
    spec.menuLabel = QString::fromUtf8(menuLabel);
    spec.menuOrder = menuOrder;
    spec.label = QString::fromUtf8(label);
    spec.shortcut = QKeySequence(QString::fromUtf8(shortcut));
    spec.order = order;
    spec.checkable = checkable;
    spec.checked = false;
    spec.destructive = destructive;
    return spec;
}

} // namespace

QList<ActionSpec> buildMonitorActions()
{
    return {
        action("file.settings", "file", "File", 0, "Settings…", "Ctrl+,", 0),
        action("file.quit", "file", "File", 0, "Quit", "Ctrl+Q", 1),

        action("view.dashboard", "view", "View", 10, "Dashboard", "Ctrl+1", 0),
        action("view.log", "view", "View", 10, "Server Log", "Ctrl+2", 1),
        action("view.refresh", "view", "View", 10, "Refresh Now", "F5", 2),

        // The llm-top keys m / u / r, with modifiers so typing in a field
        // never loads a model.
        action("server.load-next", "server", "Server", 20, "Load Next Model", "Ctrl+M", 0),
        action("server.unload", "server", "Server", 20, "Unload Model", "Ctrl+U", 1),
        action("server.restart", "server", "Server", 20, "Restart Server", "Ctrl+Shift+R", 2,
               false, true),

        action("help.about", "help", "Help", 30, "About QindaLlamaWatch", "F1", 0),
    };
}

} // namespace QindaLlamaWatch::App
