#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace Slm::ThemePolicy {

enum class AppThemeCategory {
    Shell,
    Gtk,
    Kde,
    QtGeneric,
    QtDesktopFallback,
    Unknown,
};

struct AppDescriptor {
    QString appId;
    QString desktopFileId;
    QString executable;
    QStringList categories;
    QStringList desktopKeys;
};

struct ClassificationRules {
    QHash<QString, AppThemeCategory> byAppId;
    QHash<QString, AppThemeCategory> byDesktopFileId;
    QHash<QString, AppThemeCategory> byExecutable;
};

class AppThemeClassifier
{
public:
    static AppThemeCategory classify(const AppDescriptor &app,
                                     const ClassificationRules &rules = {});
    static QString categoryToString(AppThemeCategory category);
    static AppThemeCategory categoryFromString(const QString &value);
};

} // namespace Slm::ThemePolicy
