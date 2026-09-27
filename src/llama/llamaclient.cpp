// SPDX-License-Identifier: GPL-3.0-or-later

#include "llama/llamaclient.h"

#include "llama/metricsparser.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <algorithm>
#include <utility>

namespace QindaLlamaWatch {

namespace {

constexpr int kRequestTimeoutMs = 2500;
constexpr int kNoticeMs = 6000;

QString statusOf(const QVariantMap &entry)
{
    const QVariant status = entry.value(QStringLiteral("status"));
    if (status.typeId() == QMetaType::QVariantMap) {
        return status.toMap().value(QStringLiteral("value"), QStringLiteral("unknown")).toString();
    }
    return status.isValid() ? status.toString() : QString();
}

QVariantList samplingOf(const QVariantMap &props)
{
    const QVariantMap params = props.value(QStringLiteral("default_generation_settings"))
                                   .toMap()
                                   .value(QStringLiteral("params"))
                                   .toMap();
    static const std::pair<const char *, const char *> keys[] = {
        {"temperature", "temp"}, {"top_p", "top-p"}, {"top_k", "top-k"},
        {"min_p", "min-p"}, {"presence_penalty", "presence"}, {"repeat_penalty", "repeat"},
    };
    QVariantList out;
    for (const auto &[key, label] : keys) {
        const QVariant value = params.value(QString::fromLatin1(key));
        if (value.isValid() && !value.isNull()) {
            out.append(QVariantMap{{QStringLiteral("label"), QString::fromLatin1(label)},
                                   {QStringLiteral("value"), QString::number(value.toDouble(), 'g', 4)}});
        }
    }
    return out;
}

} // namespace

LlamaClient::LlamaClient(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &LlamaClient::poll);
    m_noticeTimer.setSingleShot(true);
    connect(&m_noticeTimer, &QTimer::timeout, this, [this] { setNotice({}); });
}

void LlamaClient::configure(const QUrl &baseUrl, const QString &apiKey, int intervalMs)
{
    ++m_generation;
    m_pending = 0;
    m_base = baseUrl;
    // Accept a pasted OpenAI-style base URL ("http://host:8100/v1"): the
    // monitoring endpoints live at the server root.
    QString path = m_base.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (path.endsWith(QLatin1String("/v1"))) {
        path.chop(3);
    }
    m_base.setPath(path);
    m_auth = apiKey.isEmpty() ? QByteArray() : "Bearer " + apiKey.toUtf8();
    m_error.clear();
    m_online = false;
    m_everAnswered = false;
    m_models.clear();
    m_active.clear();
    m_server.clear();
    m_slots.clear();
    m_throughput.clear();
    m_metrics.clear();
    m_rates.reset();
    Q_EMIT stateChanged();
    Q_EMIT sampled();

    m_timer.stop();
    if (m_base.isValid() && !m_base.host().isEmpty()) {
        m_timer.start(std::clamp(intervalMs, 250, 10000));
        poll();
    }
}

QString LlamaClient::state() const
{
    if (!m_base.isValid() || m_base.host().isEmpty()) {
        return QStringLiteral("off");
    }
    if (m_online) {
        return QStringLiteral("online");
    }
    return m_everAnswered || !m_error.isEmpty() ? QStringLiteral("offline") : QStringLiteral("connecting");
}

void LlamaClient::refresh()
{
    poll();
}

void LlamaClient::poll()
{
    if (m_pending > 0 || state() == QLatin1String("off")) {
        return;
    }
    m_pending = 1;
    get(QStringLiteral("/models"), [this](const QVariant &json, const QByteArray &, bool ok) {
        takeModels(json, ok);
    });
}

QString LlamaClient::modelQuery() const
{
    // AGENT-NOTE: a router proxies /props, /slots and /metrics to one model
    // instance chosen by ?model=; a single-model server ignores it, but it
    // is left off there so a strict proxy in front cannot reject it.
    if (!m_routerMode || m_active.isEmpty()) {
        return {};
    }
    return QStringLiteral("?model=") + QString::fromUtf8(QUrl::toPercentEncoding(m_active));
}

