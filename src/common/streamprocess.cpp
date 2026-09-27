// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/streamprocess.h"

#include <algorithm>

namespace QindaLlamaWatch {

namespace {
constexpr int kFirstRetryMs = 2000;
constexpr int kMaxRetryMs = 30000;
constexpr qsizetype kStderrLines = 4;
} // namespace

StreamProcess::StreamProcess(QObject *parent)
    : QObject(parent)
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &StreamProcess::launch);
}

StreamProcess::~StreamProcess()
{
    // AGENT-GUARD: owners hold this as a member and forward stateChanged;
    // emitting it from here would call into an owner mid-destruction.
    blockSignals(true);
    stop();
}

void StreamProcess::start(const RemoteCommand &command, const QByteArray &stdinData)
{
    stop();
    m_command = command;
    m_stdin = stdinData;
    m_backoffMs = 0;
    if (m_command.program.isEmpty()) {
        return;
    }
    launch();
}

void StreamProcess::stop()
{
    m_retry.stop();
    if (m_process != nullptr) {
        // AGENT-GUARD: disconnect before killing, or finished() schedules a
        // restart of the command this call is retiring.
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(500);
        delete m_process;
        m_process = nullptr;
    }
    m_partial.clear();
    setState(State::Stopped);
}

void StreamProcess::launch()
{
    m_partial.clear();
    m_stderr.clear();
    m_sawOutput = false;
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &StreamProcess::readStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &StreamProcess::readStderr);
    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus) { finished(exitCode); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_stderr.append(tr("cannot run %1: %2").arg(m_command.program, m_process->errorString()));
            finished(-1);
        }
    });
    setState(State::Starting);
    m_process->start(m_command.program, m_command.arguments);
    if (!m_stdin.isEmpty()) {
        m_process->write(m_stdin);
    }
    m_process->closeWriteChannel();
}

void StreamProcess::readStdout()
{
    m_partial.append(m_process->readAllStandardOutput());
    const qsizetype lastNewline = m_partial.lastIndexOf('\n');
    if (lastNewline < 0) {
        return;
    }
    const QByteArray complete = m_partial.left(lastNewline);
    m_partial.remove(0, lastNewline + 1);
    QStringList lines = QString::fromUtf8(complete).split(QLatin1Char('\n'));
    if (!m_sawOutput) {
        m_sawOutput = true;
        m_backoffMs = 0;
        m_lastError.clear();
        setState(State::Running);
    }
    Q_EMIT linesReceived(lines);
}

void StreamProcess::readStderr()
{
    const QStringList lines =
        QString::fromUtf8(m_process->readAllStandardError()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        m_stderr.append(line.trimmed());
        Q_EMIT errorLineReceived(line.trimmed());
    }
    while (m_stderr.size() > kStderrLines) {
        m_stderr.removeFirst();
    }
}

void StreamProcess::finished(int exitCode)
{
    if (m_process == nullptr) {
        return;
    }
    readStderr();
    m_process->deleteLater();
    m_process = nullptr;
    m_lastError = m_stderr.isEmpty() ? tr("stream ended (exit %1)").arg(exitCode)
                                     : m_stderr.join(QStringLiteral(" · "));
    m_backoffMs = m_backoffMs == 0 ? kFirstRetryMs : std::min(m_backoffMs * 2, kMaxRetryMs);
    setState(State::Waiting);
    Q_EMIT ended(exitCode, m_lastError);
    m_retry.start(m_backoffMs);
}

void StreamProcess::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    Q_EMIT stateChanged();
}

} // namespace QindaLlamaWatch
