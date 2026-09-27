// SPDX-License-Identifier: GPL-3.0-or-later

#include "llama/slotrates.h"

#include <algorithm>

namespace QindaLlamaWatch {

SlotView slotFromJson(const QVariantMap &slot)
{
    SlotView view;
    view.id = slot.value(QStringLiteral("id")).toInt();
    view.task = slot.value(QStringLiteral("id_task"), -1).toLongLong();
    view.busy = slot.value(QStringLiteral("is_processing")).toBool();
    view.promptTokens = slot.value(QStringLiteral("n_prompt_tokens")).toLongLong();
    view.promptDone = slot.value(QStringLiteral("n_prompt_tokens_processed")).toLongLong();
    view.promptCached = slot.value(QStringLiteral("n_prompt_tokens_cache")).toLongLong();
    view.ctx = slot.value(QStringLiteral("n_ctx")).toLongLong();
    // next_token is a list in current llama.cpp and an object in older builds.
    const QVariant next = slot.value(QStringLiteral("next_token"));
    const QVariantMap nextMap = next.typeId() == QMetaType::QVariantList
        ? next.toList().value(0).toMap()
        : next.toMap();
    view.decoded = nextMap.value(QStringLiteral("n_decoded")).toLongLong();
    return view;
}

QVariantMap slotToQml(const SlotView &slot)
{
    QString phase = QStringLiteral("idle");
    if (slot.busy && slot.decoded == 0 && slot.promptTokens > 0) {
        phase = QStringLiteral("prompt");
    } else if (slot.busy) {
        phase = QStringLiteral("generate");
    }
    return {
        {QStringLiteral("id"), slot.id},
        {QStringLiteral("busy"), slot.busy},
        {QStringLiteral("phase"), phase},
        {QStringLiteral("promptTokens"), slot.promptTokens},
        {QStringLiteral("promptProgress"), slot.promptCached + slot.promptDone},
        {QStringLiteral("decoded"), slot.decoded},
        {QStringLiteral("ctx"), slot.ctx},
        {QStringLiteral("used"), slot.promptTokens + slot.decoded},
    };
}

SlotRates::Rates SlotRates::update(const QList<SlotView> &views, double nowSeconds)
{
    Rates rates;
    for (const SlotView &slot : views) {
        const auto it = m_tracks.constFind(slot.id);
        // AGENT-GUARD: only the same task counts. A new task resets the
        // counters to zero, and comparing across tasks would report the
        // previous task's whole output as one poll's worth of speed.
        if (it != m_tracks.cend() && it->task == slot.task && slot.busy) {
            const double dt = std::max(nowSeconds - it->t, 1e-3);
            rates.prompt += double(std::max<qint64>(0, slot.promptDone - it->promptDone)) / dt;
            rates.generate += double(std::max<qint64>(0, slot.decoded - it->decoded)) / dt;
        }
        m_tracks.insert(slot.id, {slot.task, slot.promptDone, slot.decoded, nowSeconds});
    }
    return rates;
}

} // namespace QindaLlamaWatch
