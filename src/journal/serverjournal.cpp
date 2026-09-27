// SPDX-License-Identifier: GPL-3.0-or-later

#include "journal/serverjournal.h"

#include <QProcess>

namespace QindaLlamaWatch {

namespace {
// Journal history replayed on connect: the log view's tail and anything the
// history filter below does not match, as in qinda-llm-top.
constexpr int kBacklogLines = 1500;
// AGENT-CONTRACT: matches every line JournalParser takes something from
// (journalparser.cpp): model spawns, load facts and request timings. Widen
// both together; a line the filter drops is invisible once it scrolls out
// of the backlog.
constexpr const char *kHistoryPattern =
    "spawning server instance with name=|eval time =|layers to GPU|model buffer size"
    "|llama_kv_cache: .*size|n_cpu_moe|flash_attn|graph splits"
    "|exited with status|terminate called after throwing|what\\(\\):";
} // namespace

ServerJournal::ServerJournal(QObject *parent)
    : QObject(parent)
{
    connect(&m_stream, &StreamProcess::linesReceived, this, &ServerJournal::takeLines);
    connect(&m_stream, &StreamProcess::errorLineReceived, this, &ServerJournal::takeError);
    connect(&m_stream, &StreamProcess::stateChanged, this, &ServerJournal::stateChanged);
    // AGENT-GUARD: every (re)connect replays the backlog, so the previous
    // session's rows must go or each reconnect duplicates the table.
    connect(&m_stream, &StreamProcess::ended, this, &ServerJournal::clear);
}

void ServerJournal::configure(const QString &host, const QString &unit, bool useSudo)
{
    m_host = host.trimmed();
    m_unit = unit.trimmed().isEmpty() ? QStringLiteral("qinda-llm.service") : unit.trimmed();
    // A permission verdict outlives reconnects (sudo -n fails the same way
    // every retry) and is only re-evaluated when the settings change.
    m_denied.clear();
    clear();

    // -n: fail at once instead of waiting on a password prompt.
    const QString journalctl = (useSudo ? QStringLiteral("sudo -n journalctl") : QStringLiteral("journalctl"))
        + QStringLiteral(" -u ") + shellQuote(m_unit) + QStringLiteral(" -b -o short-iso --no-pager");
    // AGENT-NOTE: two phases in one session. The router logs "spawning server
    // instance with name=<model> on port <port>" once, when the model loads,
    // and a long prompt read logs hundreds of progress lines; on a busy
    // server both the spawn line and older request timings are far outside
    // the replayed backlog. So first the whole boot is filtered on the host
    // (journalctl --grep) for just the lines the parser reads, then the
    // marker, then the usual backlog and follow. JournalFeed splits them.
    const QString script = journalctl + QStringLiteral(" -g ") + shellQuote(QLatin1String(kHistoryPattern))
        + QStringLiteral("; echo ") + QLatin1String(JournalFeed::kFollowMarker)
        + QStringLiteral("; exec ") + journalctl + QStringLiteral(" -n ") + QString::number(kBacklogLines)
        + QStringLiteral(" -f");
    m_stream.start(remoteCommand(m_host, {QStringLiteral("sh"), QStringLiteral("-c"), script}));
    Q_EMIT stateChanged();
}

void ServerJournal::restartService()
{
    const RemoteCommand command = remoteCommand(
        m_host, {QStringLiteral("sudo"), QStringLiteral("-n"), QStringLiteral("systemctl"),
                 QStringLiteral("restart"), QStringLiteral("--no-block"), m_unit});
    if (command.program.isEmpty()) {
        Q_EMIT serviceActionFinished(false, tr("no server host is configured"));
        return;
    }
    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        const bool ok = status == QProcess::NormalExit && exitCode == 0;
        const QString output = QString::fromUtf8(process->readAll()).trimmed();
        Q_EMIT serviceActionFinished(ok, ok ? tr("restarting %1").arg(m_unit)
                                            : tr("restart failed: %1").arg(output.isEmpty()
                                                  ? tr("exit %1").arg(exitCode) : output));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            Q_EMIT serviceActionFinished(false, process->errorString());
        }
    });
    process->start(command.program, command.arguments);
}

QString ServerJournal::state() const
{
    if (m_host.isEmpty()) {
        return QStringLiteral("off");
    }
    if (!m_denied.isEmpty()) {
        return QStringLiteral("denied");
    }
    switch (m_stream.state()) {
    case StreamProcess::State::Running:
        return QStringLiteral("live");
    case StreamProcess::State::Waiting:
        return QStringLiteral("retrying");
    case StreamProcess::State::Starting:
    case StreamProcess::State::Stopped:
        break;
    }
    return QStringLiteral("connecting");
}

QString ServerJournal::error() const
{
    return m_denied.isEmpty() ? m_stream.lastError() : m_denied;
}

void ServerJournal::clear()
{
    m_feed.reset();
    Q_EMIT requestsChanged();
    Q_EMIT crashesChanged();
    Q_EMIT factsChanged();
    Q_EMIT logChanged();
    Q_EMIT stateChanged();
}

void ServerJournal::takeError(const QString &line)
{
    // AGENT-NOTE: journalctl run by a user outside systemd-journal/wheel
    // keeps running but sees only that user's own messages, reporting it on
    // stderr; sudo -n without a NOPASSWD rule prints "a password is required".
    if (line.contains(QLatin1String("insufficient permissions"))
        || line.contains(QLatin1String("not seeing messages from other users"))
        || line.contains(QLatin1String("password is required"))) {
        m_denied = tr("the journal of %1 is not readable as the ssh user: %2").arg(m_unit, line);
        Q_EMIT stateChanged();
    }
}

void ServerJournal::takeLines(const QStringList &lines)
{
    const JournalFeed::Changes changes = m_feed.take(lines);
    if (changes.log) {
        Q_EMIT logChanged();
    }
    if (changes.requests) {
        Q_EMIT requestsChanged();
    }
    if (changes.facts) {
        Q_EMIT factsChanged();
    }
    if (changes.crashes) {
        Q_EMIT crashesChanged();
    }
    for (const QVariant &crash : changes.newCrashes) {
        Q_EMIT crashDetected(crash.toMap());
    }
}

} // namespace QindaLlamaWatch
