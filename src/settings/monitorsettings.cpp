// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/monitorsettings.h"

#include <QDir>
#include <QFile>
#include <QSettings>

#include <algorithm>

namespace QindaLlamaWatch {

namespace {

// AGENT-NOTE: the defaults point at the QindaQt LLM Server
// (sci-ml/qinda-llm-server): port 8100 on the workstation "qinda", reached
// over the tailnet, with the key file the qinda-llm client tools share.
// Every value is editable in Settings and overridable with --server/--ssh.
const char *const kDefaultServer = "http://qinda:8100";
const char *const kDefaultSshHost = "qinda";
const char *const kDefaultKeyFile = "~/.config/qinda-llm/api-key";
const char *const kDefaultUnit = "qinda-llm.service";

QString expandHome(const QString &path)
{
    if (path == QLatin1String("~")) {
        return QDir::homePath();
    }
    if (path.startsWith(QLatin1String("~/"))) {
        return QDir::homePath() + path.mid(1);
    }
    return path;
}

} // namespace

MonitorSettings::MonitorSettings(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_serverUrl = settings.value(QStringLiteral("server/url"), QString::fromLatin1(kDefaultServer)).toString();
    m_apiKeyFile = settings.value(QStringLiteral("server/apiKeyFile"), QString::fromLatin1(kDefaultKeyFile)).toString();
    m_intervalMs = std::clamp(settings.value(QStringLiteral("server/intervalMs"), 1000).toInt(), 250, 10000);
    m_sshHost = settings.value(QStringLiteral("host/ssh"), QString::fromLatin1(kDefaultSshHost)).toString();
    m_journalUnit = settings.value(QStringLiteral("host/journalUnit"), QString::fromLatin1(kDefaultUnit)).toString();
    m_journalSudo = settings.value(QStringLiteral("host/journalSudo"), false).toBool();
}

QString MonitorSettings::apiKey() const
{
    const QString fromEnv = qEnvironmentVariable("QINDA_LLM_API_KEY").trimmed();
    if (!fromEnv.isEmpty()) {
        return fromEnv;
    }
    QFile file(expandHome(m_apiKeyFile.trimmed()));
    if (m_apiKeyFile.trimmed().isEmpty() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readLine()).trimmed();
}

void MonitorSettings::apply(const QString &serverUrl, const QString &sshHost,
                            const QString &apiKeyFile, int intervalMs,
                            const QString &journalUnit, bool journalSudo)
{
    m_serverUrl = serverUrl.trimmed();
    m_sshHost = sshHost.trimmed();
    m_apiKeyFile = apiKeyFile.trimmed();
    m_intervalMs = std::clamp(intervalMs, 250, 10000);
    m_journalUnit = journalUnit.trimmed();
    m_journalSudo = journalSudo;

    QSettings settings;
    settings.setValue(QStringLiteral("server/url"), m_serverUrl);
    settings.setValue(QStringLiteral("server/apiKeyFile"), m_apiKeyFile);
    settings.setValue(QStringLiteral("server/intervalMs"), m_intervalMs);
    settings.setValue(QStringLiteral("host/ssh"), m_sshHost);
    settings.setValue(QStringLiteral("host/journalUnit"), m_journalUnit);
    settings.setValue(QStringLiteral("host/journalSudo"), m_journalSudo);
    Q_EMIT changed();
}

void MonitorSettings::overrideServer(const QString &serverUrl)
{
    m_serverUrl = serverUrl.trimmed();
    Q_EMIT changed();
}

void MonitorSettings::overrideSshHost(const QString &sshHost)
{
    m_sshHost = sshHost.trimmed();
    Q_EMIT changed();
}

} // namespace QindaLlamaWatch
