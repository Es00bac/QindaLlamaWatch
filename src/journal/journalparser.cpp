// SPDX-License-Identifier: GPL-3.0-or-later

#include "journal/journalparser.h"

#include <QRegularExpression>
#include <QStringList>

#include <array>
#include <utility>

namespace QindaLlamaWatch {

namespace {

// AGENT-NOTE: these patterns are copied from /usr/bin/qinda-llm-top
// (sci-ml/qinda-llm-server) so the two monitors agree about a request.
// llama.cpp prints, per finished task:
//   prompt eval time =   812.34 ms /  1024 tokens (...  1260.55 tokens per second)
//          eval time =  4021.10 ms /   256 tokens (...    63.66 tokens per second)
const QRegularExpression &timingPrompt()
{
    static const QRegularExpression rx(QStringLiteral(
        R"(prompt eval time =\s*([\d.]+) ms /\s*(\d+) tokens .*?([\d.]+) tokens per second)"));
    return rx;
}

const QRegularExpression &timingGenerate()
{
    static const QRegularExpression rx(QStringLiteral(
        R"(^(?:.*?\s)?eval time =\s*([\d.]+) ms /\s*(\d+) tokens .*?([\d.]+) tokens per second)"));
    return rx;
}

// The router forwards each instance's output as "[<port>] <line>" and
// announces "spawning server instance with name=<model> on port <port>".
const QRegularExpression &childTag()
{
    static const QRegularExpression rx(QStringLiteral(R"(^\[\s*(\d+)\]\s)"));
    return rx;
}

const QRegularExpression &spawn()
{
    static const QRegularExpression rx(
        QStringLiteral(R"(spawning server instance with name=(\S+) on port (\d+))"));
    return rx;
}

struct Fact {
    const char *key;
    QRegularExpression rx;
};

const std::array<Fact, 7> &loadFacts()
{
    static const std::array<Fact, 7> facts{{
        {"layers", QRegularExpression(QStringLiteral(R"(offloaded (\d+)/(\d+) layers to GPU)"))},
        {"gpu_buf", QRegularExpression(QStringLiteral(R"((?:Vulkan0|ROCm0|CUDA0) model buffer size =\s*([\d.]+) MiB)"))},
        {"cpu_buf", QRegularExpression(QStringLiteral(R"(CPU(?:_Mapped|_REPACK)? model buffer size =\s*([\d.]+) MiB)"))},
        {"kv", QRegularExpression(QStringLiteral(R"(llama_kv_cache: .*size =\s*([\d.]+) MiB)"))},
        {"n_cpu_moe", QRegularExpression(QStringLiteral(R"(n_cpu_moe\s*[=:]\s*(\d+))"))},
        {"fa", QRegularExpression(QStringLiteral(R"(flash_attn\s*=\s*(\w+))"))},
        {"graph_splits", QRegularExpression(QStringLiteral(R"(graph splits = (\d+))"))},
    }};
    return facts;
}

} // namespace

void JournalParser::reset()
{
    m_ports.clear();
    m_facts.clear();
    m_pending.reset();
}

std::optional<CompletedRequest> JournalParser::feed(const QString &line)
{
    // short-iso: "2026-09-27T15:55:01-0500 qinda llama-server[812]: message"
    const QString when = (line.size() > 19 && line.at(4) == QLatin1Char('-')) ? line.mid(11, 8) : QString();
    const qsizetype split = line.indexOf(QLatin1String("]: "));
    const QString message = split >= 0 ? line.mid(split + 3) : line;

    if (const QRegularExpressionMatch m = spawn().match(message); m.hasMatch()) {
        m_ports.insert(m.captured(2), m.captured(1));
        m_facts.clear();                  // a new instance: forget the old one's facts
        return std::nullopt;
    }
    QString model;
    if (const QRegularExpressionMatch tag = childTag().match(message); tag.hasMatch()) {
        model = m_ports.value(tag.captured(1));
    }

    if (const QRegularExpressionMatch m = timingPrompt().match(line); m.hasMatch()) {
        CompletedRequest request;
        request.when = when;
        request.model = model;
        request.promptMs = m.captured(1).toDouble();
        request.promptTokens = m.captured(2).toLongLong();
        request.promptTps = m.captured(3).toDouble();
        m_pending = request;
        return std::nullopt;
    }
    if (!line.contains(QLatin1String("prompt eval")) && m_pending.has_value()) {
        if (const QRegularExpressionMatch m = timingGenerate().match(line); m.hasMatch()) {
            CompletedRequest request = std::exchange(m_pending, std::nullopt).value();
            request.genMs = m.captured(1).toDouble();
            request.genTokens = m.captured(2).toLongLong();
            request.genTps = m.captured(3).toDouble();
            return request;
        }
    }

    if (line.contains(QLatin1String("load_tensors")) || line.contains(QLatin1String("llama_"))
        || line.contains(QLatin1String("fit"))) {
        for (const Fact &fact : loadFacts()) {
            const QRegularExpressionMatch m = fact.rx.match(line);
            if (m.hasMatch()) {
                QStringList groups;
                for (int i = 1; i <= m.lastCapturedIndex(); ++i) {
                    groups.append(m.captured(i));
                }
                m_facts.insert(QString::fromLatin1(fact.key), groups.join(QLatin1Char('/')));
            }
        }
    }
    return std::nullopt;
}

} // namespace QindaLlamaWatch
