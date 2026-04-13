#include "themepolicyengine.h"

namespace Slm::ThemePolicy {

QString ThemePolicyEngine::effectiveMode(const QVariantMap &settings)
{
    const QString globalMode = settings.value(QStringLiteral("globalAppearance")).toMap()
                                   .value(QStringLiteral("colorMode"))
                                   .toString()
                                   .trimmed()
                                   .toLower();
    if (globalMode == QStringLiteral("light") || globalMode == QStringLiteral("dark")) {
        return globalMode;
    }
    // Auto mode fallback: keep deterministic baseline for now.
    return QStringLiteral("dark");
}

QString ThemePolicyEngine::routedValue(const QVariantMap &group, const QString &mode,
                                       const QString &darkKey, const QString &lightKey)
{
    const QString key = mode == QStringLiteral("light") ? lightKey : darkKey;
    return group.value(key).toString().trimmed();
}

ThemeResolveOutput ThemePolicyEngine::resolve(AppThemeCategory category,
                                              const ThemeResolveInput &input)
{
    ThemeResolveOutput out;
    out.category = category;
    out.effectiveMode = effectiveMode(input.settings);

    const QVariantMap shell = input.settings.value(QStringLiteral("shellTheme")).toMap();
    const QVariantMap gtk = input.settings.value(QStringLiteral("gtkThemeRouting")).toMap();
    const QVariantMap kde = input.settings.value(QStringLiteral("kdeThemeRouting")).toMap();
    const QVariantMap appPolicy = input.settings.value(QStringLiteral("appThemePolicy")).toMap();
    const QVariantMap fallback = input.settings.value(QStringLiteral("fallbackPolicy")).toMap();

    auto applyGtk = [&]() {
        out.themeSource = routedValue(gtk, out.effectiveMode, QStringLiteral("themeDark"), QStringLiteral("themeLight"));
        out.iconSource = routedValue(gtk, out.effectiveMode, QStringLiteral("iconThemeDark"), QStringLiteral("iconThemeLight"));
        out.note = QStringLiteral("gtk-routing");
    };
    auto applyKde = [&]() {
        out.themeSource = routedValue(kde, out.effectiveMode, QStringLiteral("themeDark"), QStringLiteral("themeLight"));
        out.iconSource = routedValue(kde, out.effectiveMode, QStringLiteral("iconThemeDark"), QStringLiteral("iconThemeLight"));
        out.note = QStringLiteral("kde-routing");
    };
    auto applyDesktopFallback = [&]() {
        out.themeSource = routedValue(fallback, out.effectiveMode,
                                      QStringLiteral("desktopFallbackThemeDark"),
                                      QStringLiteral("desktopFallbackThemeLight"));
        out.iconSource = routedValue(fallback, out.effectiveMode,
                                     QStringLiteral("desktopFallbackIconDark"),
                                     QStringLiteral("desktopFallbackIconLight"));
        out.note = QStringLiteral("desktop-fallback");
    };

    switch (category) {
    case AppThemeCategory::Shell:
        out.themeSource = shell.value(QStringLiteral("name")).toString().trimmed();
        out.iconSource = out.effectiveMode == QStringLiteral("light")
                             ? fallback.value(QStringLiteral("desktopFallbackIconLight")).toString().trimmed()
                             : fallback.value(QStringLiteral("desktopFallbackIconDark")).toString().trimmed();
        out.note = QStringLiteral("shell-theme-only");
        break;
    case AppThemeCategory::Gtk:
        applyGtk();
        break;
    case AppThemeCategory::Kde:
        applyKde();
        break;
    case AppThemeCategory::QtGeneric: {
        const bool allowKdeCompat = appPolicy.value(QStringLiteral("qtGenericAllowKdeCompat"), true).toBool();
        const bool forceDesktopFallback = appPolicy.value(QStringLiteral("qtIncompatibleUseDesktopFallback"), true).toBool();
        if (allowKdeCompat && input.qtKdeCompatible) {
            applyKde();
            out.note = QStringLiteral("qt-generic-kde-compatible");
        } else if (forceDesktopFallback) {
            out.category = AppThemeCategory::QtDesktopFallback;
            applyDesktopFallback();
            out.note = QStringLiteral("qt-generic-desktop-fallback");
        } else {
            applyKde();
            out.note = QStringLiteral("qt-generic-kde-policy");
        }
        break;
    }
    case AppThemeCategory::QtDesktopFallback:
        applyDesktopFallback();
        break;
    case AppThemeCategory::Unknown:
    default: {
        const bool unknownUseFallback = fallback.value(QStringLiteral("unknownUsesSafeFallback"), true).toBool();
        if (unknownUseFallback) {
            out.category = AppThemeCategory::QtDesktopFallback;
            applyDesktopFallback();
            out.note = QStringLiteral("unknown-safe-fallback");
        } else {
            applyKde();
            out.note = QStringLiteral("unknown-kde-default");
        }
        break;
    }
    }

    if (out.themeSource.isEmpty()) {
        applyDesktopFallback();
        out.note = QStringLiteral("resolved-empty-theme-fallback");
    }
    if (out.iconSource.isEmpty()) {
        out.iconSource = out.effectiveMode == QStringLiteral("light")
                             ? fallback.value(QStringLiteral("desktopFallbackIconLight")).toString().trimmed()
                             : fallback.value(QStringLiteral("desktopFallbackIconDark")).toString().trimmed();
    }
    return out;
}

QVariantMap ThemePolicyEngine::toVariantMap(const ThemeResolveOutput &output)
{
    return {
        {QStringLiteral("category"), AppThemeClassifier::categoryToString(output.category)},
        {QStringLiteral("effectiveMode"), output.effectiveMode},
        {QStringLiteral("themeSource"), output.themeSource},
        {QStringLiteral("iconSource"), output.iconSource},
        {QStringLiteral("note"), output.note},
    };
}

} // namespace Slm::ThemePolicy

