// SPDX-License-Identifier: GPL-3.0-or-later
//
// How a command reaches the server host: through `ssh` for a remote host, or
// directly for "local". Pure value code; nothing here starts a process.

#pragma once

#include <QString>
#include <QStringList>

namespace QindaLlamaWatch {

struct RemoteCommand final {
    QString program;
    QStringList arguments;
};

// `host` is an ssh destination (an alias from ~/.ssh/config, user@host, ...)
// or "local"/"localhost" to run on this machine. An empty host yields an
// empty program: the caller treats that as "host telemetry is off".
//
// AGENT-GUARD: ssh joins its trailing arguments into one string that the
// remote login shell parses again, so every remote argument is single-quoted
// here. Passing an unquoted journal unit or path lets a settings value run
// arbitrary shell on the server.
[[nodiscard]] RemoteCommand remoteCommand(const QString &host, const QStringList &remoteArgv);

[[nodiscard]] bool isLocalHost(const QString &host);

// POSIX single-quote quoting: abc -> 'abc', it's -> 'it'\''s'.
[[nodiscard]] QString shellQuote(const QString &argument);

} // namespace QindaLlamaWatch
