// SPDX-License-Identifier: GPL-3.0-or-later
//
// Live prompt and generation speed from consecutive /slots snapshots.
// Generation advances every token, so it is measured poll to poll (as in
// Collector._rates() in qinda-llm-top). Prompt reading is not: llama-server
// advances n_prompt_tokens_processed once per logical batch (2,048 tokens,
// ~15 s at long context on qinda), so it is measured jump to jump and the
// last rate is held while the next batch is read.

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

// AGENT-CONTRACT: keys read by qml/SlotsPanel.qml: id, task, busy, phase
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

    // A held prompt rate is dropped once no batch has completed for this
    // many times the previous batch's duration (a stalled or finished read).
    static constexpr double kHoldFactor = 3.0;
    void reset() { m_tracks.clear(); }

private:
    struct Track {
        qint64 task = -1;
        qint64 promptDone = 0;
        qint64 decoded = 0;
        double t = 0;
        double promptSince = 0;    // when promptDone last advanced (or the task was first seen)
        double promptRate = 0;     // tokens/s over the last advance
        double promptGap = 0;      // seconds the last advance took
        // False when the task was first seen mid-read: the first advance
        // then covers only part of a batch and would overstate the rate.
        bool promptTimed = false;
    };
    QHash<int, Track> m_tracks;
};

} // namespace QindaLlamaWatch
