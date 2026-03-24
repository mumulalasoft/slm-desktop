#pragma once

#include <QString>
#include <QVariantMap>

namespace SlmPortalValidation {

inline bool isValidFileChooserMode(const QString &mode)
{
    return mode == QStringLiteral("open")
        || mode == QStringLiteral("save")
        || mode == QStringLiteral("folder");
}

inline bool hasPickFolderConflicts(const QVariantMap &options)
{
    const QString mode = options.value(QStringLiteral("mode")).toString().trimmed();
    const bool modeConflict = !mode.isEmpty() && mode != QStringLiteral("folder");
    const bool selectFoldersConflict =
        options.contains(QStringLiteral("selectFolders")) &&
        !options.value(QStringLiteral("selectFolders")).toBool();
    const bool multipleConflict =
        options.contains(QStringLiteral("multiple")) &&
        options.value(QStringLiteral("multiple")).toBool();
    return modeConflict || selectFoldersConflict || multipleConflict;
}

}

