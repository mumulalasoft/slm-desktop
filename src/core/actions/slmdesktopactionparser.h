#pragma once

#include "slmactiontypes.h"

#include <QString>
#include <QVector>

namespace Slm::Actions {

struct DesktopParseResult {
    bool ok = false;
    QString error;
    QVector<FileAction> actions;
};

class DesktopActionParser
{
public:
    DesktopParseResult parseFile(const QString &desktopFilePath) const;
};

} // namespace Slm::Actions

