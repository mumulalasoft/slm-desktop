#pragma once

#include <QVariantMap>

namespace Slm::Context {

class RuleEngine
{
public:
    QVariantMap evaluate(const QVariantMap &context, const QVariantMap &settingsSnapshot) const;
};

} // namespace Slm::Context

