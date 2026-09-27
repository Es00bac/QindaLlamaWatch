// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QString>

namespace QindaLlamaWatch {

// Prometheus text exposition (llama-server --metrics) -> {name: value} with
// the "llamacpp:" prefix and any labels dropped, e.g.
// "llamacpp:tokens_predicted_total 1234" -> {"tokens_predicted_total": 1234}.
[[nodiscard]] QHash<QString, double> parsePrometheus(const QString &text);

} // namespace QindaLlamaWatch
