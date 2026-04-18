#pragma once

#include "cleanertypes.h"

#include <QString>

namespace Slm::Cleaner {

class CleanerPolicyStore
{
public:
    CleanerPolicy load() const;
    bool save(const CleanerPolicy &policy) const;

private:
    QString policyPath() const;
};

} // namespace Slm::Cleaner

