// SPDX-License-Identifier: GPL-3.0-or-later
//
// The two-phase journal session: boot history (grep-filtered), the follow
// marker, then a backlog that overlaps the history, then live lines.

#include "journal/journalfeed.h"

#include <QtTest>

using namespace QindaLlamaWatch;

namespace {

QString line(const char *time, const char *message)
{
    return QStringLiteral("2026-09-27T%1-0600 qinda llama-server[1217]: %2")
        .arg(QString::fromLatin1(time), QString::fromLatin1(message));
}

const char *const kSpawn = "load: spawning server instance with name=ornith on port 46205";
const char *const kPrompt1 = "[46205] 1.34.800.603 I slot print_timing: id  1 | task 0 | prompt eval time =   55552.00 ms /   939 tokens (   59.16 ms per token,    16.90 tokens per second)";
const char *const kEval1 = "[46205] 1.34.800.606 I slot print_timing: id  1 | task 0 |        eval time =    9032.36 ms /   108 tokens (   84.41 ms per token,    11.85 tokens per second)";
const char *const kPrompt2 = "[46205] 2.10.000.001 I slot print_timing: id  1 | task 9 | prompt eval time =   700.00 ms /   20 tokens (   35.00 ms per token,    28.57 tokens per second)";
const char *const kEval2 = "[46205] 2.10.000.002 I slot print_timing: id  1 | task 9 |        eval time =    1000.00 ms /   17 tokens (   58.82 ms per token,    17.00 tokens per second)";
const char *const kProgress = "[46205] 2.20.000.000 I slot update_slots: id  1 | task 12 | prompt processing progress, n_tokens = 4096";

} // namespace

class TestJournalFeed : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void historyRequestsSurviveABacklogThatNoLongerHoldsThem()
    {
        JournalFeed feed;
        // Phase one: the whole boot, filtered. Phase two's backlog is all
        // progress noise, as during a long prompt read.
        const JournalFeed::Changes changes = feed.take({
            line("15:38:39", kSpawn), line("15:40:13", kPrompt1), line("15:40:13", kEval1),
            QString::fromLatin1(JournalFeed::kFollowMarker),
            line("16:40:00", kProgress), line("16:40:01", kProgress)});
        QVERIFY(changes.requests);
        QCOMPARE(feed.requestTotal(), 1);
        QCOMPARE(feed.requests().first().toMap().value(QStringLiteral("model")).toString(),
                 QStringLiteral("ornith"));
        // Only phase two reaches the log view.
        QCOMPARE(feed.log().size(), qsizetype(2));
    }

    void backlogOverlappingHistoryIsNotCountedTwice()
    {
        JournalFeed feed;
        static_cast<void>(feed.take({
            line("15:38:39", kSpawn), line("15:40:13", kPrompt1), line("15:40:13", kEval1),
            line("16:29:43", kPrompt2), line("16:29:43", kEval2),
            QString::fromLatin1(JournalFeed::kFollowMarker)}));
        QCOMPARE(feed.requestTotal(), 2);

        // The backlog replays the newest request exactly as phase one saw it.
        static_cast<void>(feed.take({line("16:29:43", kPrompt2), line("16:29:43", kEval2)}));
        QCOMPARE(feed.requestTotal(), 2);
        QCOMPARE(feed.requests().size(), qsizetype(2));
        QCOMPARE(feed.log().size(), qsizetype(2));

        // A request completed after the history was read is new.
        const JournalFeed::Changes live = feed.take({line("16:45:00", kPrompt2), line("16:45:02", kEval2)});
        QVERIFY(live.requests);
        QCOMPARE(feed.requestTotal(), 3);
        // A request is stamped with its prompt line's time, as in llm-top.
        QCOMPARE(feed.requests().first().toMap().value(QStringLiteral("when")).toString(),
                 QStringLiteral("16:45:00"));
    }

    void promptInHistoryPairsWithEvalArrivingLive()
    {
        JournalFeed feed;
        static_cast<void>(feed.take({line("15:38:39", kSpawn), line("16:50:00", kPrompt2),
                                     QString::fromLatin1(JournalFeed::kFollowMarker)}));
        QCOMPARE(feed.requestTotal(), 0);
        static_cast<void>(feed.take({line("16:50:00", kPrompt2), line("16:50:03", kEval2)}));
        QCOMPARE(feed.requestTotal(), 1);
    }

    void tableKeepsTheNewestButTotalCountsAll()
    {
        JournalFeed feed;
        QStringList history{line("15:38:39", kSpawn)};
        for (int i = 0; i < 250; ++i) {
            history << line("16:00:00", kPrompt2) + QString::number(i)
                    << line("16:00:01", kEval2) + QString::number(i);
        }
        history << QString::fromLatin1(JournalFeed::kFollowMarker);
        static_cast<void>(feed.take(history));
        QCOMPARE(feed.requestTotal(), 250);
        QCOMPARE(feed.requests().size(), JournalFeed::kMaxRequests);
    }

    void resetForgetsTheSession()
    {
        JournalFeed feed;
        static_cast<void>(feed.take({line("15:40:13", kPrompt1), line("15:40:13", kEval1),
                                     QString::fromLatin1(JournalFeed::kFollowMarker)}));
        feed.reset();
        QCOMPARE(feed.requestTotal(), 0);
        QVERIFY(feed.requests().isEmpty());
        // After a reset the marker is expected again: lines are history.
        static_cast<void>(feed.take({line("16:00:00", kProgress)}));
        QVERIFY(feed.log().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestJournalFeed)
#include "tst_journalfeed.moc"