void LlamaClient::takeModels(const QVariant &json, bool ok)
{
    if (!ok) {
        m_pending = 0;
        m_models.clear();
        m_active.clear();
        m_slots.clear();
        m_rates.reset();
        Q_EMIT sampled();
        return;
    }
    const QVariantList data = json.toMap().value(QStringLiteral("data")).toList();
    QVariantList models;
    QString active;
    bool anyStatus = false;
    for (const QVariant &item : data) {
        const QVariantMap entry = item.toMap();
        const QString id = entry.value(QStringLiteral("id")).toString();
        const QString status = statusOf(entry);
        anyStatus = anyStatus || !status.isEmpty();
        models.append(QVariantMap{{QStringLiteral("id"), id},
                                  {QStringLiteral("status"), status.isEmpty() ? QStringLiteral("loaded") : status}});
        if (active.isEmpty() && (status == QLatin1String("loaded") || status.isEmpty())) {
            active = id;
        }
    }
    m_routerMode = anyStatus;
    m_models = models;
    if (active != m_active) {
        m_rates.reset();
    }
    m_active = active;

    if (m_active.isEmpty()) {
        m_pending = 0;
        m_server.clear();
        m_slots.clear();
        m_throughput.clear();
        Q_EMIT sampled();
        return;
    }

    const QString query = modelQuery();
    m_pending = 3;
    get(QStringLiteral("/props") + query, [this](const QVariant &props, const QByteArray &, bool ok) {
        if (ok) {
            const QVariantMap map = props.toMap();
            const QVariantMap defaults = map.value(QStringLiteral("default_generation_settings")).toMap();
            QVariantMap server;
            server.insert(QStringLiteral("modelPath"), map.value(QStringLiteral("model_path")));
            server.insert(QStringLiteral("ctx"), map.value(QStringLiteral("n_ctx"),
                                                           defaults.value(QStringLiteral("n_ctx"))));
            server.insert(QStringLiteral("totalSlots"), map.value(QStringLiteral("total_slots")));
            server.insert(QStringLiteral("build"), map.value(QStringLiteral("build_info")));
            server.insert(QStringLiteral("sampling"), samplingOf(map));
            m_server = server;
        }
        finishPoll();
    });
    get(QStringLiteral("/slots") + query, [this](const QVariant &slotsJson, const QByteArray &, bool ok) {
        QList<SlotView> views;
        QVariantList qml;
        if (ok) {
            for (const QVariant &item : slotsJson.toList()) {
                const SlotView view = slotFromJson(item.toMap());
                views.append(view);
                qml.append(slotToQml(view));
            }
        }
        const SlotRates::Rates rates = m_rates.update(views, double(m_clock.elapsed()) / 1000.0);
        m_slots = qml;
        m_throughput.insert(QStringLiteral("promptLive"), rates.prompt);
        m_throughput.insert(QStringLiteral("generateLive"), rates.generate);
        finishPoll();
    });
    get(QStringLiteral("/metrics") + query, [this](const QVariant &, const QByteArray &raw, bool ok) {
        // /metrics needs --metrics on the server; without it the rest of the
        // dashboard still works and the lifetime figures show dashes.
        m_metrics = ok ? parsePrometheus(QString::fromUtf8(raw)) : QHash<QString, double>();
        finishPoll();
    });
}

void LlamaClient::finishPoll()
{
    if (--m_pending > 0) {
        return;
    }
    m_pending = 0;
    const QHash<QString, double> &m = m_metrics;
    auto put = [this, &m](const char *key, const char *metric) {
        const auto it = m.constFind(QString::fromLatin1(metric));
        if (it != m.cend()) {
            m_throughput.insert(QString::fromLatin1(key), it.value());
        } else {
            m_throughput.remove(QString::fromLatin1(key));
        }
    };
    put("promptTotal", "prompt_tokens_total");
    put("cachedTotal", "prompt_tokens_cached_total");
    put("generatedTotal", "tokens_predicted_total");
    put("processing", "requests_processing");
    put("deferred", "requests_deferred");
    put("kvUsage", "kv_cache_usage_ratio");
    const double genSeconds = m.value(QStringLiteral("tokens_predicted_seconds_total"));
    const double promptSeconds = m.value(QStringLiteral("prompt_seconds_total"));
    if (genSeconds > 0) {
        m_throughput.insert(QStringLiteral("generateAvg"),
                            m.value(QStringLiteral("tokens_predicted_total")) / genSeconds);
    }
    if (promptSeconds > 0) {
        m_throughput.insert(QStringLiteral("promptAvg"),
                            m.value(QStringLiteral("prompt_tokens_total")) / promptSeconds);
    }
    Q_EMIT sampled();
}

