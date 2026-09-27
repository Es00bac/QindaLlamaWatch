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
#include <utility>

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

// A model instance that exited with a non-zero status. The router respawns
// it, so from the API this looks like a model briefly reloading.
struct InstanceCrash final {
    QString when;          // HH:MM:SS of the error line, else of the exit line
    QString model;
    int status = 0;
    QString reason;        // what() text, else the exception type, else "exit status N"
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

    // The crash the last feed() completed, if any; cleared by the call.
    [[nodiscard]] std::optional<InstanceCrash> takeCrash() { return std::exchange(m_crash, std::nullopt); }

    void reset();

private:
    struct LastError {
        QString when;
        QString text;
        bool fromWhat = false;             // what() text beats the exception type
    };

    QHash<QString, QString> m_ports;       // router port -> model name
    QHash<QString, QString> m_modelPort;   // model name -> its current port
    QHash<QString, LastError> m_lastError; // router port -> its last fatal error
    std::optional<InstanceCrash> m_crash;
    QHash<QString, QString> m_facts;
    std::optional<CompletedRequest> m_pending;
};

} // namespace QindaLlamaWatch
