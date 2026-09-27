// SPDX-License-Identifier: GPL-3.0-or-later
//
// Live prompt and generation speed from consecutive /slots snapshots: the
// tokens each busy slot advanced since the previous poll, over the time
// between polls. Port of Collector._rates() in qinda-llm-top.

#pragma once

#include <QHash>
#include <QVariantList>
#include <QVariantMap>

namespace QindaLlamaWatch {

// One slot, flattened from llama-server's /slots JSON.
struct SlotView final {
    int id = 0;
    qint64 task = -1;
    bool busy = false;
    qint64 promptTokens = 0;      // n_prompt_tokens
    qint64 promptDone = 0;        // n_prompt_tokens_processed
    qint64 promptCached = 0;      // n_prompt_tokens_cache
    qint64 decoded = 0;           // next_token[0].n_decoded
    qint64 ctx = 0;               // n_ctx
};

[[nodiscard]] SlotView slotFromJson(const QVariantMap &slot);

// AGENT-CONTRACT: keys read by qml/SlotsPanel.qml: id, busy, phase
// ("prompt"/"generate"/"idle"), promptTokens, promptProgress (cached+done),
// decoded, ctx, used.
[[nodiscard]] QVariantMap slotToQml(const SlotView &slot);

class SlotRates final
{
public:
    struct Rates {
        double prompt = 0;        // tokens/s read
        double generate = 0;      // tokens/s produced
    };

    // `nowSeconds` is a monotonic clock reading of when the snapshot arrived.
    [[nodiscard]] Rates update(const QList<SlotView> &views, double nowSeconds);
    void reset() { m_tracks.clear(); }

private:
    struct Track {
        qint64 task = -1;
        qint64 promptDone = 0;
        qint64 decoded = 0;
        double t = 0;
    };
    QHash<int, Track> m_tracks;
};

} // namespace QindaLlamaWatch
