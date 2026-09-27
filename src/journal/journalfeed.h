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
    };

    [[nodiscard]] Changes take(const QStringList &lines);
    void reset();

    // Newest first, at most kMaxRequests: {when, model, promptTokens,
    // promptTps, promptSeconds, genTokens, genTps, genSeconds}.
    [[nodiscard]] const QVariantList &requests() const { return m_requests; }
    // Every request completed this boot, including those beyond the table.
    [[nodiscard]] int requestTotal() const { return m_requestTotal; }
    [[nodiscard]] const QStringList &log() const { return m_log; }
    [[nodiscard]] QVariantMap facts() const;

    static constexpr qsizetype kMaxRequests = 200;
    static constexpr qsizetype kMaxLogLines = 400;

private:
    bool feedParser(const QString &line);

    JournalParser m_parser;
    // Phase-one lines. The backlog replays the tail of the same journal, and
    // these exact lines (timestamp and all) must not be parsed twice.
    QSet<QString> m_history;
    QVariantList m_requests;
    QStringList m_log;
    int m_requestTotal = 0;
    bool m_following = false;
};

} // namespace QindaLlamaWatch
