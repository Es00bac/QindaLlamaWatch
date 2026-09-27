// SPDX-License-Identifier: GPL-3.0-or-later
//
// Raises a desktop notification (org.freedesktop.Notifications, served by
// the QindaQt notification host) when a model instance crashes, so the
// crash is seen while the window is hidden or on another workspace.
//
// Ownership/threading: GUI thread. The D-Bus call is asynchronous and its
// result is ignored: a missing notification server must never stall or
// break the monitor; the in-window banner still shows the crash.

#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QVariantMap>

namespace QindaLlamaWatch::App {

class CrashNotifier final : public QObject
{
    Q_OBJECT

public:
    explicit CrashNotifier(QDBusConnection connection, QObject *parent = nullptr);

    // `crash` is a ServerJournal::crashDetected() map; `host` names the
    // server machine in the text ("" leaves it out).
    void notify(const QVariantMap &crash, const QString &host);

private:
    QDBusConnection m_connection;
};

} // namespace QindaLlamaWatch::App
