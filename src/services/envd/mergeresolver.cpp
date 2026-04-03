#include "mergeresolver.h"

#include <algorithm>

ResolvedEnv MergeResolver::resolve(const QList<Layer> &layers)
{
    // Sort a copy so callers don't need to pre-order.
    QList<Layer> sorted = layers;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Layer &a, const Layer &b) {
                         return static_cast<int>(a.first) < static_cast<int>(b.first);
                     });

    ResolvedEnv env;
    for (const Layer &layer : sorted) {
        for (const EnvEntry &entry : layer.second) {
            applyEntry(env, entry, layer.first);
        }
    }
    return env;
}

void MergeResolver::applyEntry(ResolvedEnv &env, const EnvEntry &entry, EnvLayer layer)
{
    if (!entry.enabled || entry.key.isEmpty())
        return;

    const QString mode = entry.mergeMode.isEmpty()
                             ? QStringLiteral("replace")
                             : entry.mergeMode;

    if (mode == QLatin1String("prepend")) {
        const QString prev = env.contains(entry.key) ? env[entry.key].value : QString{};
        const QString merged = prev.isEmpty()
                                   ? entry.value
                                   : entry.value + QLatin1Char(':') + prev;
        env.insert(entry.key, ResolvedVar{merged, layer});
    } else if (mode == QLatin1String("append")) {
        const QString prev = env.contains(entry.key) ? env[entry.key].value : QString{};
        const QString merged = prev.isEmpty()
                                   ? entry.value
                                   : prev + QLatin1Char(':') + entry.value;
        env.insert(entry.key, ResolvedVar{merged, layer});
    } else {
        // "replace" (default) — unconditional overwrite
        env.insert(entry.key, ResolvedVar{entry.value, layer});
    }
}
