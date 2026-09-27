// SPDX-License-Identifier: GPL-3.0-or-later
//
// Polls a llama.cpp server over HTTP: /models (router mode or a single
// model), then /props, /slots and /metrics for the loaded model. Publishes
// what qinda-llm-top shows in its MODEL, THROUGHPUT and SLOTS panels, and
// loads/unloads models through the router API.
//
// Ownership/threading: GUI thread; owns its QNetworkAccessManager. One poll
// is in flight at a time; a tick that arrives during a poll is skipped, so a
// slow server is never flooded.

#pragma once

#include "llama/slotrates.h"

#include <QElapsedTimer>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class QNetworkReply;

namespace QindaLlamaWatch {

class LlamaClient final : public QObject
{
    Q_OBJECT
    // "off" (no URL), "connecting", "online", "offline".
    Q_PROPERTY(QString state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString baseUrl READ baseUrl NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool routerMode READ routerMode NOTIFY sampled FINAL)
    // [{id, status}] with status "loaded", "loading", "sleeping", "unloaded"...
    Q_PROPERTY(QVariantList models READ models NOTIFY sampled FINAL)
    Q_PROPERTY(QString activeModel READ activeModel NOTIFY sampled FINAL)
    // AGENT-CONTRACT: keys read by qml/ModelPanel.qml: modelPath, ctx,
    // totalSlots, build, sampling (list of {label, value}).
    Q_PROPERTY(QVariantMap server READ server NOTIFY sampled FINAL)
    // See slotToQml() in slotrates.h.
    Q_PROPERTY(QVariantList slotList READ slotList NOTIFY sampled FINAL)
    // AGENT-CONTRACT: keys read by qml/ThroughputPanel.qml: promptLive,
    // generateLive (tokens/s now), promptAvg, generateAvg (server lifetime),
    // promptTotal, cachedTotal, generatedTotal, processing, deferred, and
    // kvUsage (0..1) when the server exports it. Absent = not reported.
    Q_PROPERTY(QVariantMap throughput READ throughput NOTIFY sampled FINAL)
    // A transient one-line result of the last user action.
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged FINAL)

public:
    explicit LlamaClient(QObject *parent = nullptr);

    // `apiKey` may be empty (server without --api-key).
    void configure(const QUrl &baseUrl, const QString &apiKey, int intervalMs);

    [[nodiscard]] QString state() const;
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] QString baseUrl() const { return m_base.toString(); }
    [[nodiscard]] bool routerMode() const { return m_routerMode; }
    [[nodiscard]] QVariantList models() const { return m_models; }
    [[nodiscard]] QString activeModel() const { return m_active; }
    [[nodiscard]] QVariantMap server() const { return m_server; }
    [[nodiscard]] QVariantList slotList() const { return m_slots; }
    [[nodiscard]] QVariantMap throughput() const { return m_throughput; }
    [[nodiscard]] QString notice() const { return m_notice; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadModel(const QString &id);
    Q_INVOKABLE void unloadModel(const QString &id);
    // Cycles to the model after the active one, like llm-top's `m` key.
    Q_INVOKABLE void loadNextModel();

Q_SIGNALS:
    void stateChanged();
    // Once per completed poll; graphs append on this.
    void sampled();
    void noticeChanged();

private:
    using JsonHandler = std::function<void(const QVariant &json, const QByteArray &raw, bool ok)>;

    void poll();
    void takeModels(const QVariant &json, bool ok);
    void finishPoll();
    void get(const QString &path, JsonHandler handler);
    void post(const QString &path, const QVariantMap &body, JsonHandler handler);
    void track(QNetworkReply *reply, const QString &path, JsonHandler handler);
    [[nodiscard]] QString modelQuery() const;
    void setNotice(const QString &text);
    void setOnline(bool online, const QString &error);

    QNetworkAccessManager m_net;
    QTimer m_timer;
    QTimer m_noticeTimer;
    QElapsedTimer m_clock;
    SlotRates m_rates;
    QUrl m_base;
    QByteArray m_auth;
    QString m_error;
    QString m_notice;
    QString m_active;
    QVariantList m_models;
    QVariantMap m_server;
    QVariantList m_slots;
    QVariantMap m_throughput;
    QHash<QString, double> m_metrics;
    int m_pending = 0;
    quint64 m_generation = 0;       // bumps on configure; late replies are dropped
    bool m_routerMode = false;
    bool m_online = false;
    bool m_everAnswered = false;
};

} // namespace QindaLlamaWatch
