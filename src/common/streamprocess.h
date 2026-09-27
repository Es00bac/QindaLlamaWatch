// SPDX-License-Identifier: GPL-3.0-or-later
//
// A long-running child process read line by line, restarted with backoff
// when it ends. HostProbe (the sampler loop) and ServerJournal (journalctl
// -f) each own one.
//
// Ownership/threading: a GUI-thread QObject that owns its QProcess. stop()
// and the destructor kill the child; with ssh that closes the channel and
// the remote loop dies of SIGPIPE on its next write.

#pragma once

#include "common/remotecommand.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

namespace QindaLlamaWatch {

class StreamProcess final : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Starting, Running, Waiting };
    Q_ENUM(State)

    explicit StreamProcess(QObject *parent = nullptr);
    ~StreamProcess() override;

    // Replaces the command and (re)starts it. `stdinData`, when not empty, is
    // written once and the write channel is then closed.
    void start(const RemoteCommand &command, const QByteArray &stdinData = {});
    void stop();

    [[nodiscard]] State state() const { return m_state; }
    // The last few stderr lines of the most recent run, for the status bar.
    [[nodiscard]] QString lastError() const { return m_lastError; }

Q_SIGNALS:
    // One batch per read; a batch may hold a whole journal backlog.
    void linesReceived(const QStringList &lines);
    void stateChanged();
    // Each stderr line as it arrives (journalctl reports permission problems
    // there while it keeps running).
    void errorLineReceived(const QString &line);
    // The child ended (for any reason); a restart is already scheduled.
    void ended(int exitCode, const QString &error);

private:
    void launch();
    void readStdout();
    void readStderr();
    void finished(int exitCode);
    void setState(State state);

    QProcess *m_process = nullptr;
    QTimer m_retry;
    RemoteCommand m_command;
    QByteArray m_stdin;
    QByteArray m_partial;
    QStringList m_stderr;
    QString m_lastError;
    State m_state = State::Stopped;
    int m_backoffMs = 0;
    bool m_sawOutput = false;
};

} // namespace QindaLlamaWatch
