// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows the llama.cpp service's journal on the server host
// (`journalctl -u <unit> -b -f`) for completed-request timings, model load
// facts and the log tail. The HTTP API has no request history; this is the
// only source of per-request prompt and generation speed.
//
// Ownership/threading: GUI thread; owns one StreamProcess.

#pragma once

#include "common/streamprocess.h"
#include "journal/journalparser.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace QindaLlamaWatch {

class ServerJournal final : public QObject
{
    Q_OBJECT
    // "off", "connecting", "live", "retrying", "denied" (journal not readable
    // by the ssh user; enable "use sudo -n" or add the user to systemd-journal).
    Q_PROPERTY(QString state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString unit READ unit NOTIFY stateChanged FINAL)
    // Newest first: {when, model, promptTokens, promptTps, promptSeconds,
    // genTokens, genTps, genSeconds}.
    Q_PROPERTY(QVariantList requests READ requests NOTIFY requestsChanged FINAL)
    Q_PROPERTY(QVariantMap facts READ facts NOTIFY factsChanged FINAL)
    Q_PROPERTY(QStringList logLines READ logLines NOTIFY logChanged FINAL)

public:
    explicit ServerJournal(QObject *parent = nullptr);

    void configure(const QString &host, const QString &unit, bool useSudo);

    // `systemctl restart --no-block <unit>` on the host through `sudo -n`,
    // like llm-top's `r` key; needs a NOPASSWD sudo rule for that command.
    // The outcome arrives as serviceActionFinished().
    Q_INVOKABLE void restartService();

    [[nodiscard]] QString state() const;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QString unit() const { return m_unit; }
    [[nodiscard]] QVariantList requests() const { return m_requests; }
    [[nodiscard]] QVariantMap facts() const;
    [[nodiscard]] QStringList logLines() const { return m_log; }

Q_SIGNALS:
    void stateChanged();
    void requestsChanged();
    void factsChanged();
    void logChanged();
    void serviceActionFinished(bool ok, const QString &message);

private:
    void takeLines(const QStringList &lines);
    void takeError(const QString &line);
    void clear();

    StreamProcess m_stream;
    JournalParser m_parser;
    QString m_host;
    QString m_unit;
    QVariantList m_requests;
    QStringList m_log;
    QString m_denied;
};

} // namespace QindaLlamaWatch
