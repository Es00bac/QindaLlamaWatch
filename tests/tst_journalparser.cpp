// SPDX-License-Identifier: GPL-3.0-or-later
//
// Journal lines -> completed requests and load facts, matching
// parse_journal() in qinda-llm-top.

#include "journal/journalparser.h"

#include <QtTest>

using namespace QindaLlamaWatch;

namespace {
const char *const kPrefix = "2026-09-27T15:54:10-0500 qinda llama-server[812]: ";

QString line(const char *message)
{
    return QString::fromLatin1(kPrefix) + QString::fromLatin1(message);
}
} // namespace

class TestJournalParser : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pairsPromptAndEvalTimings()
    {
        JournalParser parser;
        QVERIFY(!parser.feed(line("spawning server instance with name=ornith on port 8101")).has_value());
        QVERIFY(!parser.feed(line("[8101] prompt eval time =   15153.10 ms / 18342 tokens (    0.83 ms per token,  1210.44 tokens per second)")).has_value());
        const std::optional<CompletedRequest> request =
            parser.feed(line("[8101]        eval time =   23325.51 ms /  1024 tokens (   22.78 ms per token,    43.90 tokens per second)"));
        QVERIFY(request.has_value());
        QCOMPARE(request->when, QStringLiteral("15:54:10"));
        QCOMPARE(request->model, QStringLiteral("ornith"));
        QCOMPARE(request->promptTokens, 18342LL);
        QCOMPARE(request->promptTps, 1210.44);
        QCOMPARE(request->promptMs, 15153.10);
        QCOMPARE(request->genTokens, 1024LL);
        QCOMPARE(request->genTps, 43.90);
    }

    void evalWithoutPromptIsIgnored()
    {
        JournalParser parser;
        QVERIFY(!parser.feed(line("       eval time =   100.00 ms /  10 tokens (   10.00 ms per token,  100.00 tokens per second)")).has_value());
    }

    void singleModelServerHasNoModelTag()
    {
        JournalParser parser;
        static_cast<void>(parser.feed(line("prompt eval time =  10.00 ms / 5 tokens (2.00 ms per token, 500.00 tokens per second)")));
        const auto request = parser.feed(line("       eval time =  20.00 ms / 2 tokens (10.00 ms per token, 100.00 tokens per second)"));
        QVERIFY(request.has_value());
        QVERIFY(request->model.isEmpty());
    }

    void collectsLoadFactsAndForgetsThemOnRespawn()
    {
        JournalParser parser;
        static_cast<void>(parser.feed(line("[8101] load_tensors: offloaded 41/41 layers to GPU")));
        static_cast<void>(parser.feed(line("[8101] load_tensors:      Vulkan0 model buffer size = 19876.20 MiB")));
        static_cast<void>(parser.feed(line("[8101] load_tensors:   CPU_Mapped model buffer size =   412.50 MiB")));
        static_cast<void>(parser.feed(line("[8101] llama_kv_cache:    Vulkan0 KV buffer size =  2048.00 MiB")));
        QCOMPARE(parser.facts().value(QStringLiteral("layers")), QStringLiteral("41/41"));
        QCOMPARE(parser.facts().value(QStringLiteral("gpu_buf")), QStringLiteral("19876.20"));
        QCOMPARE(parser.facts().value(QStringLiteral("cpu_buf")), QStringLiteral("412.50"));
        QCOMPARE(parser.facts().value(QStringLiteral("kv")), QStringLiteral("2048.00"));

        static_cast<void>(parser.feed(line("spawning server instance with name=qwen on port 8102")));
        QVERIFY(parser.facts().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestJournalParser)
#include "tst_journalparser.moc"
