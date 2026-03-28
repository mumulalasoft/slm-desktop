#pragma once

#include <QList>
#include <QString>
#include <QVariantList>

#include "dependencyguard.h"

namespace Slm::System {

class ComponentRegistry
{
public:
    static QList<ComponentRequirement> all();
    static QList<ComponentRequirement> forDomain(const QString &domain);
    static QVariantList missingForDomain(const QString &domain);
    static bool findById(const QString &componentId, ComponentRequirement *out);
};

} // namespace Slm::System
