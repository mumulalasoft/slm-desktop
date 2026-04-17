#pragma once

#include "envtypes.h"
#include "../../apps/settings/modules/developer/enventry.h"

#include <QList>
#include <QPair>

// MergeResolver — stateless, pure-function merge of environment layers.
//
// Each layer is submitted as a list of EnvEntry items together with the
// EnvLayer tag that identifies its priority.  The resolver processes layers
// in ascending priority order.  For each entry in a layer:
//
//   replace  — overwrites any existing value from a lower-priority layer
//   prepend  — "<this-value>:<previous-value>" (or just "<this-value>" if empty)
//   append   — "<previous-value>:<this-value>" (or just "<this-value>" if empty)
//
// Disabled entries (enabled == false) are skipped.
//
class MergeResolver
{
public:
    using Layer = QPair<EnvLayer, QList<EnvEntry>>;

    // Merge an ordered list of layers and return the resolved environment.
    // Layers do not need to be pre-sorted — they are sorted internally.
    static ResolvedEnv resolve(const QList<Layer> &layers);

    // Convenience: add a single entry on top of an existing resolved env.
    // Useful for per-exec launch-time overrides without re-resolving everything.
    static void applyEntry(ResolvedEnv &env, const EnvEntry &entry, EnvLayer layer);
};
