#include "uipreferences.h"
#include <QCoreApplication>
#include <QtGlobal>

namespace {
constexpr auto kMotionPresetKey = "dock/motionPreset";
constexpr auto kDropPulseEnabledKey = "dock/dropPulseEnabled";
constexpr auto kDockAutoHideEnabledKey = "dock/autoHideEnabled";
constexpr auto kDockHideModeKey = "dock/hideMode";
constexpr auto kDockHideDurationMsKey = "dock/hideDurationMs";
constexpr auto kDragThresholdMouseKey = "dock/dragThresholdMousePx";
constexpr auto kDragThresholdTouchpadKey = "dock/dragThresholdTouchpadPx";
constexpr auto kVerboseLoggingKey = "debug/verboseLogging";
constexpr auto kIconThemeLightKey = "iconTheme/light";
constexpr auto kIconThemeDarkKey = "iconTheme/dark";
constexpr auto kWallpaperUriKey = "wallpaper/uri";
constexpr auto kThemeModeKey = "appearance/themeMode";
constexpr auto kAccentColorKey = "appearance/accentColor";
constexpr auto kFontScaleKey = "appearance/fontScale";
constexpr auto kGtkThemeLightKey = "appearance/gtkThemeLight";
constexpr auto kGtkThemeDarkKey = "appearance/gtkThemeDark";
constexpr auto kKdeColorSchemeLightKey = "appearance/kdeColorSchemeLight";
constexpr auto kKdeColorSchemeDarkKey = "appearance/kdeColorSchemeDark";
constexpr auto kGtkIconThemeLightKey = "appearance/gtkIconThemeLight";
constexpr auto kGtkIconThemeDarkKey = "appearance/gtkIconThemeDark";
constexpr auto kKdeIconThemeLightKey = "appearance/kdeIconThemeLight";
constexpr auto kKdeIconThemeDarkKey = "appearance/kdeIconThemeDark";
constexpr auto kDefaultFontKey = "fonts/defaultFont";
constexpr auto kDocumentFontKey = "fonts/documentFont";
constexpr auto kMonospaceFontKey = "fonts/monospaceFont";
constexpr auto kTitlebarFontKey = "fonts/titlebarFont";
constexpr auto kDefaultAccentColor = "#0a84ff";
constexpr auto kWindowBindMinimizeKey = "windowing/bindMinimize";
constexpr auto kWindowBindRestoreKey = "windowing/bindRestore";
constexpr auto kWindowBindSwitchNextKey = "windowing/bindSwitchNext";
constexpr auto kWindowBindSwitchPrevKey = "windowing/bindSwitchPrev";
constexpr auto kWindowBindWorkspaceKey = "windowing/bindWorkspace";
constexpr auto kWindowBindOverviewLegacyKey = "windowing/bindOverview";
constexpr auto kWindowBindCloseKey = "windowing/bindClose";
constexpr auto kWindowBindMaximizeKey = "windowing/bindMaximize";
constexpr auto kWindowBindTileLeftKey = "windowing/bindTileLeft";
constexpr auto kWindowBindTileRightKey = "windowing/bindTileRight";
constexpr auto kWindowBindUntileKey = "windowing/bindUntile";
constexpr auto kWindowBindQuarterTopLeftKey = "windowing/bindQuarterTopLeft";
constexpr auto kWindowBindQuarterTopRightKey = "windowing/bindQuarterTopRight";
constexpr auto kWindowBindQuarterBottomLeftKey = "windowing/bindQuarterBottomLeft";
constexpr auto kWindowBindQuarterBottomRightKey = "windowing/bindQuarterBottomRight";
constexpr auto kWindowShadowEnabledKey = "windowing/shadowEnabled";
constexpr auto kWindowShadowStrengthKey = "windowing/shadowStrength";
constexpr auto kWindowRoundedEnabledKey = "windowing/roundedEnabled";
constexpr auto kWindowRoundedRadiusKey = "windowing/roundedRadius";
constexpr auto kWindowAnimationEnabledKey = "windowing/animationEnabled";
constexpr auto kWindowAnimationSpeedKey = "windowing/animationSpeed";
constexpr auto kWindowSceneFxEnabledKey = "windowing/sceneFxEnabled";
constexpr auto kWindowSceneFxDimAlphaKey = "windowing/sceneFxDimAlpha";
constexpr auto kWindowSceneFxAnimBoostKey = "windowing/sceneFxAnimBoost";
constexpr auto kWindowSceneFxRoundedClipEnabledKey = "windowing/sceneFxRoundedClipEnabled";
constexpr auto kAppScoreLaunchWeightKey = "app/scoreLaunchWeight";
constexpr auto kAppScoreFileOpenWeightKey = "app/scoreFileOpenWeight";
constexpr auto kAppScoreRecencyWeightKey = "app/scoreRecencyWeight";
constexpr auto kTothespotSearchProfileKey = "tothespot/searchProfile";
constexpr auto kWindowBindMinimizeDefault = "Alt+F9";
constexpr auto kWindowBindRestoreDefault = "Alt+F10";
constexpr auto kWindowBindSwitchNextDefault = "Alt+F11";
constexpr auto kWindowBindSwitchPrevDefault = "Alt+F12";
constexpr auto kWindowBindWorkspaceDefault = "Alt+F3";
constexpr auto kWindowBindCloseDefault = "Alt+F4";
constexpr auto kWindowBindMaximizeDefault = "Alt+Up";
constexpr auto kWindowBindTileLeftDefault = "Alt+Left";
constexpr auto kWindowBindTileRightDefault = "Alt+Right";
constexpr auto kWindowBindUntileDefault = "Alt+Down";
constexpr auto kWindowBindQuarterTopLeftDefault = "Alt+Shift+Left";
constexpr auto kWindowBindQuarterTopRightDefault = "Alt+Shift+Right";
constexpr auto kWindowBindQuarterBottomLeftDefault = "Alt+Shift+Down";
constexpr auto kWindowBindQuarterBottomRightDefault = "Alt+Shift+Up";
constexpr auto kWindowShadowEnabledDefault = true;
constexpr auto kWindowShadowStrengthDefault = 100;
constexpr auto kWindowRoundedEnabledDefault = false;
constexpr auto kWindowRoundedRadiusDefault = 10;
constexpr auto kWindowAnimationEnabledDefault = true;
constexpr auto kWindowAnimationSpeedDefault = 100;
constexpr auto kWindowSceneFxEnabledDefault = false;
constexpr auto kWindowSceneFxDimAlphaDefault = 0.38;
constexpr auto kWindowSceneFxAnimBoostDefault = 115;
constexpr auto kWindowSceneFxRoundedClipEnabledDefault = false;
constexpr auto kAppScoreLaunchWeightDefault = 10;
constexpr auto kAppScoreFileOpenWeightDefault = 3;
constexpr auto kAppScoreRecencyWeightDefault = 1;
constexpr auto kTothespotSearchProfileDefault = "balanced";

QString normalizeTothespotSearchProfile(const QString &profile)
{
    const QString v = profile.trimmed().toLower();
    if (v == QStringLiteral("apps-first") ||
        v == QStringLiteral("files-first") ||
        v == QStringLiteral("balanced")) {
        return v;
    }
    return QString::fromLatin1(kTothespotSearchProfileDefault);
}

QString normalizePreferenceKey(const QString &key)
{
    const QString k = key.trimmed();
    if (k.isEmpty()) {
        return QString();
    }
    if (k.compare(QStringLiteral("dock.motionPreset"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kMotionPresetKey);
    }
    if (k.compare(QStringLiteral("dock.dropPulseEnabled"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kDropPulseEnabledKey);
    }
    if (k.compare(QStringLiteral("dock.autoHideEnabled"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kDockAutoHideEnabledKey);
    }
    if (k.compare(QStringLiteral("dock.hideMode"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kDockHideModeKey);
    }
    if (k.compare(QStringLiteral("dock.hideDurationMs"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kDockHideDurationMsKey);
    }
    if (k.compare(QStringLiteral("dock.dragThresholdMousePx"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kDragThresholdMouseKey);
    }
    if (k.compare(QStringLiteral("dock.dragThresholdTouchpadPx"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kDragThresholdTouchpadKey);
    }
    if (k.compare(QStringLiteral("debug.verboseLogging"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kVerboseLoggingKey);
    }
    if (k.compare(QStringLiteral("iconTheme.light"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("icon.theme.light"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kIconThemeLightKey);
    }
    if (k.compare(QStringLiteral("iconTheme.dark"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("icon.theme.dark"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kIconThemeDarkKey);
    }
    if (k.compare(QStringLiteral("windowing.bindMinimize"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.minimize"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindMinimizeKey);
    }
    if (k.compare(QStringLiteral("windowing.bindRestore"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.restore"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindRestoreKey);
    }
    if (k.compare(QStringLiteral("windowing.bindSwitchNext"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.switchNext"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.switch.next"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindSwitchNextKey);
    }
    if (k.compare(QStringLiteral("windowing.bindSwitchPrev"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.switchPrev"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.switch.prev"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindSwitchPrevKey);
    }
    if (k.compare(QStringLiteral("windowing.bindWorkspace"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.workspace"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bindOverview"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.overview"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindWorkspaceKey);
    }
    if (k.compare(QStringLiteral("windowing.bindClose"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.close"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindCloseKey);
    }
    if (k.compare(QStringLiteral("windowing.bindMaximize"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.maximize"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindMaximizeKey);
    }
    if (k.compare(QStringLiteral("windowing.bindTileLeft"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.tileLeft"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.tile.left"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindTileLeftKey);
    }
    if (k.compare(QStringLiteral("windowing.bindTileRight"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.tileRight"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.tile.right"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindTileRightKey);
    }
    if (k.compare(QStringLiteral("windowing.bindUntile"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.untile"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindUntileKey);
    }
    if (k.compare(QStringLiteral("windowing.bindQuarterTopLeft"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.quarterTopLeft"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.quarter.top.left"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindQuarterTopLeftKey);
    }
    if (k.compare(QStringLiteral("windowing.bindQuarterTopRight"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.quarterTopRight"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.quarter.top.right"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindQuarterTopRightKey);
    }
    if (k.compare(QStringLiteral("windowing.bindQuarterBottomLeft"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.quarterBottomLeft"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.quarter.bottom.left"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindQuarterBottomLeftKey);
    }
    if (k.compare(QStringLiteral("windowing.bindQuarterBottomRight"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.quarterBottomRight"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.bind.quarter.bottom.right"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowBindQuarterBottomRightKey);
    }
    if (k.compare(QStringLiteral("windowing.shadowEnabled"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.shadow.enabled"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowShadowEnabledKey);
    }
    if (k.compare(QStringLiteral("windowing.shadowStrength"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.shadow.strength"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowShadowStrengthKey);
    }
    if (k.compare(QStringLiteral("windowing.roundedEnabled"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.rounded.enabled"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowRoundedEnabledKey);
    }
    if (k.compare(QStringLiteral("windowing.roundedRadius"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.rounded.radius"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowRoundedRadiusKey);
    }
    if (k.compare(QStringLiteral("windowing.animationEnabled"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.animation.enabled"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowAnimationEnabledKey);
    }
    if (k.compare(QStringLiteral("windowing.animationSpeed"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.animation.speed"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowAnimationSpeedKey);
    }
    if (k.compare(QStringLiteral("windowing.sceneFxEnabled"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.scenefxenabled"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.scenefx.enabled"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowSceneFxEnabledKey);
    }
    if (k.compare(QStringLiteral("windowing.sceneFxDimAlpha"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.scenefxdimalpha"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.scenefx.dimalpha"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowSceneFxDimAlphaKey);
    }
    if (k.compare(QStringLiteral("windowing.sceneFxAnimBoost"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.scenefxanimboost"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.scenefx.animboost"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowSceneFxAnimBoostKey);
    }
    if (k.compare(QStringLiteral("windowing.sceneFxRoundedClipEnabled"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.scenefxroundedclipenabled"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("windowing.scenefx.roundedclip.enabled"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kWindowSceneFxRoundedClipEnabledKey);
    }
    if (k.compare(QStringLiteral("app.score.launchWeight"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("app.scoreLaunchWeight"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kAppScoreLaunchWeightKey);
    }
    if (k.compare(QStringLiteral("app.score.fileOpenWeight"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("app.scoreFileOpenWeight"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kAppScoreFileOpenWeightKey);
    }
    if (k.compare(QStringLiteral("app.score.recencyWeight"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("app.scoreRecencyWeight"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kAppScoreRecencyWeightKey);
    }
    if (k.compare(QStringLiteral("tothespot.searchProfile"), Qt::CaseInsensitive) == 0 ||
        k.compare(QStringLiteral("tothespot.search.profile"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kTothespotSearchProfileKey);
    }
    return k;
}
} // namespace

UIPreferences::UIPreferences(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat,
                 QSettings::UserScope,
                 QCoreApplication::organizationName().isEmpty()
                     ? QStringLiteral("DesktopShell")
                     : QCoreApplication::organizationName(),
                 QCoreApplication::applicationName().isEmpty()
                     ? QStringLiteral("DesktopShell")
                     : QCoreApplication::applicationName())
{
    if (!m_settings.contains(kWindowBindWorkspaceKey) &&
        m_settings.contains(kWindowBindOverviewLegacyKey)) {
        m_settings.setValue(kWindowBindWorkspaceKey, m_settings.value(kWindowBindOverviewLegacyKey));
    }
    m_dockMotionPreset = sanitizePreset(m_settings.value(kMotionPresetKey, QStringLiteral("macos-lively")).toString());
    m_dockDropPulseEnabled = m_settings.value(kDropPulseEnabledKey, true).toBool();
    m_dockAutoHideEnabled = m_settings.value(kDockAutoHideEnabledKey, false).toBool();
    m_dockDragThresholdMouse = clampThreshold(m_settings.value(kDragThresholdMouseKey, 6).toInt());
    m_dockDragThresholdTouchpad = clampThreshold(m_settings.value(kDragThresholdTouchpadKey, 3).toInt());
    m_verboseLogging = m_settings.value(kVerboseLoggingKey, false).toBool();
    m_iconThemeLight = m_settings.value(kIconThemeLightKey).toString().trimmed();
    m_iconThemeDark = m_settings.value(kIconThemeDarkKey).toString().trimmed();
    m_wallpaperUri = m_settings.value(kWallpaperUriKey).toString().trimmed();
    {
        const QString mode = m_settings.value(kThemeModeKey, QStringLiteral("auto")).toString().trimmed().toLower();
        m_themeMode = (mode == QLatin1String("light") || mode == QLatin1String("dark")) ? mode : QStringLiteral("auto");
    }
    m_accentColor = m_settings.value(kAccentColorKey, QString::fromLatin1(kDefaultAccentColor)).toString().trimmed();
    if (m_accentColor.isEmpty()) m_accentColor = QString::fromLatin1(kDefaultAccentColor);
    m_fontScale = qBound(0.8, m_settings.value(kFontScaleKey, 1.0).toDouble(), 1.5);
    m_gtkThemeLight = m_settings.value(kGtkThemeLightKey).toString().trimmed();
    m_gtkThemeDark = m_settings.value(kGtkThemeDarkKey).toString().trimmed();
    m_kdeColorSchemeLight = m_settings.value(kKdeColorSchemeLightKey).toString().trimmed();
    m_kdeColorSchemeDark = m_settings.value(kKdeColorSchemeDarkKey).toString().trimmed();
    m_gtkIconThemeLight = m_settings.value(kGtkIconThemeLightKey).toString().trimmed();
    m_gtkIconThemeDark = m_settings.value(kGtkIconThemeDarkKey).toString().trimmed();
    m_kdeIconThemeLight = m_settings.value(kKdeIconThemeLightKey).toString().trimmed();
    m_kdeIconThemeDark = m_settings.value(kKdeIconThemeDarkKey).toString().trimmed();
    m_defaultFont = m_settings.value(kDefaultFontKey).toString().trimmed();
    m_documentFont = m_settings.value(kDocumentFontKey).toString().trimmed();
    m_monospaceFont = m_settings.value(kMonospaceFontKey).toString().trimmed();
    m_titlebarFont = m_settings.value(kTitlebarFontKey).toString().trimmed();
    syncSettings();
    watchSettingsFile();
}

QString UIPreferences::dockMotionPreset() const
{
    return m_dockMotionPreset;
}

bool UIPreferences::dockDropPulseEnabled() const
{
    return m_dockDropPulseEnabled;
}

bool UIPreferences::dockAutoHideEnabled() const
{
    return m_dockAutoHideEnabled;
}

int UIPreferences::dockDragThresholdMouse() const
{
    return m_dockDragThresholdMouse;
}

int UIPreferences::dockDragThresholdTouchpad() const
{
    return m_dockDragThresholdTouchpad;
}

bool UIPreferences::verboseLogging() const
{
    return m_verboseLogging;
}

QString UIPreferences::iconThemeLight() const
{
    return m_iconThemeLight;
}

QString UIPreferences::iconThemeDark() const
{
    return m_iconThemeDark;
}

QString UIPreferences::wallpaperUri() const
{
    return m_wallpaperUri;
}

void UIPreferences::setWallpaperUri(const QString &uri)
{
    const QString trimmed = uri.trimmed();
    if (trimmed == m_wallpaperUri)
        return;
    m_wallpaperUri = trimmed;
    if (m_wallpaperUri.isEmpty())
        m_settings.remove(kWallpaperUriKey);
    else
        m_settings.setValue(kWallpaperUriKey, m_wallpaperUri);
    emit wallpaperUriChanged();
}

QString UIPreferences::themeMode() const { return m_themeMode; }

void UIPreferences::setThemeMode(const QString &mode)
{
    const QString v = mode.trimmed().toLower();
    const QString normalized = (v == QLatin1String("light") || v == QLatin1String("dark")) ? v : QStringLiteral("auto");
    if (normalized == m_themeMode) return;
    m_themeMode = normalized;
    m_settings.setValue(kThemeModeKey, m_themeMode);
    emit themeModeChanged();
}

QString UIPreferences::accentColor() const { return m_accentColor; }

void UIPreferences::setAccentColor(const QString &color)
{
    const QString v = color.trimmed();
    if (v == m_accentColor || v.isEmpty()) return;
    m_accentColor = v;
    m_settings.setValue(kAccentColorKey, m_accentColor);
    emit accentColorChanged();
}

qreal UIPreferences::fontScale() const { return m_fontScale; }

void UIPreferences::setFontScale(qreal scale)
{
    const qreal clamped = qBound(0.8, scale, 1.5);
    if (qFuzzyCompare(clamped, m_fontScale)) return;
    m_fontScale = clamped;
    m_settings.setValue(kFontScaleKey, m_fontScale);
    emit fontScaleChanged();
}

QString UIPreferences::gtkThemeLight() const { return m_gtkThemeLight; }

void UIPreferences::setGtkThemeLight(const QString &theme)
{
    const QString v = theme.trimmed();
    if (v == m_gtkThemeLight) return;
    m_gtkThemeLight = v;
    if (m_gtkThemeLight.isEmpty())
        m_settings.remove(kGtkThemeLightKey);
    else
        m_settings.setValue(kGtkThemeLightKey, m_gtkThemeLight);
    emit gtkThemeLightChanged();
}

QString UIPreferences::gtkThemeDark() const { return m_gtkThemeDark; }

void UIPreferences::setGtkThemeDark(const QString &theme)
{
    const QString v = theme.trimmed();
    if (v == m_gtkThemeDark) return;
    m_gtkThemeDark = v;
    if (m_gtkThemeDark.isEmpty())
        m_settings.remove(kGtkThemeDarkKey);
    else
        m_settings.setValue(kGtkThemeDarkKey, m_gtkThemeDark);
    emit gtkThemeDarkChanged();
}

QString UIPreferences::kdeColorSchemeLight() const { return m_kdeColorSchemeLight; }

void UIPreferences::setKdeColorSchemeLight(const QString &scheme)
{
    const QString v = scheme.trimmed();
    if (v == m_kdeColorSchemeLight) return;
    m_kdeColorSchemeLight = v;
    if (m_kdeColorSchemeLight.isEmpty())
        m_settings.remove(kKdeColorSchemeLightKey);
    else
        m_settings.setValue(kKdeColorSchemeLightKey, m_kdeColorSchemeLight);
    emit kdeColorSchemeLightChanged();
}

QString UIPreferences::kdeColorSchemeDark() const { return m_kdeColorSchemeDark; }

void UIPreferences::setKdeColorSchemeDark(const QString &scheme)
{
    const QString v = scheme.trimmed();
    if (v == m_kdeColorSchemeDark) return;
    m_kdeColorSchemeDark = v;
    if (m_kdeColorSchemeDark.isEmpty())
        m_settings.remove(kKdeColorSchemeDarkKey);
    else
        m_settings.setValue(kKdeColorSchemeDarkKey, m_kdeColorSchemeDark);
    emit kdeColorSchemeDarkChanged();
}

QString UIPreferences::gtkIconThemeLight() const { return m_gtkIconThemeLight; }

void UIPreferences::setGtkIconThemeLight(const QString &theme)
{
    const QString v = theme.trimmed();
    if (v == m_gtkIconThemeLight) return;
    m_gtkIconThemeLight = v;
    if (m_gtkIconThemeLight.isEmpty())
        m_settings.remove(kGtkIconThemeLightKey);
    else
        m_settings.setValue(kGtkIconThemeLightKey, m_gtkIconThemeLight);
    emit gtkIconThemeLightChanged();
}

QString UIPreferences::gtkIconThemeDark() const { return m_gtkIconThemeDark; }

void UIPreferences::setGtkIconThemeDark(const QString &theme)
{
    const QString v = theme.trimmed();
    if (v == m_gtkIconThemeDark) return;
    m_gtkIconThemeDark = v;
    if (m_gtkIconThemeDark.isEmpty())
        m_settings.remove(kGtkIconThemeDarkKey);
    else
        m_settings.setValue(kGtkIconThemeDarkKey, m_gtkIconThemeDark);
    emit gtkIconThemeDarkChanged();
}

QString UIPreferences::kdeIconThemeLight() const { return m_kdeIconThemeLight; }

void UIPreferences::setKdeIconThemeLight(const QString &theme)
{
    const QString v = theme.trimmed();
    if (v == m_kdeIconThemeLight) return;
    m_kdeIconThemeLight = v;
    if (m_kdeIconThemeLight.isEmpty())
        m_settings.remove(kKdeIconThemeLightKey);
    else
        m_settings.setValue(kKdeIconThemeLightKey, m_kdeIconThemeLight);
    emit kdeIconThemeLightChanged();
}

QString UIPreferences::kdeIconThemeDark() const { return m_kdeIconThemeDark; }

void UIPreferences::setKdeIconThemeDark(const QString &theme)
{
    const QString v = theme.trimmed();
    if (v == m_kdeIconThemeDark) return;
    m_kdeIconThemeDark = v;
    if (m_kdeIconThemeDark.isEmpty())
        m_settings.remove(kKdeIconThemeDarkKey);
    else
        m_settings.setValue(kKdeIconThemeDarkKey, m_kdeIconThemeDark);
    emit kdeIconThemeDarkChanged();
}

QString UIPreferences::defaultFont() const { return m_defaultFont; }

void UIPreferences::setDefaultFont(const QString &spec)
{
    const QString v = spec.trimmed();
    if (v == m_defaultFont) return;
    m_defaultFont = v;
    if (m_defaultFont.isEmpty()) m_settings.remove(kDefaultFontKey);
    else m_settings.setValue(kDefaultFontKey, m_defaultFont);
    emit defaultFontChanged();
}

QString UIPreferences::documentFont() const { return m_documentFont; }

void UIPreferences::setDocumentFont(const QString &spec)
{
    const QString v = spec.trimmed();
    if (v == m_documentFont) return;
    m_documentFont = v;
    if (m_documentFont.isEmpty()) m_settings.remove(kDocumentFontKey);
    else m_settings.setValue(kDocumentFontKey, m_documentFont);
    emit documentFontChanged();
}

QString UIPreferences::monospaceFont() const { return m_monospaceFont; }

void UIPreferences::setMonospaceFont(const QString &spec)
{
    const QString v = spec.trimmed();
    if (v == m_monospaceFont) return;
    m_monospaceFont = v;
    if (m_monospaceFont.isEmpty()) m_settings.remove(kMonospaceFontKey);
    else m_settings.setValue(kMonospaceFontKey, m_monospaceFont);
    emit monospaceFontChanged();
}

QString UIPreferences::titlebarFont() const { return m_titlebarFont; }

void UIPreferences::setTitlebarFont(const QString &spec)
{
    const QString v = spec.trimmed();
    if (v == m_titlebarFont) return;
    m_titlebarFont = v;
    if (m_titlebarFont.isEmpty()) m_settings.remove(kTitlebarFontKey);
    else m_settings.setValue(kTitlebarFontKey, m_titlebarFont);
    emit titlebarFontChanged();
}

void UIPreferences::watchSettingsFile()
{
    const QString path = m_settings.fileName();
    if (path.isEmpty())
        return;
    if (!m_fileWatcher.files().contains(path))
        m_fileWatcher.addPath(path);
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &UIPreferences::onSettingsFileChanged,
            Qt::UniqueConnection);
}

void UIPreferences::onSettingsFileChanged(const QString &path)
{
    // Re-watch: some editors/writers do atomic replace (delete + create).
    if (!m_fileWatcher.files().contains(path))
        m_fileWatcher.addPath(path);

    m_settings.sync();

    // Re-read each property and emit if changed.
    {
        const QString mode = m_settings.value(kThemeModeKey, QStringLiteral("auto")).toString().trimmed().toLower();
        const QString normalized = (mode == QLatin1String("light") || mode == QLatin1String("dark")) ? mode : QStringLiteral("auto");
        if (normalized != m_themeMode) {
            m_themeMode = normalized;
            emit themeModeChanged();
        }
    }
    {
        const QString color = m_settings.value(kAccentColorKey, QString::fromLatin1(kDefaultAccentColor)).toString().trimmed();
        const QString val = color.isEmpty() ? QString::fromLatin1(kDefaultAccentColor) : color;
        if (val != m_accentColor) {
            m_accentColor = val;
            emit accentColorChanged();
        }
    }
    {
        const qreal scale = qBound(0.8, m_settings.value(kFontScaleKey, 1.0).toDouble(), 1.5);
        if (!qFuzzyCompare(scale, m_fontScale)) {
            m_fontScale = scale;
            emit fontScaleChanged();
        }
    }
    {
        const QString uri = m_settings.value(kWallpaperUriKey).toString().trimmed();
        if (uri != m_wallpaperUri) {
            m_wallpaperUri = uri;
            emit wallpaperUriChanged();
        }
    }
    {
        const QString light = m_settings.value(kIconThemeLightKey).toString().trimmed();
        if (light != m_iconThemeLight) {
            m_iconThemeLight = light;
            emit iconThemeLightChanged();
        }
    }
    {
        const QString dark = m_settings.value(kIconThemeDarkKey).toString().trimmed();
        if (dark != m_iconThemeDark) {
            m_iconThemeDark = dark;
            emit iconThemeDarkChanged();
        }
    }
    {
        const QString v = m_settings.value(kGtkThemeLightKey).toString().trimmed();
        if (v != m_gtkThemeLight) { m_gtkThemeLight = v; emit gtkThemeLightChanged(); }
    }
    {
        const QString v = m_settings.value(kGtkThemeDarkKey).toString().trimmed();
        if (v != m_gtkThemeDark) { m_gtkThemeDark = v; emit gtkThemeDarkChanged(); }
    }
    {
        const QString v = m_settings.value(kKdeColorSchemeLightKey).toString().trimmed();
        if (v != m_kdeColorSchemeLight) { m_kdeColorSchemeLight = v; emit kdeColorSchemeLightChanged(); }
    }
    {
        const QString v = m_settings.value(kKdeColorSchemeDarkKey).toString().trimmed();
        if (v != m_kdeColorSchemeDark) { m_kdeColorSchemeDark = v; emit kdeColorSchemeDarkChanged(); }
    }
    {
        const QString v = m_settings.value(kGtkIconThemeLightKey).toString().trimmed();
        if (v != m_gtkIconThemeLight) { m_gtkIconThemeLight = v; emit gtkIconThemeLightChanged(); }
    }
    {
        const QString v = m_settings.value(kGtkIconThemeDarkKey).toString().trimmed();
        if (v != m_gtkIconThemeDark) { m_gtkIconThemeDark = v; emit gtkIconThemeDarkChanged(); }
    }
    {
        const QString v = m_settings.value(kKdeIconThemeLightKey).toString().trimmed();
        if (v != m_kdeIconThemeLight) { m_kdeIconThemeLight = v; emit kdeIconThemeLightChanged(); }
    }
    {
        const QString v = m_settings.value(kKdeIconThemeDarkKey).toString().trimmed();
        if (v != m_kdeIconThemeDark) { m_kdeIconThemeDark = v; emit kdeIconThemeDarkChanged(); }
    }
    {
        const QString v = m_settings.value(kDefaultFontKey).toString().trimmed();
        if (v != m_defaultFont) { m_defaultFont = v; emit defaultFontChanged(); }
    }
    {
        const QString v = m_settings.value(kDocumentFontKey).toString().trimmed();
        if (v != m_documentFont) { m_documentFont = v; emit documentFontChanged(); }
    }
    {
        const QString v = m_settings.value(kMonospaceFontKey).toString().trimmed();
        if (v != m_monospaceFont) { m_monospaceFont = v; emit monospaceFontChanged(); }
    }
    {
        const QString v = m_settings.value(kTitlebarFontKey).toString().trimmed();
        if (v != m_titlebarFont) { m_titlebarFont = v; emit titlebarFontChanged(); }
    }
    {
        const QString preset = sanitizePreset(m_settings.value(kMotionPresetKey, QStringLiteral("macos-lively")).toString());
        if (preset != m_dockMotionPreset) {
            m_dockMotionPreset = preset;
            emit dockMotionPresetChanged();
        }
    }
    {
        const bool pulse = m_settings.value(kDropPulseEnabledKey, true).toBool();
        if (pulse != m_dockDropPulseEnabled) {
            m_dockDropPulseEnabled = pulse;
            emit dockDropPulseEnabledChanged();
        }
    }
    {
        const bool autoHide = m_settings.value(kDockAutoHideEnabledKey, false).toBool();
        if (autoHide != m_dockAutoHideEnabled) {
            m_dockAutoHideEnabled = autoHide;
            emit dockAutoHideEnabledChanged();
        }
    }
    {
        const int mouse = clampThreshold(m_settings.value(kDragThresholdMouseKey, 6).toInt());
        if (mouse != m_dockDragThresholdMouse) {
            m_dockDragThresholdMouse = mouse;
            emit dockDragThresholdMouseChanged();
        }
    }
    {
        const int touchpad = clampThreshold(m_settings.value(kDragThresholdTouchpadKey, 3).toInt());
        if (touchpad != m_dockDragThresholdTouchpad) {
            m_dockDragThresholdTouchpad = touchpad;
            emit dockDragThresholdTouchpadChanged();
        }
    }
}

void UIPreferences::setDockMotionPreset(const QString &preset)
{
    const QString normalized = sanitizePreset(preset);
    if (normalized == m_dockMotionPreset) {
        return;
    }
    m_dockMotionPreset = normalized;
    m_settings.setValue(kMotionPresetKey, m_dockMotionPreset);
    m_settings.sync();
    emit dockMotionPresetChanged();
}

void UIPreferences::setDockDropPulseEnabled(bool enabled)
{
    if (enabled == m_dockDropPulseEnabled) {
        return;
    }
    m_dockDropPulseEnabled = enabled;
    m_settings.setValue(kDropPulseEnabledKey, m_dockDropPulseEnabled);
    m_settings.sync();
    emit dockDropPulseEnabledChanged();
}

void UIPreferences::setDockAutoHideEnabled(bool enabled)
{
    if (enabled == m_dockAutoHideEnabled) {
        return;
    }
    m_dockAutoHideEnabled = enabled;
    m_settings.setValue(kDockAutoHideEnabledKey, m_dockAutoHideEnabled);
    m_settings.sync();
    emit dockAutoHideEnabledChanged();
}

void UIPreferences::setDockDragThresholdMouse(int thresholdPx)
{
    const int clamped = clampThreshold(thresholdPx);
    if (clamped == m_dockDragThresholdMouse) {
        return;
    }
    m_dockDragThresholdMouse = clamped;
    m_settings.setValue(kDragThresholdMouseKey, m_dockDragThresholdMouse);
    m_settings.sync();
    emit dockDragThresholdMouseChanged();
}

void UIPreferences::setDockDragThresholdTouchpad(int thresholdPx)
{
    const int clamped = clampThreshold(thresholdPx);
    if (clamped == m_dockDragThresholdTouchpad) {
        return;
    }
    m_dockDragThresholdTouchpad = clamped;
    m_settings.setValue(kDragThresholdTouchpadKey, m_dockDragThresholdTouchpad);
    m_settings.sync();
    emit dockDragThresholdTouchpadChanged();
}

void UIPreferences::setVerboseLogging(bool enabled)
{
    if (enabled == m_verboseLogging) {
        return;
    }
    m_verboseLogging = enabled;
    m_settings.setValue(kVerboseLoggingKey, m_verboseLogging);
    m_settings.sync();
    emit verboseLoggingChanged();
}

void UIPreferences::setIconThemeLight(const QString &themeName)
{
    const QString normalized = themeName.trimmed();
    if (normalized == m_iconThemeLight) {
        return;
    }
    m_iconThemeLight = normalized;
    if (m_iconThemeLight.isEmpty()) {
        m_settings.remove(kIconThemeLightKey);
    } else {
        m_settings.setValue(kIconThemeLightKey, m_iconThemeLight);
    }
    m_settings.sync();
    emit iconThemeLightChanged();
}

void UIPreferences::setIconThemeDark(const QString &themeName)
{
    const QString normalized = themeName.trimmed();
    if (normalized == m_iconThemeDark) {
        return;
    }
    m_iconThemeDark = normalized;
    if (m_iconThemeDark.isEmpty()) {
        m_settings.remove(kIconThemeDarkKey);
    } else {
        m_settings.setValue(kIconThemeDarkKey, m_iconThemeDark);
    }
    m_settings.sync();
    emit iconThemeDarkChanged();
}

void UIPreferences::setIconThemeMapping(const QString &lightThemeName, const QString &darkThemeName)
{
    setIconThemeLight(lightThemeName);
    setIconThemeDark(darkThemeName);
}

QString UIPreferences::sanitizePreset(const QString &preset) const
{
    const QString lowered = preset.trimmed().toLower();
    if (lowered == QStringLiteral("macos-lively") || lowered == QStringLiteral("lively") ||
        lowered == QStringLiteral("expressive")) {
        return QStringLiteral("macos-lively");
    }
    return QStringLiteral("subtle");
}

int UIPreferences::clampThreshold(int value) const
{
    if (value < 2) {
        return 2;
    }
    if (value > 24) {
        return 24;
    }
    return value;
}

void UIPreferences::syncSettings()
{
    m_settings.setValue(kMotionPresetKey, m_dockMotionPreset);
    m_settings.setValue(kDropPulseEnabledKey, m_dockDropPulseEnabled);
    m_settings.setValue(kDockAutoHideEnabledKey, m_dockAutoHideEnabled);
    m_settings.setValue(kDragThresholdMouseKey, m_dockDragThresholdMouse);
    m_settings.setValue(kDragThresholdTouchpadKey, m_dockDragThresholdTouchpad);
    m_settings.setValue(kVerboseLoggingKey, m_verboseLogging);
    if (m_iconThemeLight.isEmpty()) {
        m_settings.remove(kIconThemeLightKey);
    } else {
        m_settings.setValue(kIconThemeLightKey, m_iconThemeLight);
    }
    if (m_iconThemeDark.isEmpty()) {
        m_settings.remove(kIconThemeDarkKey);
    } else {
        m_settings.setValue(kIconThemeDarkKey, m_iconThemeDark);
    }
    m_settings.sync();
}

QVariant UIPreferences::getPreference(const QString &key, const QVariant &fallback) const
{
    const QString normalized = normalizePreferenceKey(key);
    if (normalized.isEmpty()) {
        return fallback;
    }
    if (normalized == QString::fromLatin1(kMotionPresetKey)) {
        return m_dockMotionPreset;
    }
    if (normalized == QString::fromLatin1(kDropPulseEnabledKey)) {
        return m_dockDropPulseEnabled;
    }
    if (normalized == QString::fromLatin1(kDockAutoHideEnabledKey)) {
        return m_dockAutoHideEnabled;
    }
    if (normalized == QString::fromLatin1(kDockHideModeKey)) {
        QString raw = m_settings.value(kDockHideModeKey).toString().trimmed().toLower();
        if (raw == QStringLiteral("snart_hide")) {
            raw = QStringLiteral("smart_hide");
        }
        if (raw == QStringLiteral("no_hide") ||
            raw == QStringLiteral("duration_hide") ||
            raw == QStringLiteral("smart_hide")) {
            return raw;
        }
        return m_dockAutoHideEnabled ? QStringLiteral("duration_hide")
                                     : QStringLiteral("no_hide");
    }
    if (normalized == QString::fromLatin1(kDockHideDurationMsKey)) {
        const int v = m_settings.value(kDockHideDurationMsKey, 450).toInt();
        return qBound(100, v, 5000);
    }
    if (normalized == QString::fromLatin1(kDragThresholdMouseKey)) {
        return m_dockDragThresholdMouse;
    }
    if (normalized == QString::fromLatin1(kDragThresholdTouchpadKey)) {
        return m_dockDragThresholdTouchpad;
    }
    if (normalized == QString::fromLatin1(kVerboseLoggingKey)) {
        return m_verboseLogging;
    }
    if (normalized == QString::fromLatin1(kIconThemeLightKey)) {
        return m_iconThemeLight;
    }
    if (normalized == QString::fromLatin1(kIconThemeDarkKey)) {
        return m_iconThemeDark;
    }
    if (normalized == QString::fromLatin1(kWindowBindMinimizeKey)) {
        return m_settings.value(kWindowBindMinimizeKey, QString::fromLatin1(kWindowBindMinimizeDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindRestoreKey)) {
        return m_settings.value(kWindowBindRestoreKey, QString::fromLatin1(kWindowBindRestoreDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindSwitchNextKey)) {
        return m_settings.value(kWindowBindSwitchNextKey, QString::fromLatin1(kWindowBindSwitchNextDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindSwitchPrevKey)) {
        return m_settings.value(kWindowBindSwitchPrevKey, QString::fromLatin1(kWindowBindSwitchPrevDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindWorkspaceKey)) {
        if (m_settings.contains(kWindowBindWorkspaceKey)) {
            return m_settings.value(kWindowBindWorkspaceKey,
                                    QString::fromLatin1(kWindowBindWorkspaceDefault));
        }
        if (m_settings.contains(kWindowBindOverviewLegacyKey)) {
            return m_settings.value(kWindowBindOverviewLegacyKey,
                                    QString::fromLatin1(kWindowBindWorkspaceDefault));
        }
        return QString::fromLatin1(kWindowBindWorkspaceDefault);
    }
    if (normalized == QString::fromLatin1(kWindowBindCloseKey)) {
        return m_settings.value(kWindowBindCloseKey, QString::fromLatin1(kWindowBindCloseDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindMaximizeKey)) {
        return m_settings.value(kWindowBindMaximizeKey, QString::fromLatin1(kWindowBindMaximizeDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindTileLeftKey)) {
        return m_settings.value(kWindowBindTileLeftKey, QString::fromLatin1(kWindowBindTileLeftDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindTileRightKey)) {
        return m_settings.value(kWindowBindTileRightKey, QString::fromLatin1(kWindowBindTileRightDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindUntileKey)) {
        return m_settings.value(kWindowBindUntileKey, QString::fromLatin1(kWindowBindUntileDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindQuarterTopLeftKey)) {
        return m_settings.value(kWindowBindQuarterTopLeftKey, QString::fromLatin1(kWindowBindQuarterTopLeftDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindQuarterTopRightKey)) {
        return m_settings.value(kWindowBindQuarterTopRightKey, QString::fromLatin1(kWindowBindQuarterTopRightDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindQuarterBottomLeftKey)) {
        return m_settings.value(kWindowBindQuarterBottomLeftKey, QString::fromLatin1(kWindowBindQuarterBottomLeftDefault));
    }
    if (normalized == QString::fromLatin1(kWindowBindQuarterBottomRightKey)) {
        return m_settings.value(kWindowBindQuarterBottomRightKey, QString::fromLatin1(kWindowBindQuarterBottomRightDefault));
    }
    if (normalized == QString::fromLatin1(kWindowShadowEnabledKey)) {
        return m_settings.value(kWindowShadowEnabledKey, kWindowShadowEnabledDefault).toBool();
    }
    if (normalized == QString::fromLatin1(kWindowShadowStrengthKey)) {
        const int v = m_settings.value(kWindowShadowStrengthKey, kWindowShadowStrengthDefault).toInt();
        return qBound(20, v, 180);
    }
    if (normalized == QString::fromLatin1(kWindowRoundedEnabledKey)) {
        return m_settings.value(kWindowRoundedEnabledKey, kWindowRoundedEnabledDefault).toBool();
    }
    if (normalized == QString::fromLatin1(kWindowRoundedRadiusKey)) {
        const int v = m_settings.value(kWindowRoundedRadiusKey, kWindowRoundedRadiusDefault).toInt();
        return qBound(4, v, 24);
    }
    if (normalized == QString::fromLatin1(kWindowAnimationEnabledKey)) {
        return m_settings.value(kWindowAnimationEnabledKey, kWindowAnimationEnabledDefault).toBool();
    }
    if (normalized == QString::fromLatin1(kWindowAnimationSpeedKey)) {
        const int v = m_settings.value(kWindowAnimationSpeedKey, kWindowAnimationSpeedDefault).toInt();
        return qBound(50, v, 200);
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxEnabledKey)) {
        return m_settings.value(kWindowSceneFxEnabledKey, kWindowSceneFxEnabledDefault).toBool();
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxDimAlphaKey)) {
        const double v = m_settings.value(kWindowSceneFxDimAlphaKey, kWindowSceneFxDimAlphaDefault).toDouble();
        return qBound(0.10, v, 0.70);
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxAnimBoostKey)) {
        const int v = m_settings.value(kWindowSceneFxAnimBoostKey, kWindowSceneFxAnimBoostDefault).toInt();
        return qBound(50, v, 180);
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxRoundedClipEnabledKey)) {
        return m_settings.value(kWindowSceneFxRoundedClipEnabledKey,
                                kWindowSceneFxRoundedClipEnabledDefault).toBool();
    }
    if (normalized == QString::fromLatin1(kAppScoreLaunchWeightKey)) {
        const int v = m_settings.value(kAppScoreLaunchWeightKey, kAppScoreLaunchWeightDefault).toInt();
        return qBound(1, v, 50);
    }
    if (normalized == QString::fromLatin1(kAppScoreFileOpenWeightKey)) {
        const int v = m_settings.value(kAppScoreFileOpenWeightKey, kAppScoreFileOpenWeightDefault).toInt();
        return qBound(0, v, 50);
    }
    if (normalized == QString::fromLatin1(kAppScoreRecencyWeightKey)) {
        const int v = m_settings.value(kAppScoreRecencyWeightKey, kAppScoreRecencyWeightDefault).toInt();
        return qBound(0, v, 20);
    }
    if (normalized == QString::fromLatin1(kTothespotSearchProfileKey)) {
        return normalizeTothespotSearchProfile(
            m_settings.value(kTothespotSearchProfileKey,
                             QString::fromLatin1(kTothespotSearchProfileDefault)).toString());
    }
    return m_settings.value(normalized, fallback);
}

bool UIPreferences::setPreference(const QString &key, const QVariant &value)
{
    const QString normalized = normalizePreferenceKey(key);
    if (normalized.isEmpty()) {
        return false;
    }

    if (normalized == QString::fromLatin1(kMotionPresetKey)) {
        const QString before = m_dockMotionPreset;
        setDockMotionPreset(value.toString());
        return before != m_dockMotionPreset;
    }
    if (normalized == QString::fromLatin1(kDropPulseEnabledKey)) {
        const bool before = m_dockDropPulseEnabled;
        setDockDropPulseEnabled(value.toBool());
        return before != m_dockDropPulseEnabled;
    }
    if (normalized == QString::fromLatin1(kDockAutoHideEnabledKey)) {
        const bool before = m_dockAutoHideEnabled;
        setDockAutoHideEnabled(value.toBool());
        return before != m_dockAutoHideEnabled;
    }
    if (normalized == QString::fromLatin1(kDockHideModeKey)) {
        QString mode = value.toString().trimmed().toLower();
        if (mode == QStringLiteral("snart_hide")) {
            mode = QStringLiteral("smart_hide");
        }
        if (mode != QStringLiteral("no_hide") &&
            mode != QStringLiteral("duration_hide") &&
            mode != QStringLiteral("smart_hide")) {
            mode = QStringLiteral("duration_hide");
        }
        const QString before = m_settings.value(kDockHideModeKey).toString().trimmed().toLower();
        if (before == mode) {
            return false;
        }
        m_settings.setValue(kDockHideModeKey, mode);
        // Keep legacy boolean in sync.
        setDockAutoHideEnabled(mode != QStringLiteral("no_hide"));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kDockHideDurationMsKey)) {
        const int before = m_settings.value(kDockHideDurationMsKey, 450).toInt();
        const int after = qBound(100, value.toInt(), 5000);
        if (before == after) {
            return false;
        }
        m_settings.setValue(kDockHideDurationMsKey, after);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kDragThresholdMouseKey)) {
        const int before = m_dockDragThresholdMouse;
        setDockDragThresholdMouse(value.toInt());
        return before != m_dockDragThresholdMouse;
    }
    if (normalized == QString::fromLatin1(kDragThresholdTouchpadKey)) {
        const int before = m_dockDragThresholdTouchpad;
        setDockDragThresholdTouchpad(value.toInt());
        return before != m_dockDragThresholdTouchpad;
    }
    if (normalized == QString::fromLatin1(kVerboseLoggingKey)) {
        const bool before = m_verboseLogging;
        setVerboseLogging(value.toBool());
        return before != m_verboseLogging;
    }
    if (normalized == QString::fromLatin1(kIconThemeLightKey)) {
        const QString before = m_iconThemeLight;
        setIconThemeLight(value.toString());
        return before != m_iconThemeLight;
    }
    if (normalized == QString::fromLatin1(kIconThemeDarkKey)) {
        const QString before = m_iconThemeDark;
        setIconThemeDark(value.toString());
        return before != m_iconThemeDark;
    }
    if (normalized == QString::fromLatin1(kWindowBindMinimizeKey) ||
        normalized == QString::fromLatin1(kWindowBindRestoreKey) ||
        normalized == QString::fromLatin1(kWindowBindSwitchNextKey) ||
        normalized == QString::fromLatin1(kWindowBindSwitchPrevKey) ||
        normalized == QString::fromLatin1(kWindowBindWorkspaceKey) ||
        normalized == QString::fromLatin1(kWindowBindCloseKey) ||
        normalized == QString::fromLatin1(kWindowBindMaximizeKey) ||
        normalized == QString::fromLatin1(kWindowBindTileLeftKey) ||
        normalized == QString::fromLatin1(kWindowBindTileRightKey) ||
        normalized == QString::fromLatin1(kWindowBindUntileKey) ||
        normalized == QString::fromLatin1(kWindowBindQuarterTopLeftKey) ||
        normalized == QString::fromLatin1(kWindowBindQuarterTopRightKey) ||
        normalized == QString::fromLatin1(kWindowBindQuarterBottomLeftKey) ||
        normalized == QString::fromLatin1(kWindowBindQuarterBottomRightKey)) {
        const QString before = m_settings.value(normalized).toString();
        const QString after = value.toString().trimmed();
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        if (normalized == QString::fromLatin1(kWindowBindWorkspaceKey)) {
            m_settings.setValue(kWindowBindOverviewLegacyKey, after);
        }
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowShadowEnabledKey) ||
        normalized == QString::fromLatin1(kWindowRoundedEnabledKey) ||
        normalized == QString::fromLatin1(kWindowAnimationEnabledKey) ||
        normalized == QString::fromLatin1(kWindowSceneFxEnabledKey) ||
        normalized == QString::fromLatin1(kWindowSceneFxRoundedClipEnabledKey)) {
        const bool defaultValue =
            (normalized == QString::fromLatin1(kWindowSceneFxEnabledKey))
                ? kWindowSceneFxEnabledDefault
                : ((normalized == QString::fromLatin1(kWindowSceneFxRoundedClipEnabledKey))
                    ? kWindowSceneFxRoundedClipEnabledDefault : true);
        const bool before = m_settings.value(normalized, defaultValue).toBool();
        const bool after = value.toBool();
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowShadowStrengthKey)) {
        const int before = m_settings.value(normalized, kWindowShadowStrengthDefault).toInt();
        const int after = qBound(20, value.toInt(), 180);
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowRoundedRadiusKey)) {
        const int before = m_settings.value(normalized, kWindowRoundedRadiusDefault).toInt();
        const int after = qBound(4, value.toInt(), 24);
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowAnimationSpeedKey)) {
        const int before = m_settings.value(normalized, kWindowAnimationSpeedDefault).toInt();
        const int after = qBound(50, value.toInt(), 200);
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxDimAlphaKey)) {
        const double before = m_settings.value(normalized, kWindowSceneFxDimAlphaDefault).toDouble();
        const double after = qBound(0.10, value.toDouble(), 0.70);
        if (qFuzzyCompare(before + 1.0, after + 1.0)) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxAnimBoostKey)) {
        const int before = m_settings.value(normalized, kWindowSceneFxAnimBoostDefault).toInt();
        const int after = qBound(50, value.toInt(), 180);
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kAppScoreLaunchWeightKey)) {
        const int before = m_settings.value(normalized, kAppScoreLaunchWeightDefault).toInt();
        const int after = qBound(1, value.toInt(), 50);
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        emit preferenceChanged(normalized, after);
        return true;
    }
    if (normalized == QString::fromLatin1(kAppScoreFileOpenWeightKey)) {
        const int before = m_settings.value(normalized, kAppScoreFileOpenWeightDefault).toInt();
        const int after = qBound(0, value.toInt(), 50);
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        emit preferenceChanged(normalized, after);
        return true;
    }
    if (normalized == QString::fromLatin1(kAppScoreRecencyWeightKey)) {
        const int before = m_settings.value(normalized, kAppScoreRecencyWeightDefault).toInt();
        const int after = qBound(0, value.toInt(), 20);
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        emit preferenceChanged(normalized, after);
        return true;
    }
    if (normalized == QString::fromLatin1(kTothespotSearchProfileKey)) {
        const QString before = normalizeTothespotSearchProfile(
            m_settings.value(normalized, QString::fromLatin1(kTothespotSearchProfileDefault)).toString());
        const QString after = normalizeTothespotSearchProfile(value.toString());
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        emit preferenceChanged(normalized, after);
        return true;
    }

    const QVariant before = m_settings.value(normalized);
    if (before == value) {
        return false;
    }
    m_settings.setValue(normalized, value);
    m_settings.sync();
    emit preferenceChanged(normalized, value);
    return true;
}

bool UIPreferences::resetPreference(const QString &key)
{
    const QString normalized = normalizePreferenceKey(key);
    if (normalized.isEmpty()) {
        return false;
    }

    if (normalized == QString::fromLatin1(kMotionPresetKey)) {
        const QString before = m_dockMotionPreset;
        setDockMotionPreset(QStringLiteral("subtle"));
        return before != m_dockMotionPreset;
    }
    if (normalized == QString::fromLatin1(kDropPulseEnabledKey)) {
        const bool before = m_dockDropPulseEnabled;
        setDockDropPulseEnabled(true);
        return before != m_dockDropPulseEnabled;
    }
    if (normalized == QString::fromLatin1(kDockAutoHideEnabledKey)) {
        const bool before = m_dockAutoHideEnabled;
        setDockAutoHideEnabled(false);
        return before != m_dockAutoHideEnabled;
    }
    if (normalized == QString::fromLatin1(kDockHideModeKey)) {
        const QString before = m_settings.value(kDockHideModeKey).toString().trimmed().toLower();
        if (before == QStringLiteral("duration_hide")) {
            return false;
        }
        m_settings.setValue(kDockHideModeKey, QStringLiteral("duration_hide"));
        setDockAutoHideEnabled(true);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kDockHideDurationMsKey)) {
        const int before = m_settings.value(kDockHideDurationMsKey, 450).toInt();
        if (before == 450) {
            return false;
        }
        m_settings.setValue(kDockHideDurationMsKey, 450);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kDragThresholdMouseKey)) {
        const int before = m_dockDragThresholdMouse;
        setDockDragThresholdMouse(6);
        return before != m_dockDragThresholdMouse;
    }
    if (normalized == QString::fromLatin1(kDragThresholdTouchpadKey)) {
        const int before = m_dockDragThresholdTouchpad;
        setDockDragThresholdTouchpad(3);
        return before != m_dockDragThresholdTouchpad;
    }
    if (normalized == QString::fromLatin1(kVerboseLoggingKey)) {
        const bool before = m_verboseLogging;
        setVerboseLogging(false);
        return before != m_verboseLogging;
    }
    if (normalized == QString::fromLatin1(kIconThemeLightKey)) {
        const QString before = m_iconThemeLight;
        setIconThemeLight(QString());
        return before != m_iconThemeLight;
    }
    if (normalized == QString::fromLatin1(kIconThemeDarkKey)) {
        const QString before = m_iconThemeDark;
        setIconThemeDark(QString());
        return before != m_iconThemeDark;
    }
    if (normalized == QString::fromLatin1(kWindowBindMinimizeKey)) {
        const QString before = m_settings.value(kWindowBindMinimizeKey).toString();
        if (before == QString::fromLatin1(kWindowBindMinimizeDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindMinimizeKey, QString::fromLatin1(kWindowBindMinimizeDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindRestoreKey)) {
        const QString before = m_settings.value(kWindowBindRestoreKey).toString();
        if (before == QString::fromLatin1(kWindowBindRestoreDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindRestoreKey, QString::fromLatin1(kWindowBindRestoreDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindSwitchNextKey)) {
        const QString before = m_settings.value(kWindowBindSwitchNextKey).toString();
        if (before == QString::fromLatin1(kWindowBindSwitchNextDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindSwitchNextKey, QString::fromLatin1(kWindowBindSwitchNextDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindSwitchPrevKey)) {
        const QString before = m_settings.value(kWindowBindSwitchPrevKey).toString();
        if (before == QString::fromLatin1(kWindowBindSwitchPrevDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindSwitchPrevKey, QString::fromLatin1(kWindowBindSwitchPrevDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindWorkspaceKey)) {
        const QString before = m_settings.value(kWindowBindWorkspaceKey).toString();
        if (before == QString::fromLatin1(kWindowBindWorkspaceDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindWorkspaceKey, QString::fromLatin1(kWindowBindWorkspaceDefault));
        m_settings.setValue(kWindowBindOverviewLegacyKey, QString::fromLatin1(kWindowBindWorkspaceDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindCloseKey)) {
        const QString before = m_settings.value(kWindowBindCloseKey).toString();
        if (before == QString::fromLatin1(kWindowBindCloseDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindCloseKey, QString::fromLatin1(kWindowBindCloseDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindMaximizeKey)) {
        const QString before = m_settings.value(kWindowBindMaximizeKey).toString();
        if (before == QString::fromLatin1(kWindowBindMaximizeDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindMaximizeKey, QString::fromLatin1(kWindowBindMaximizeDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindTileLeftKey)) {
        const QString before = m_settings.value(kWindowBindTileLeftKey).toString();
        if (before == QString::fromLatin1(kWindowBindTileLeftDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindTileLeftKey, QString::fromLatin1(kWindowBindTileLeftDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindTileRightKey)) {
        const QString before = m_settings.value(kWindowBindTileRightKey).toString();
        if (before == QString::fromLatin1(kWindowBindTileRightDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindTileRightKey, QString::fromLatin1(kWindowBindTileRightDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindUntileKey)) {
        const QString before = m_settings.value(kWindowBindUntileKey).toString();
        if (before == QString::fromLatin1(kWindowBindUntileDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindUntileKey, QString::fromLatin1(kWindowBindUntileDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindQuarterTopLeftKey)) {
        const QString before = m_settings.value(kWindowBindQuarterTopLeftKey).toString();
        if (before == QString::fromLatin1(kWindowBindQuarterTopLeftDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindQuarterTopLeftKey, QString::fromLatin1(kWindowBindQuarterTopLeftDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindQuarterTopRightKey)) {
        const QString before = m_settings.value(kWindowBindQuarterTopRightKey).toString();
        if (before == QString::fromLatin1(kWindowBindQuarterTopRightDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindQuarterTopRightKey, QString::fromLatin1(kWindowBindQuarterTopRightDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindQuarterBottomLeftKey)) {
        const QString before = m_settings.value(kWindowBindQuarterBottomLeftKey).toString();
        if (before == QString::fromLatin1(kWindowBindQuarterBottomLeftDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindQuarterBottomLeftKey, QString::fromLatin1(kWindowBindQuarterBottomLeftDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowBindQuarterBottomRightKey)) {
        const QString before = m_settings.value(kWindowBindQuarterBottomRightKey).toString();
        if (before == QString::fromLatin1(kWindowBindQuarterBottomRightDefault)) {
            return false;
        }
        m_settings.setValue(kWindowBindQuarterBottomRightKey, QString::fromLatin1(kWindowBindQuarterBottomRightDefault));
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowShadowEnabledKey)) {
        const bool before = m_settings.value(kWindowShadowEnabledKey, kWindowShadowEnabledDefault).toBool();
        if (before == kWindowShadowEnabledDefault) {
            return false;
        }
        m_settings.setValue(kWindowShadowEnabledKey, kWindowShadowEnabledDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowShadowStrengthKey)) {
        const int before = m_settings.value(kWindowShadowStrengthKey, kWindowShadowStrengthDefault).toInt();
        if (before == kWindowShadowStrengthDefault) {
            return false;
        }
        m_settings.setValue(kWindowShadowStrengthKey, kWindowShadowStrengthDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowRoundedEnabledKey)) {
        const bool before = m_settings.value(kWindowRoundedEnabledKey, kWindowRoundedEnabledDefault).toBool();
        if (before == kWindowRoundedEnabledDefault) {
            return false;
        }
        m_settings.setValue(kWindowRoundedEnabledKey, kWindowRoundedEnabledDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowRoundedRadiusKey)) {
        const int before = m_settings.value(kWindowRoundedRadiusKey, kWindowRoundedRadiusDefault).toInt();
        if (before == kWindowRoundedRadiusDefault) {
            return false;
        }
        m_settings.setValue(kWindowRoundedRadiusKey, kWindowRoundedRadiusDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowAnimationEnabledKey)) {
        const bool before = m_settings.value(kWindowAnimationEnabledKey, kWindowAnimationEnabledDefault).toBool();
        if (before == kWindowAnimationEnabledDefault) {
            return false;
        }
        m_settings.setValue(kWindowAnimationEnabledKey, kWindowAnimationEnabledDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxEnabledKey)) {
        const bool before = m_settings.value(kWindowSceneFxEnabledKey, kWindowSceneFxEnabledDefault).toBool();
        if (before == kWindowSceneFxEnabledDefault) {
            return false;
        }
        m_settings.setValue(kWindowSceneFxEnabledKey, kWindowSceneFxEnabledDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxRoundedClipEnabledKey)) {
        const bool before = m_settings.value(kWindowSceneFxRoundedClipEnabledKey,
                                             kWindowSceneFxRoundedClipEnabledDefault).toBool();
        if (before == kWindowSceneFxRoundedClipEnabledDefault) {
            return false;
        }
        m_settings.setValue(kWindowSceneFxRoundedClipEnabledKey,
                            kWindowSceneFxRoundedClipEnabledDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowAnimationSpeedKey)) {
        const int before = m_settings.value(kWindowAnimationSpeedKey, kWindowAnimationSpeedDefault).toInt();
        if (before == kWindowAnimationSpeedDefault) {
            return false;
        }
        m_settings.setValue(kWindowAnimationSpeedKey, kWindowAnimationSpeedDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxDimAlphaKey)) {
        const double before = m_settings.value(kWindowSceneFxDimAlphaKey, kWindowSceneFxDimAlphaDefault).toDouble();
        if (qFuzzyCompare(before + 1.0, kWindowSceneFxDimAlphaDefault + 1.0)) {
            return false;
        }
        m_settings.setValue(kWindowSceneFxDimAlphaKey, kWindowSceneFxDimAlphaDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kWindowSceneFxAnimBoostKey)) {
        const int before = m_settings.value(kWindowSceneFxAnimBoostKey, kWindowSceneFxAnimBoostDefault).toInt();
        if (before == kWindowSceneFxAnimBoostDefault) {
            return false;
        }
        m_settings.setValue(kWindowSceneFxAnimBoostKey, kWindowSceneFxAnimBoostDefault);
        m_settings.sync();
        return true;
    }
    if (normalized == QString::fromLatin1(kTothespotSearchProfileKey)) {
        const QString before = normalizeTothespotSearchProfile(
            m_settings.value(normalized, QString::fromLatin1(kTothespotSearchProfileDefault)).toString());
        const QString after = QString::fromLatin1(kTothespotSearchProfileDefault);
        if (before == after) {
            return false;
        }
        m_settings.setValue(normalized, after);
        m_settings.sync();
        emit preferenceChanged(normalized, after);
        return true;
    }

    if (!m_settings.contains(normalized)) {
        return false;
    }
    m_settings.remove(normalized);
    m_settings.sync();
    emit preferenceChanged(normalized, getPreference(normalized, QVariant()));
    return true;
}

QVariantMap UIPreferences::preferenceSnapshot() const
{
    QVariantMap out;
    out.insert(QString::fromLatin1(kMotionPresetKey), m_dockMotionPreset);
    out.insert(QString::fromLatin1(kDropPulseEnabledKey), m_dockDropPulseEnabled);
    out.insert(QString::fromLatin1(kDockAutoHideEnabledKey), m_dockAutoHideEnabled);
    out.insert(QString::fromLatin1(kDockHideModeKey),
               getPreference(QString::fromLatin1(kDockHideModeKey), QStringLiteral("duration_hide")));
    out.insert(QString::fromLatin1(kDockHideDurationMsKey),
               getPreference(QString::fromLatin1(kDockHideDurationMsKey), 450));
    out.insert(QString::fromLatin1(kDragThresholdMouseKey), m_dockDragThresholdMouse);
    out.insert(QString::fromLatin1(kDragThresholdTouchpadKey), m_dockDragThresholdTouchpad);
    out.insert(QString::fromLatin1(kVerboseLoggingKey), m_verboseLogging);
    out.insert(QString::fromLatin1(kIconThemeLightKey), m_iconThemeLight);
    out.insert(QString::fromLatin1(kIconThemeDarkKey), m_iconThemeDark);
    out.insert(QString::fromLatin1(kAppScoreLaunchWeightKey),
               getPreference(QString::fromLatin1(kAppScoreLaunchWeightKey),
                             kAppScoreLaunchWeightDefault));
    out.insert(QString::fromLatin1(kAppScoreFileOpenWeightKey),
               getPreference(QString::fromLatin1(kAppScoreFileOpenWeightKey),
                             kAppScoreFileOpenWeightDefault));
    out.insert(QString::fromLatin1(kAppScoreRecencyWeightKey),
               getPreference(QString::fromLatin1(kAppScoreRecencyWeightKey),
                             kAppScoreRecencyWeightDefault));
    out.insert(QString::fromLatin1(kTothespotSearchProfileKey),
               getPreference(QString::fromLatin1(kTothespotSearchProfileKey),
                             QString::fromLatin1(kTothespotSearchProfileDefault)));
    out.insert(QString::fromLatin1(kWindowBindMinimizeKey),
               m_settings.value(kWindowBindMinimizeKey, QString::fromLatin1(kWindowBindMinimizeDefault)));
    out.insert(QString::fromLatin1(kWindowBindRestoreKey),
               m_settings.value(kWindowBindRestoreKey, QString::fromLatin1(kWindowBindRestoreDefault)));
    out.insert(QString::fromLatin1(kWindowBindSwitchNextKey),
               m_settings.value(kWindowBindSwitchNextKey, QString::fromLatin1(kWindowBindSwitchNextDefault)));
    out.insert(QString::fromLatin1(kWindowBindSwitchPrevKey),
               m_settings.value(kWindowBindSwitchPrevKey, QString::fromLatin1(kWindowBindSwitchPrevDefault)));
    out.insert(QString::fromLatin1(kWindowBindWorkspaceKey),
               m_settings.value(kWindowBindWorkspaceKey, QString::fromLatin1(kWindowBindWorkspaceDefault)));
    out.insert(QString::fromLatin1(kWindowBindOverviewLegacyKey),
               m_settings.value(kWindowBindWorkspaceKey, QString::fromLatin1(kWindowBindWorkspaceDefault)));
    out.insert(QString::fromLatin1(kWindowBindCloseKey),
               m_settings.value(kWindowBindCloseKey, QString::fromLatin1(kWindowBindCloseDefault)));
    out.insert(QString::fromLatin1(kWindowBindMaximizeKey),
               m_settings.value(kWindowBindMaximizeKey, QString::fromLatin1(kWindowBindMaximizeDefault)));
    out.insert(QString::fromLatin1(kWindowBindTileLeftKey),
               m_settings.value(kWindowBindTileLeftKey, QString::fromLatin1(kWindowBindTileLeftDefault)));
    out.insert(QString::fromLatin1(kWindowBindTileRightKey),
               m_settings.value(kWindowBindTileRightKey, QString::fromLatin1(kWindowBindTileRightDefault)));
    out.insert(QString::fromLatin1(kWindowBindUntileKey),
               m_settings.value(kWindowBindUntileKey, QString::fromLatin1(kWindowBindUntileDefault)));
    out.insert(QString::fromLatin1(kWindowBindQuarterTopLeftKey),
               m_settings.value(kWindowBindQuarterTopLeftKey, QString::fromLatin1(kWindowBindQuarterTopLeftDefault)));
    out.insert(QString::fromLatin1(kWindowBindQuarterTopRightKey),
               m_settings.value(kWindowBindQuarterTopRightKey, QString::fromLatin1(kWindowBindQuarterTopRightDefault)));
    out.insert(QString::fromLatin1(kWindowBindQuarterBottomLeftKey),
               m_settings.value(kWindowBindQuarterBottomLeftKey, QString::fromLatin1(kWindowBindQuarterBottomLeftDefault)));
    out.insert(QString::fromLatin1(kWindowBindQuarterBottomRightKey),
               m_settings.value(kWindowBindQuarterBottomRightKey, QString::fromLatin1(kWindowBindQuarterBottomRightDefault)));
    out.insert(QString::fromLatin1(kWindowShadowEnabledKey),
               m_settings.value(kWindowShadowEnabledKey, kWindowShadowEnabledDefault));
    out.insert(QString::fromLatin1(kWindowShadowStrengthKey),
               qBound(20, m_settings.value(kWindowShadowStrengthKey, kWindowShadowStrengthDefault).toInt(), 180));
    out.insert(QString::fromLatin1(kWindowRoundedEnabledKey),
               m_settings.value(kWindowRoundedEnabledKey, kWindowRoundedEnabledDefault));
    out.insert(QString::fromLatin1(kWindowRoundedRadiusKey),
               qBound(4, m_settings.value(kWindowRoundedRadiusKey, kWindowRoundedRadiusDefault).toInt(), 24));
    out.insert(QString::fromLatin1(kWindowAnimationEnabledKey),
               m_settings.value(kWindowAnimationEnabledKey, kWindowAnimationEnabledDefault));
    out.insert(QString::fromLatin1(kWindowAnimationSpeedKey),
               qBound(50, m_settings.value(kWindowAnimationSpeedKey, kWindowAnimationSpeedDefault).toInt(), 200));
    out.insert(QString::fromLatin1(kWindowSceneFxEnabledKey),
               m_settings.value(kWindowSceneFxEnabledKey, kWindowSceneFxEnabledDefault));
    out.insert(QString::fromLatin1(kWindowSceneFxDimAlphaKey),
               qBound(0.10, m_settings.value(kWindowSceneFxDimAlphaKey, kWindowSceneFxDimAlphaDefault).toDouble(), 0.70));
    out.insert(QString::fromLatin1(kWindowSceneFxAnimBoostKey),
               qBound(50, m_settings.value(kWindowSceneFxAnimBoostKey, kWindowSceneFxAnimBoostDefault).toInt(), 180));
    out.insert(QString::fromLatin1(kWindowSceneFxRoundedClipEnabledKey),
               m_settings.value(kWindowSceneFxRoundedClipEnabledKey,
                                kWindowSceneFxRoundedClipEnabledDefault));
    return out;
}

QString UIPreferences::settingsFilePath() const
{
    return m_settings.fileName();
}
