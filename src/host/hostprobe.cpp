// SPDX-License-Identifier: GPL-3.0-or-later

#include "host/hostprobe.h"

#include <QFile>

#include <algorithm>

namespace QindaLlamaWatch {

namespace {

QByteArray samplerScript()
{
    // AGENT-CONTRACT: resource path set by qt_add_resources in CMakeLists.txt.
    QFile file(QStringLiteral(":/llamawatch/sampler.sh"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

HostProbe::HostProbe(QObject *parent)
    : QObject(parent)
{
    connect(&m_stream, &StreamProcess::linesReceived, this, &HostProbe::takeLines);
    connect(&m_stream, &StreamProcess::stateChanged, this, &HostProbe::stateChanged);
    connect(&m_stream, &StreamProcess::ended, this, [this] {
        // A new connection starts a new counter baseline; rates from the
        // previous session would compare unrelated uptimes.
        m_parser.reset();
        m_stats.reset();
        // AGENT-GUARD: stale figures must not stay on screen looking live
        // while the session is down; the panel shows the error instead.
        m_reading.clear();
        Q_EMIT sampled();
    });
}

void HostProbe::configure(const QString &host, int intervalMs)
{
    m_host = host.trimmed();
    m_parser.reset();
    m_stats.reset();
    m_reading.clear();
    Q_EMIT sampled();

    const double seconds = std::clamp(intervalMs, 250, 10000) / 1000.0;
    // `sh -s` reads the script from stdin; the interval is $1.
    const RemoteCommand command = remoteCommand(
        m_host, {QStringLiteral("sh"), QStringLiteral("-s"), QStringLiteral("--"),
                 QString::number(seconds, 'f', 2)});
    m_stream.start(command, samplerScript());
    Q_EMIT stateChanged();
}

QString HostProbe::state() const
{
    if (m_host.isEmpty()) {
        return QStringLiteral("off");
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

QString HostProbe::error() const
{
    return m_stream.lastError();
}

void HostProbe::takeLines(const QStringList &lines)
{
    bool changed = false;
    for (const QString &line : lines) {
        if (std::optional<HostFrame> frame = m_parser.feed(line)) {
            m_reading = m_stats.update(*frame, m_parser.identity());
            changed = true;
        }
    }
    // A backlog can deliver several frames at once; only the newest is shown.
    if (changed) {
        Q_EMIT sampled();
    }
}

} // namespace QindaLlamaWatch
