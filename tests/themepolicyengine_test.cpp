#include <QtTest/QtTest>

#include "../src/services/theme-policy/themepolicyengine.h"

using namespace Slm::ThemePolicy;

namespace {
QVariantMap baseSettings()
{
    return {
        {QStringLiteral("globalAppearance"), QVariantMap{
             {QStringLiteral("colorMode"), QStringLiteral("dark")},
         }},
        {QStringLiteral("shellTheme"), QVariantMap{
             {QStringLiteral("name"), QStringLiteral("slm")},
         }},
        {QStringLiteral("gtkThemeRouting"), QVariantMap{
             {QStringLiteral("themeDark"), QStringLiteral("SLM-GTK-Dark")},
             {QStringLiteral("themeLight"), QStringLiteral("SLM-GTK-Light")},
             {QStringLiteral("iconThemeDark"), QStringLiteral("SLM-Icons-Dark")},
             {QStringLiteral("iconThemeLight"), QStringLiteral("SLM-Icons-Light")},
         }},
        {QStringLiteral("kdeThemeRouting"), QVariantMap{
             {QStringLiteral("themeDark"), QStringLiteral("SLM-KDE-Dark")},
             {QStringLiteral("themeLight"), QStringLiteral("SLM-KDE-Light")},
             {QStringLiteral("iconThemeDark"), QStringLiteral("SLM-Icons-Dark")},
             {QStringLiteral("iconThemeLight"), QStringLiteral("SLM-Icons-Light")},
         }},
        {QStringLiteral("appThemePolicy"), QVariantMap{
             {QStringLiteral("qtGenericAllowKdeCompat"), true},
             {QStringLiteral("qtIncompatibleUseDesktopFallback"), true},
         }},
        {QStringLiteral("fallbackPolicy"), QVariantMap{
             {QStringLiteral("unknownUsesSafeFallback"), true},
             {QStringLiteral("desktopFallbackThemeDark"), QStringLiteral("slm-dark")},
             {QStringLiteral("desktopFallbackThemeLight"), QStringLiteral("slm-light")},
             {QStringLiteral("desktopFallbackIconDark"), QStringLiteral("SLM-Icons-Dark")},
             {QStringLiteral("desktopFallbackIconLight"), QStringLiteral("SLM-Icons-Light")},
         }},
    };
}
} // namespace

class ThemePolicyEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void resolves_shell_theme()
    {
        ThemeResolveInput in;
        in.settings = baseSettings();
        const ThemeResolveOutput out = ThemePolicyEngine::resolve(AppThemeCategory::Shell, in);
        QCOMPARE(out.themeSource, QStringLiteral("slm"));
        QCOMPARE(out.effectiveMode, QStringLiteral("dark"));
    }

    void resolves_gtk_routing()
    {
        ThemeResolveInput in;
        in.settings = baseSettings();
        const ThemeResolveOutput out = ThemePolicyEngine::resolve(AppThemeCategory::Gtk, in);
        QCOMPARE(out.themeSource, QStringLiteral("SLM-GTK-Dark"));
        QCOMPARE(out.iconSource, QStringLiteral("SLM-Icons-Dark"));
    }

    void resolves_qt_generic_fallback_when_incompatible()
    {
        ThemeResolveInput in;
        in.settings = baseSettings();
        in.qtKdeCompatible = false;
        const ThemeResolveOutput out = ThemePolicyEngine::resolve(AppThemeCategory::QtGeneric, in);
        QCOMPARE(out.category, AppThemeCategory::QtDesktopFallback);
        QCOMPARE(out.themeSource, QStringLiteral("slm-dark"));
    }

    void resolves_unknown_to_safe_fallback()
    {
        ThemeResolveInput in;
        in.settings = baseSettings();
        const ThemeResolveOutput out = ThemePolicyEngine::resolve(AppThemeCategory::Unknown, in);
        QCOMPARE(out.category, AppThemeCategory::QtDesktopFallback);
        QCOMPARE(out.iconSource, QStringLiteral("SLM-Icons-Dark"));
    }
};

QTEST_MAIN(ThemePolicyEngineTest)
#include "themepolicyengine_test.moc"

