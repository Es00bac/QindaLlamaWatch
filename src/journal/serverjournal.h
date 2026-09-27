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
#include "journal/journalfeed.h"

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
    // Requests completed this boot; the table keeps only the newest 200.
    Q_PROPERTY(int requestTotal READ requestTotal NOTIFY requestsChanged FINAL)
    Q_PROPERTY(QVariantMap facts READ facts NOTIFY factsChanged FINAL)
    // Model instances that died this boot (non-zero exit), newest first:
    // {when, model, status, reason}; the list keeps 50, the total counts all.
    Q_PROPERTY(QVariantList crashes READ crashes NOTIFY crashesChanged FINAL)
    Q_PROPERTY(int crashTotal READ crashTotal NOTIFY crashesChanged FINAL)
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
    [[nodiscard]] QVariantList requests() const { return m_feed.requests(); }
    [[nodiscard]] int requestTotal() const { return m_feed.requestTotal(); }
    [[nodiscard]] QVariantMap facts() const { return m_feed.facts(); }
    [[nodiscard]] QVariantList crashes() const { return m_feed.crashes(); }
    [[nodiscard]] int crashTotal() const { return m_feed.crashTotal(); }
    [[nodiscard]] QStringList logLines() const { return m_feed.log(); }

Q_SIGNALS:
    void stateChanged();
    void requestsChanged();
    void factsChanged();
    void logChanged();
    void crashesChanged();
    // A crash seen while following (not one read from the boot history).
    void crashDetected(const QVariantMap &crash);
    void serviceActionFinished(bool ok, const QString &message);

private:
    void takeLines(const QStringList &lines);
    void takeError(const QString &line);
    void clear();

    StreamProcess m_stream;
    JournalFeed m_feed;
    QString m_host;
    QString m_unit;
    QString m_denied;
};

} // namespace QindaLlamaWatch
