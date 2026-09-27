// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/crashnotifier.h"

#include <QDBusMessage>
#include <QStringList>

#include <utility>

namespace QindaLlamaWatch::App {

CrashNotifier::CrashNotifier(QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
{
}

void CrashNotifier::notify(const QVariantMap &crash, const QString &host)
{
    const QString model = crash.value(QStringLiteral("model")).toString();
    const QString summary = host.isEmpty()
        ? tr("%1 crashed").arg(model)
        : tr("%1 crashed on %2").arg(model, host);
    const QString body = tr("%1 · %2\nThe router restarts the model; its cached prompt is lost, so "
                            "the next request re-reads the whole conversation.")
                             .arg(crash.value(QStringLiteral("when")).toString(),
                                  crash.value(QStringLiteral("reason")).toString());

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));
    // AGENT-CONTRACT: freedesktop Notify(app_name, replaces_id, app_icon,
    // summary, body, actions, hints, expire_timeout). "urgency" must be a
    // byte (D-Bus 'y'); 2 = critical, which stays until dismissed.
    message << QStringLiteral("QindaLlamaWatch")
            << quint32(0)
            << QStringLiteral("org.qindaqt.LlamaWatch")
            << summary
            << body
            << QStringList{}
            << QVariantMap{{QStringLiteral("urgency"), QVariant::fromValue<uchar>(2)},
                           {QStringLiteral("desktop-entry"), QStringLiteral("org.qindaqt.LlamaWatch")}}
            << qint32(-1);
    static_cast<void>(m_connection.send(message));
}

} // namespace QindaLlamaWatch::App
