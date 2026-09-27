// SPDX-License-Identifier: GPL-3.0-or-later

#include "journal/journalfeed.h"

namespace QindaLlamaWatch {

namespace {

QVariantMap toMap(const CompletedRequest &r)
{
    return {
        {QStringLiteral("when"), r.when},
        {QStringLiteral("model"), r.model},
        {QStringLiteral("promptTokens"), r.promptTokens},
        {QStringLiteral("promptTps"), r.promptTps},
        {QStringLiteral("promptSeconds"), r.promptMs / 1000.0},
        {QStringLiteral("genTokens"), r.genTokens},
        {QStringLiteral("genTps"), r.genTps},
        {QStringLiteral("genSeconds"), r.genMs / 1000.0},
    };
}

QVariantMap toMap(const InstanceCrash &c)
{
    return {
        {QStringLiteral("when"), c.when},
        {QStringLiteral("model"), c.model},
        {QStringLiteral("status"), c.status},
        {QStringLiteral("reason"), c.reason},
    };
}

} // namespace

void JournalFeed::reset()
{
    m_parser.reset();
    m_history.clear();
    m_requests.clear();
    m_log.clear();
    m_crashes.clear();
    m_requestTotal = 0;
    m_crashTotal = 0;
    m_following = false;
}

QVariantMap JournalFeed::facts() const
{
    QVariantMap map;
    const QHash<QString, QString> &facts = m_parser.facts();
    for (auto it = facts.cbegin(); it != facts.cend(); ++it) {
        map.insert(it.key(), it.value());
    }
    return map;
}

void JournalFeed::feedParser(const QString &line, bool live, Changes &changes)
{
    if (std::optional<CompletedRequest> request = m_parser.feed(line)) {
        m_requests.prepend(toMap(*request));
        ++m_requestTotal;
        changes.requests = true;
    }
    if (std::optional<InstanceCrash> crash = m_parser.takeCrash()) {
        const QVariantMap map = toMap(*crash);
        m_crashes.prepend(map);
        if (m_crashes.size() > kMaxCrashes) {
            m_crashes.resize(kMaxCrashes);
        }
        ++m_crashTotal;
        changes.crashes = true;
        if (live) {
            changes.newCrashes.append(map);
        }
    }
}

JournalFeed::Changes JournalFeed::take(const QStringList &lines)
{
    Changes changes;
    const QHash<QString, QString> factsBefore = m_parser.facts();   // at most 7 entries
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("-- "))) {
            continue;                         // "-- No entries --", "-- Boot ... --"
        }
        if (!m_following) {
            if (line == QLatin1String(kFollowMarker)) {
                m_following = true;
            } else {
                m_history.insert(line);
                feedParser(line, false, changes);
            }
            continue;
        }
        m_log.append(line);
        changes.log = true;
        // AGENT-GUARD: a line phase one already parsed would add its request
        // twice, or re-open a prompt that was already paired.
        if (!m_history.contains(line)) {
            // AGENT-NOTE: "live" includes backlog lines newer than the
            // history read, which is a window of well under a second.
            feedParser(line, true, changes);
        }
    }
    if (m_log.size() > kMaxLogLines) {
        m_log.remove(0, m_log.size() - kMaxLogLines);
    }
    if (m_requests.size() > kMaxRequests) {
        m_requests.resize(kMaxRequests);
    }
    changes.facts = m_parser.facts() != factsBefore;
    return changes;
}

} // namespace QindaLlamaWatch
