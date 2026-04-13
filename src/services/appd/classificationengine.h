#pragma once

#include "appentry.h"

#include <QHash>
#include <QString>
#include <QVariantMap>

namespace Slm::Appd {

// ClassificationEngine — resolves AppCategory for a given app.
//
// Resolution priority:
//  1. Explicit override config (~/.config/desktop-appd/overrides.json)
//  2. .desktop metadata (X-KDE-*, Categories)
//  3. Toolkit detection (env var presence)
//  4. Parent process identity (terminal → cli-app)
//  5. Window existence (has window = GUI)
//  6. Fallback (unknown)
class ClassificationEngine
{
public:
    ClassificationEngine();

    // Load per-app overrides from ~/.config/desktop-appd/overrides.json.
    void loadOverrides();

    // Classify an app given its identity hints.
    AppCategory classify(const QString &appId,
                         const QString &desktopFile,
                         const QString &executable,
                         const QStringList &desktopCategories,
                         bool hasWindow,
                         qint64 parentPid) const;

private:
    AppCategory classifyFromOverride(const QString &appId) const;
    AppCategory classifyFromDesktop(const QStringList &categories,
                                    const QString &desktopFile) const;
    AppCategory classifyFromToolkit(const QString &executable) const;
    AppCategory classifyFromParentPid(qint64 parentPid) const;

    static bool isTerminal(const QString &executable);
    static QStringList readDesktopCategories(const QString &desktopFile);

    QHash<QString, AppCategory> m_overrides;
};

} // namespace Slm::Appd
