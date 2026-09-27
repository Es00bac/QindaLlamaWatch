// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reads llama-server journal lines (journalctl -o short-iso) into completed
// requests with their prompt/generation timings, and the "load facts" the
// server logs while placing a model (GPU layers, buffer sizes, KV cache).
// Incremental port of parse_journal() in qinda-llm-top.

#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace QindaLlamaWatch {

struct CompletedRequest final {
    QString when;          // HH:MM:SS from the journal timestamp
    QString model;         // from the router's "[port]" tag, when present
    qint64 promptTokens = 0;
    double promptMs = 0;
    double promptTps = 0;
    qint64 genTokens = 0;
    double genMs = 0;
    double genTps = 0;
};

class JournalParser final
{
public:
    // Returns a request when `line` completes one (its "eval time" line).
    [[nodiscard]] std::optional<CompletedRequest> feed(const QString &line);

    // AGENT-CONTRACT: keys read by qml/ModelPanel.qml: layers ("33/49"),
    // gpu_buf, cpu_buf, kv (MiB), n_cpu_moe, fa, graph_splits. A new
    // "spawning server instance" line clears them (a new model instance).
    [[nodiscard]] const QHash<QString, QString> &facts() const { return m_facts; }

    void reset();

private:
    QHash<QString, QString> m_ports;       // router port -> model name
    QHash<QString, QString> m_facts;
    std::optional<CompletedRequest> m_pending;
};

} // namespace QindaLlamaWatch
