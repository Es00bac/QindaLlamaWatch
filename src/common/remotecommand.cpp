// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/remotecommand.h"

namespace QindaLlamaWatch {

bool isLocalHost(const QString &host)
{
    const QString h = host.trimmed().toLower();
    return h == QLatin1String("local") || h == QLatin1String("localhost");
}

QString shellQuote(const QString &argument)
{
    QString quoted = argument;
    quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

RemoteCommand remoteCommand(const QString &host, const QStringList &remoteArgv)
{
    const QString destination = host.trimmed();
    if (destination.isEmpty() || remoteArgv.isEmpty()) {
        return {};
    }
    if (isLocalHost(destination)) {
        return {remoteArgv.first(), remoteArgv.mid(1)};
    }

    QStringList quoted;
    quoted.reserve(remoteArgv.size());
    for (const QString &argument : remoteArgv) {
        quoted.append(shellQuote(argument));
    }
    // AGENT-NOTE: BatchMode makes a missing key or an unknown host fail at
    // once with a readable stderr line instead of waiting on a password
    // prompt nobody can see. The keepalives let a dead tailnet link end the
    // stream so StreamProcess reconnects.
    return {QStringLiteral("ssh"),
            {QStringLiteral("-T"),
             QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
             QStringLiteral("-o"), QStringLiteral("ConnectTimeout=6"),
             QStringLiteral("-o"), QStringLiteral("ServerAliveInterval=5"),
             QStringLiteral("-o"), QStringLiteral("ServerAliveCountMax=3"),
             destination,
             QStringLiteral("--"),
             quoted.join(QLatin1Char(' '))}};
}

} // namespace QindaLlamaWatch
