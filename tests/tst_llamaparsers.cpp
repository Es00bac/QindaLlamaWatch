// SPDX-License-Identifier: GPL-3.0-or-later
//
// /metrics parsing, live slot rates, and the ssh command builder's quoting.

#include "common/remotecommand.h"
#include "llama/metricsparser.h"
#include "llama/slotrates.h"

#include <QtTest>

using namespace QindaLlamaWatch;

class TestLlamaParsers : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesPrometheusText()
    {
        const QHash<QString, double> m = parsePrometheus(QStringLiteral(
            "# HELP llamacpp:prompt_tokens_total Number of prompt tokens processed.\n"
            "# TYPE llamacpp:prompt_tokens_total counter\n"
            "llamacpp:prompt_tokens_total 4821933\n"
            "llamacpp:tokens_predicted_seconds_total 5080.25\n"
            "llamacpp:requests_processing{slot=\"0\"} 1\n"
            "garbage\n"));
        QCOMPARE(m.value(QStringLiteral("prompt_tokens_total")), 4821933.0);
        QCOMPARE(m.value(QStringLiteral("tokens_predicted_seconds_total")), 5080.25);
        QCOMPARE(m.value(QStringLiteral("requests_processing")), 1.0);
        QCOMPARE(m.size(), qsizetype(3));
    }

    void readsSlotJsonInBothNextTokenShapes()
    {
        QVariantMap slot{{QStringLiteral("id"), 1}, {QStringLiteral("id_task"), 7},
                         {QStringLiteral("is_processing"), true},
                         {QStringLiteral("n_prompt_tokens"), 100},
                         {QStringLiteral("n_ctx"), 4096},
                         {QStringLiteral("next_token"),
                          QVariantList{QVariantMap{{QStringLiteral("n_decoded"), 12}}}}};
        QCOMPARE(slotFromJson(slot).decoded, 12LL);
        slot.insert(QStringLiteral("next_token"), QVariantMap{{QStringLiteral("n_decoded"), 13}});
        QCOMPARE(slotFromJson(slot).decoded, 13LL);
        const QVariantMap qml = slotToQml(slotFromJson(slot));
        QCOMPARE(qml.value(QStringLiteral("phase")).toString(), QStringLiteral("generate"));
        QCOMPARE(qml.value(QStringLiteral("used")).toLongLong(), 113LL);
    }

    void ratesCountOnlyTheSameTask()
    {
        SlotRates rates;
        SlotView slot;
        slot.id = 0;
        slot.task = 7;
        slot.busy = true;
        slot.decoded = 100;
        static_cast<void>(rates.update({slot}, 10.0));

        slot.decoded = 150;
        SlotRates::Rates r = rates.update({slot}, 11.0);
        QCOMPARE(r.generate, 50.0);

        // A new task restarts n_decoded at a small number: not a rate.
        slot.task = 8;
        slot.decoded = 5;
        r = rates.update({slot}, 12.0);
        QCOMPARE(r.generate, 0.0);
    }

    void promptRateIsHeldBetweenBatchCompletions()
    {
        // llama-server advances the processed count one 2,048-token batch at
        // a time; qinda took ~15 s per batch at long context.
        SlotRates rates;
        SlotView slot;
        slot.id = 1;
        slot.task = 40651;
        slot.busy = true;
        slot.promptDone = 30840;
        static_cast<void>(rates.update({slot}, 100.0));        // first sight: already mid-read
        QCOMPARE(rates.update({slot}, 101.0).prompt, 0.0);      // nothing measured yet

        // The first advance covers only the part of a batch since first
        // sight (5 s of a 15 s batch here): it starts the clock, no rate.
        slot.promptDone += 2048;
        QCOMPARE(rates.update({slot}, 105.0).prompt, 0.0);
        slot.promptDone += 2048;
        QCOMPARE(rates.update({slot}, 115.0).prompt, 2048.0 / 10.0);
        slot.promptDone -= 2048;                                // (rewind for the checks below)
        rates.reset();
        static_cast<void>(rates.update({slot}, 90.0));
        slot.promptDone += 2048;
        static_cast<void>(rates.update({slot}, 100.0));        // clock started
        slot.promptDone += 2048;
        QCOMPARE(rates.update({slot}, 115.0).prompt, 2048.0 / 15.0);
        // no batch completed in the next polls: the rate is held, not 0
        QCOMPARE(rates.update({slot}, 116.0).prompt, 2048.0 / 15.0);
        QCOMPARE(rates.update({slot}, 129.0).prompt, 2048.0 / 15.0);

        slot.promptDone += 2048;
        QCOMPARE(rates.update({slot}, 131.0).prompt, 2048.0 / 16.0);

        // generation started: prompt reading is over
        slot.decoded = 3;
        QCOMPARE(rates.update({slot}, 132.0).prompt, 0.0);
    }

    void heldPromptRateExpiresWhenReadingStalls()
    {
        SlotRates rates;
        SlotView slot;
        slot.id = 0;
        slot.task = 7;
        slot.busy = true;
        static_cast<void>(rates.update({slot}, 0.0));
        slot.promptDone = 2048;
        QCOMPARE(rates.update({slot}, 10.0).prompt, 204.8);
        QCOMPARE(rates.update({slot}, 39.0).prompt, 204.8);     // within 3 x 10 s
        QCOMPARE(rates.update({slot}, 41.0).prompt, 0.0);       // stalled: dropped
    }

    void quotesEveryRemoteArgument()
    {
        const RemoteCommand c = remoteCommand(QStringLiteral("qinda"),
                                              {QStringLiteral("journalctl"), QStringLiteral("-u"),
                                               QStringLiteral("x'; rm -rf ~; '")});
        QCOMPARE(c.program, QStringLiteral("ssh"));
        QCOMPARE(c.arguments.last(), QStringLiteral("'journalctl' '-u' 'x'\\''; rm -rf ~; '\\'''"));
        QVERIFY(c.arguments.contains(QStringLiteral("BatchMode=yes")));
    }

    void localAndEmptyHosts()
    {
        const RemoteCommand local = remoteCommand(QStringLiteral("local"),
                                                  {QStringLiteral("sh"), QStringLiteral("-s")});
        QCOMPARE(local.program, QStringLiteral("sh"));
        QCOMPARE(local.arguments, QStringList{QStringLiteral("-s")});
        QVERIFY(remoteCommand(QString(), {QStringLiteral("sh")}).program.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLlamaParsers)
#include "tst_llamaparsers.moc"
