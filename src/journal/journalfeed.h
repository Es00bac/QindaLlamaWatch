// SPDX-License-Identifier: GPL-3.0-or-later
//
// What ServerJournal does with the lines of one journal session, as a pure
// class: the session prints the grep-filtered boot history (phase one), the
// follow marker, then the usual backlog and live lines (phase two). Phase
// one feeds the parser and the request table; phase two feeds the log view
// and adds only lines phase one has not already counted.

#pragma once

#include "journal/journalparser.h"

#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace QindaLlamaWatch {

class JournalFeed final
{
public:
    // AGENT-CONTRACT: printed by the session script in ServerJournal::
    // configure() between phase one and phase two.
    static constexpr const char *kFollowMarker = "@@follow";

    struct Changes {
        bool requests = false;
        bool facts = false;
        bool log = false;
        bool crashes = false;
        bool progress = false;
        // Crashes seen live (phase two), oldest first: these alert the user.
        // Crashes found in the boot history only update the count and list.
        QVariantList newCrashes;
    };

    [[nodiscard]] Changes take(const QStringList &lines);
    void reset();

    // Newest first, at most kMaxRequests: {when, model, promptTokens,
    // promptTps, promptSeconds, genTokens, genTps, genSeconds}.
    [[nodiscard]] const QVariantList &requests() const { return m_requests; }
    // Every request completed this boot, including those beyond the table.
    [[nodiscard]] int requestTotal() const { return m_requestTotal; }
    [[nodiscard]] const QStringList &log() const { return m_log; }
    // Newest first, at most kMaxCrashes: {when, model, status, reason}.
    [[nodiscard]] const QVariantList &crashes() const { return m_crashes; }
    [[nodiscard]] int crashTotal() const { return m_crashTotal; }
    [[nodiscard]] QVariantMap facts() const;
    // AGENT-CONTRACT: read by qml/SlotsPanel.qml. Slot id (as a string) ->
    // {task, tokens, fraction, total (tokens / fraction, estimated), when};
    // the panel uses an entry only when its task matches the slot's id_task.
    [[nodiscard]] QVariantMap progress() const;

    static constexpr qsizetype kMaxRequests = 200;
    static constexpr qsizetype kMaxLogLines = 400;
    static constexpr qsizetype kMaxCrashes = 50;

private:
    // Feeds one line; records a completed request or crash. `live` marks a
    // crash for alerting.
    void feedParser(const QString &line, bool live, Changes &changes);

    JournalParser m_parser;
    // Phase-one lines. The backlog replays the tail of the same journal, and
    // these exact lines (timestamp and all) must not be parsed twice.
    QSet<QString> m_history;
    QVariantList m_requests;
    QStringList m_log;
    QVariantList m_crashes;
    int m_requestTotal = 0;
    int m_crashTotal = 0;
    bool m_following = false;
};

} // namespace QindaLlamaWatch
