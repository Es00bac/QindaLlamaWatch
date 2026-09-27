// SPDX-License-Identifier: GPL-3.0-or-later

#include "llama/metricsparser.h"

#include <QStringList>

namespace QindaLlamaWatch {

QHash<QString, double> parsePrometheus(const QString &text)
{
    QHash<QString, double> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            continue;
        }
        QString name = parts.first().section(QLatin1Char('{'), 0, 0);
        if (const qsizetype colon = name.indexOf(QLatin1Char(':')); colon >= 0) {
            name = name.mid(colon + 1);
        }
        bool ok = false;
        const double value = parts.at(1).toDouble(&ok);
        if (ok) {
            out.insert(name, value);
        }
    }
    return out;
}

} // namespace QindaLlamaWatch
