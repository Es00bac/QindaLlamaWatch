// SPDX-License-Identifier: GPL-3.0-or-later
//
// CPU, memory, GPU and network telemetry of the machine running llama.cpp.
// Runs src/host/sampler.sh on that host through one long-lived ssh session
// (or locally) and publishes each derived sample as `reading`.
//
// Ownership/threading: GUI thread. Parsing a frame is a few hundred lines of
// text per second, so it stays on the GUI thread; the child process does all
// waiting.

#pragma once

#include "common/streamprocess.h"
#include "host/hoststats.h"

#include <QObject>
#include <QVariantMap>

namespace QindaLlamaWatch {

class HostProbe final : public QObject
{
    Q_OBJECT
    // "off" (no host configured), "connecting", "live", "retrying".
    Q_PROPERTY(QString state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString host READ host NOTIFY stateChanged FINAL)
    // See HostStats::update for the keys; absent key = unavailable reading.
    Q_PROPERTY(QVariantMap reading READ reading NOTIFY sampled FINAL)

public:
    explicit HostProbe(QObject *parent = nullptr);

    // Restarts the sampler. An empty host turns host telemetry off.
    void configure(const QString &host, int intervalMs);

    [[nodiscard]] QString state() const;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] QVariantMap reading() const { return m_reading; }

Q_SIGNALS:
    void stateChanged();
    // Once per host sample; graphs append on this.
    void sampled();

private:
    void takeLines(const QStringList &lines);

    StreamProcess m_stream;
    HostSampleParser m_parser;
    HostStats m_stats;
    QString m_host;
    QVariantMap m_reading;
};

} // namespace QindaLlamaWatch
