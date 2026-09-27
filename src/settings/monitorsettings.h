// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where the server is and how to reach its host. Persisted with QSettings
// under the application's organisation/name (QindaQt/qindallamawatch).
//
// The API key itself is never stored here or shown: only the path of the
// file that holds it, read when the connection is (re)configured.

#pragma once

#include <QObject>
#include <QString>

namespace QindaLlamaWatch {

class MonitorSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY changed FINAL)
    Q_PROPERTY(QString sshHost READ sshHost NOTIFY changed FINAL)
    Q_PROPERTY(QString apiKeyFile READ apiKeyFile NOTIFY changed FINAL)
    Q_PROPERTY(int intervalMs READ intervalMs NOTIFY changed FINAL)
    Q_PROPERTY(QString journalUnit READ journalUnit NOTIFY changed FINAL)
    Q_PROPERTY(bool journalSudo READ journalSudo NOTIFY changed FINAL)
    // Whether apiKeyFile exists and holds a non-empty first line.
    Q_PROPERTY(bool apiKeyFound READ apiKeyFound NOTIFY changed FINAL)

public:
    explicit MonitorSettings(QObject *parent = nullptr);

    [[nodiscard]] QString serverUrl() const { return m_serverUrl; }
    [[nodiscard]] QString sshHost() const { return m_sshHost; }
    [[nodiscard]] QString apiKeyFile() const { return m_apiKeyFile; }
    [[nodiscard]] int intervalMs() const { return m_intervalMs; }
    [[nodiscard]] QString journalUnit() const { return m_journalUnit; }
    [[nodiscard]] bool journalSudo() const { return m_journalSudo; }
    [[nodiscard]] bool apiKeyFound() const { return !apiKey().isEmpty(); }

    // First line of apiKeyFile ("~/" expanded), or $QINDA_LLM_API_KEY when
    // set, as qinda-llm-top does. Empty when neither exists.
    [[nodiscard]] QString apiKey() const;

    // Applies and persists every field at once, so a half-edited dialog can
    // never reconfigure the collectors field by field. Emits changed() once.
    Q_INVOKABLE void apply(const QString &serverUrl, const QString &sshHost,
                           const QString &apiKeyFile, int intervalMs,
                           const QString &journalUnit, bool journalSudo);

    // Command-line overrides for this run only (not persisted).
    void overrideServer(const QString &serverUrl);
    void overrideSshHost(const QString &sshHost);

Q_SIGNALS:
    void changed();

private:
    QString m_serverUrl;
    QString m_sshHost;
    QString m_apiKeyFile;
    QString m_journalUnit;
    int m_intervalMs = 1000;
    bool m_journalSudo = false;
};

} // namespace QindaLlamaWatch
