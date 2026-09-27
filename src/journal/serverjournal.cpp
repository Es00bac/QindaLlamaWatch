// SPDX-License-Identifier: GPL-3.0-or-later

#include "journal/serverjournal.h"

#include <QProcess>

namespace QindaLlamaWatch {

namespace {
constexpr qsizetype kMaxRequests = 200;
constexpr qsizetype kMaxLogLines = 400;
// Journal history replayed on connect: enough for the recent-request table
// and the model's load facts after a long uptime, as in qinda-llm-top.
constexpr int kBacklogLines = 1500;
// AGENT-CONTRACT: matches the lines JournalParser takes ports and load facts
// from (journalparser.cpp); widen both together.
constexpr const char *kHistoryPattern =
    "spawning server instance with name=|layers to GPU|model buffer size|llama_kv_cache: .*size"
    "|n_cpu_moe|flash_attn|graph splits";
constexpr const char *kFollowMarker = "@@follow";

QVariantMap toMap(const CompletedRequest &r)
{
    return {
        {QStringLiteral("when"), r.when},
        {QStringLiteral("model"), r.model},
        {QStringLiteral("promptTokens"), r.promptTokens},
        {QStringLiteral("promptTps"), r.promptTps},
        {QStringLiteral("promptSeconds"), r.promptMs / 1000.0},
        {QStringLiteral("genTokens"), r.genTokens},
        {QStringLiteral("genTps"), r.genTps},
        {QStringLiteral("genSeconds"), r.genMs / 1000.0},
    };
}
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
    // instance with name=<model> on port <port>" once, when the model loads;
    // on a server up for hours that line is far outside the replayed
    // backlog, and without it no "[port]" tag resolves to a model name. So
    // first the whole boot is filtered on the host (journalctl --grep) for
    // just the spawn and load-fact lines, then the marker, then the usual
    // backlog and follow. takeLines() feeds phase one to the parser only.
    const QString script = journalctl + QStringLiteral(" -g ") + shellQuote(QLatin1String(kHistoryPattern))
        + QStringLiteral("; echo ") + QLatin1String(kFollowMarker)
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

QVariantMap ServerJournal::facts() const
{
    QVariantMap map;
    const QHash<QString, QString> &facts = m_parser.facts();
    for (auto it = facts.cbegin(); it != facts.cend(); ++it) {
        map.insert(it.key(), it.value());
    }
    return map;
}

void ServerJournal::clear()
{
    m_parser.reset();
    m_following = false;
    m_requests.clear();
    m_log.clear();
    Q_EMIT requestsChanged();
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
    bool requestsAdded = false;
    const QHash<QString, QString> factsBefore = m_parser.facts();   // at most 7 entries
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("-- "))) {
            continue;                         // "-- No entries --", "-- Boot ... --"
        }
        if (!m_following) {
            // Phase one (see configure): history for the parser only; it
            // holds no timing lines, and the log view shows the real tail.
            if (line == QLatin1String(kFollowMarker)) {
                m_following = true;
            } else {
                static_cast<void>(m_parser.feed(line));
            }
            continue;
        }
        m_log.append(line);
        if (std::optional<CompletedRequest> request = m_parser.feed(line)) {
            m_requests.prepend(toMap(*request));
            requestsAdded = true;
        }
    }
    if (m_log.size() > kMaxLogLines) {
        m_log.remove(0, m_log.size() - kMaxLogLines);
    }
    if (m_requests.size() > kMaxRequests) {
        m_requests.resize(kMaxRequests);
    }
    Q_EMIT logChanged();
    if (requestsAdded) {
        Q_EMIT requestsChanged();
    }
    // Facts change rarely (a model load); do not re-emit a map per batch.
    if (m_parser.facts() != factsBefore) {
        Q_EMIT factsChanged();
    }
}

} // namespace QindaLlamaWatch
