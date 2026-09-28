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

// A prompt being read, from llama-server's per-batch progress line:
// "id  0 | task 40651 | prompt processing, n_tokens = 30322, progress = 0.30, ...".
// /slots cannot give this: on the router build its n_prompt_tokens is the end
// of the batch being read, not the prompt's length.
struct PromptProgress final {
    qint64 task = -1;
    qint64 tokens = 0;      // prompt tokens in the slot so far
    double fraction = 0;    // 0..1, logged with two decimals
    QString when;
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

    // Prompt reads in progress, by slot id; an entry goes when its task's
    // prompt timing or release line arrives, or a new instance spawns.
    [[nodiscard]] const QHash<int, PromptProgress> &progress() const { return m_progress; }

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
    QHash<int, PromptProgress> m_progress;
    QHash<QString, QString> m_facts;
    std::optional<CompletedRequest> m_pending;
};

} // namespace QindaLlamaWatch
