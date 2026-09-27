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

    void reportsAnInstanceCrashWithItsReason()
    {
        // The exact sequence qinda logged at 17:08 on 2026-09-27.
        JournalParser parser;
        static_cast<void>(parser.feed(line("0.01.170.424 I srv          load: spawning server instance with name=ornith on port 34003")));
        QVERIFY(!parser.takeCrash().has_value());
        const QString at = QStringLiteral("2026-09-27T17:08:21-06:00 qinda llama-server[1217]: ");
        static_cast<void>(parser.feed(at + QStringLiteral("[34003] 19.45.810.546 E srv  update_slots: decode() failed: vk::Queue::submit: ErrorDeviceLost")));
        static_cast<void>(parser.feed(at + QStringLiteral("[34003] terminate called after throwing an instance of 'vk::DeviceLostError'")));
        static_cast<void>(parser.feed(at + QStringLiteral("[34003]   what():  vk::Queue::submit: ErrorDeviceLost")));
        QVERIFY(!parser.takeCrash().has_value());
        static_cast<void>(parser.feed(QStringLiteral("2026-09-27T17:08:26-06:00 qinda llama-server[1217]: 89.48.529.594 I srv           run: instance name=ornith exited with status 1")));
        const std::optional<InstanceCrash> crash = parser.takeCrash();
        QVERIFY(crash.has_value());
        QCOMPARE(crash->model, QStringLiteral("ornith"));
        QCOMPARE(crash->status, 1);
        QCOMPARE(crash->when, QStringLiteral("17:08:21"));     // the error, not the exit
        QCOMPARE(crash->reason, QStringLiteral("vk::Queue::submit: ErrorDeviceLost"));
        QVERIFY(!parser.takeCrash().has_value());               // taken once
    }

    void cleanExitIsNotACrash()
    {
        JournalParser parser;
        static_cast<void>(parser.feed(line("load: spawning server instance with name=ornith on port 46205")));
        static_cast<void>(parser.feed(line("118.24.567.311 I srv           run: instance name=ornith exited with status 0")));
        QVERIFY(!parser.takeCrash().has_value());
    }

    void crashWithoutErrorLinesReportsTheStatus()
    {
        JournalParser parser;
        static_cast<void>(parser.feed(line("load: spawning server instance with name=ornith on port 46205")));
        static_cast<void>(parser.feed(line("run: instance name=ornith exited with status 137")));
        const std::optional<InstanceCrash> crash = parser.takeCrash();
        QVERIFY(crash.has_value());
        QCOMPARE(crash->reason, QStringLiteral("exit status 137"));
        QCOMPARE(crash->when, QStringLiteral("15:54:10"));
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
