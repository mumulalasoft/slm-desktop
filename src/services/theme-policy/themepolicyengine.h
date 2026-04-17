#pragma once

#include "appthemeclassifier.h"

#include <QVariantMap>

namespace Slm::ThemePolicy {

struct ThemeResolveInput {
    QVariantMap settings;
    bool qtKdeCompatible = false;
};

struct ThemeResolveOutput {
    AppThemeCategory category = AppThemeCategory::Unknown;
    QString effectiveMode;
    QString themeSource;
    QString iconSource;
    QString note;
};

class ThemePolicyEngine
{
public:
    static ThemeResolveOutput resolve(AppThemeCategory category,
                                      const ThemeResolveInput &input);
    static QVariantMap toVariantMap(const ThemeResolveOutput &output);

private:
    static QString effectiveMode(const QVariantMap &settings);
    static QString routedValue(const QVariantMap &group, const QString &mode,
                               const QString &darkKey, const QString &lightKey);
};

} // namespace Slm::ThemePolicy