void LlamaClient::loadModel(const QString &id)
{
    if (id.isEmpty()) {
        return;
    }
    setNotice(tr("loading %1…").arg(id));
    post(QStringLiteral("/models/load"), {{QStringLiteral("model"), id}},
         [this, id](const QVariant &, const QByteArray &, bool ok) {
             setNotice(ok ? tr("loading %1…").arg(id) : tr("load failed: %1").arg(m_error));
             poll();
         });
}

void LlamaClient::unloadModel(const QString &id)
{
    if (id.isEmpty()) {
        return;
    }
    post(QStringLiteral("/models/unload"), {{QStringLiteral("model"), id}},
         [this, id](const QVariant &, const QByteArray &, bool ok) {
             setNotice(ok ? tr("unloading %1").arg(id) : tr("unload failed: %1").arg(m_error));
             poll();
         });
}

void LlamaClient::loadNextModel()
{
    QStringList ids;
    for (const QVariant &model : std::as_const(m_models)) {
        ids.append(model.toMap().value(QStringLiteral("id")).toString());
    }
    if (ids.isEmpty()) {
        setNotice(tr("the server lists no models"));
        return;
    }
    const qsizetype index = ids.indexOf(m_active);
    loadModel(ids.at((index + 1) % ids.size()));
}

void LlamaClient::get(const QString &path, JsonHandler handler)
{
    QNetworkRequest request(QUrl(m_base.toString() + path));
    request.setTransferTimeout(kRequestTimeoutMs);
    if (!m_auth.isEmpty()) {
        request.setRawHeader("Authorization", m_auth);
    }
    track(m_net.get(request), path, std::move(handler));
}

void LlamaClient::post(const QString &path, const QVariantMap &body, JsonHandler handler)
{
    QNetworkRequest request(QUrl(m_base.toString() + path));
    request.setTransferTimeout(kRequestTimeoutMs * 2);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_auth.isEmpty()) {
        request.setRawHeader("Authorization", m_auth);
    }
    track(m_net.post(request, QJsonDocument::fromVariant(body).toJson(QJsonDocument::Compact)), path,
          std::move(handler));
}

void LlamaClient::track(QNetworkReply *reply, const QString &path, JsonHandler handler)
{
    const quint64 generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, path, generation, handler] {
        reply->deleteLater();
        // AGENT-GUARD: a reply from before configure() belongs to another
        // server; applying it would mix two servers' figures (and corrupt
        // m_pending for the new poll).
        if (generation != m_generation) {
            return;
        }
        const QByteArray raw = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError;
        if (!ok) {
            const QString message = status > 0 ? tr("HTTP %1 on %2").arg(status).arg(path)
                                               : reply->errorString();
            // Only the /models probe decides whether the server is reachable;
            // a disabled /metrics or /slots endpoint is not an outage.
            if (path == QLatin1String("/models")) {
                setOnline(false, message);
            } else {
                m_error = message;
            }
            handler({}, raw, false);
            return;
        }
        if (path == QLatin1String("/models")) {
            setOnline(true, {});
        }
        handler(QJsonDocument::fromJson(raw).toVariant(), raw, true);
    });
}

void LlamaClient::setOnline(bool online, const QString &error)
{
    m_everAnswered = m_everAnswered || online;
    if (m_online == online && m_error == error) {
        return;
    }
    m_online = online;
    m_error = error;
    Q_EMIT stateChanged();
}

void LlamaClient::setNotice(const QString &text)
{
    if (m_notice == text) {
        return;
    }
    m_notice = text;
    if (!text.isEmpty()) {
        m_noticeTimer.start(kNoticeMs);
    }
    Q_EMIT noticeChanged();
}

} // namespace QindaLlamaWatch
