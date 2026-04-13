#include "desktopsettingsclient.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QStringList>
#include <QtGlobal>
#include <functional>

namespace {
constexpr auto kService = "org.slm.Desktop.Settings";
constexpr auto kPath = "/org/slm/Desktop/Settings";
constexpr auto kIface = "org.slm.Desktop.Settings";

const QStringList kManagedShortcutPaths{
    QStringLiteral("windowing.bindClose"),
    QStringLiteral("windowing.bindMinimize"),
    QStringLiteral("windowing.bindMaximize"),
    QStringLiteral("windowing.bindTileLeft"),
    QStringLiteral("windowing.bindTileRight"),
    QStringLiteral("windowing.bindSwitchNext"),
    QStringLiteral("windowing.bindSwitchPrev"),
    QStringLiteral("windowing.bindWorkspace"),
    QStringLiteral("shortcuts.workspaceOverview"),
    QStringLiteral("shortcuts.workspacePrev"),
    QStringLiteral("shortcuts.workspaceNext"),
    QStringLiteral("shortcuts.moveWindowPrev"),
    QStringLiteral("shortcuts.moveWindowNext"),
};

bool isManagedShortcutPath(const QString &path)
{
    return kManagedShortcutPaths.contains(path.trimmed());
}

QVariant mapValueByPath(const QVariantMap &root, const QString &path, bool *ok = nullptr)
{
    if (ok) {
        *ok = false;
    }
    const QStringList segments = path.split('.', Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        return {};
    }
    QVariant current = root;
    for (int i = 0; i < segments.size(); ++i) {
        const QVariantMap node = current.toMap();
        if (!node.contains(segments.at(i))) {
            return {};
        }
        current = node.value(segments.at(i));
        if (i < segments.size() - 1 && !current.canConvert<QVariantMap>()) {
            return {};
        }
    }
    if (ok) {
        *ok = true;
    }
    return current;
}

bool mapSetValueByPath(QVariantMap &root, const QString &path, const QVariant &value)
{
    const QStringList segments = path.split('.', Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        return false;
    }
    std::function<bool(QVariantMap &, int)> assign = [&](QVariantMap &node, int depth) -> bool {
        const QString key = segments.at(depth);
        if (depth == segments.size() - 1) {
            node.insert(key, value);
            return true;
        }
        QVariant child = node.value(key);
        QVariantMap childMap;
        if (child.isNull()) {
            childMap = QVariantMap{};
        } else if (child.canConvert<QVariantMap>()) {
            childMap = child.toMap();
        } else {
            return false;
        }
        if (!assign(childMap, depth + 1)) {
            return false;
        }
        node.insert(key, childMap);
        return true;
    };
    return assign(root, 0);
}
}

DesktopSettingsClient::DesktopSettingsClient(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("/org/freedesktop/DBus"),
                QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("NameOwnerChanged"),
                this,
                SLOT(onNameOwnerChanged(QString,QString,QString)));

    refresh();
}

bool DesktopSettingsClient::available() const { return m_available; }
QString DesktopSettingsClient::themeMode() const { return m_themeMode; }
QString DesktopSettingsClient::accentColor() const { return m_accentColor; }
double DesktopSettingsClient::fontScale() const { return m_fontScale; }
QString DesktopSettingsClient::gtkThemeLight() const { return m_gtkThemeLight; }
QString DesktopSettingsClient::gtkThemeDark() const { return m_gtkThemeDark; }
QString DesktopSettingsClient::kdeColorSchemeLight() const { return m_kdeColorSchemeLight; }
QString DesktopSettingsClient::kdeColorSchemeDark() const { return m_kdeColorSchemeDark; }
QString DesktopSettingsClient::gtkIconThemeLight() const { return m_gtkIconThemeLight; }
QString DesktopSettingsClient::gtkIconThemeDark() const { return m_gtkIconThemeDark; }
QString DesktopSettingsClient::kdeIconThemeLight() const { return m_kdeIconThemeLight; }
QString DesktopSettingsClient::kdeIconThemeDark() const { return m_kdeIconThemeDark; }
bool DesktopSettingsClient::qtGenericAllowKdeCompat() const { return m_qtGenericAllowKdeCompat; }
bool DesktopSettingsClient::qtIncompatibleUseDesktopFallback() const { return m_qtIncompatibleUseDesktopFallback; }
bool DesktopSettingsClient::unknownUsesSafeFallback() const { return m_unknownUsesSafeFallback; }
bool DesktopSettingsClient::contextAutoReduceAnimation() const { return m_contextAutoReduceAnimation; }
bool DesktopSettingsClient::contextAutoDisableBlur() const { return m_contextAutoDisableBlur; }
bool DesktopSettingsClient::contextAutoDisableHeavyEffects() const { return m_contextAutoDisableHeavyEffects; }
QString DesktopSettingsClient::contextTimeMode() const { return m_contextTimeMode; }
int DesktopSettingsClient::contextTimeSunriseHour() const { return m_contextTimeSunriseHour; }
int DesktopSettingsClient::contextTimeSunsetHour() const { return m_contextTimeSunsetHour; }
bool DesktopSettingsClient::highContrast() const { return m_highContrast; }
QString DesktopSettingsClient::dockMotionPreset() const { return m_dockMotionPreset; }
bool DesktopSettingsClient::dockAutoHideEnabled() const { return m_dockAutoHideEnabled; }
bool DesktopSettingsClient::dockDropPulseEnabled() const { return m_dockDropPulseEnabled; }
int DesktopSettingsClient::dockDragThresholdMouse() const { return m_dockDragThresholdMouse; }
int DesktopSettingsClient::dockDragThresholdTouchpad() const { return m_dockDragThresholdTouchpad; }
QString DesktopSettingsClient::dockIconSize() const { return m_dockIconSize; }
bool DesktopSettingsClient::dockMagnificationEnabled() const { return m_dockMagnificationEnabled; }
bool DesktopSettingsClient::windowingAnimationEnabled() const { return m_windowingAnimationEnabled; }
QString DesktopSettingsClient::animationMode() const { return m_animationMode; }
QString DesktopSettingsClient::windowControlsSide() const { return m_windowControlsSide; }
QString DesktopSettingsClient::printPdfFallbackPrinterId() const { return m_printPdfFallbackPrinterId; }
QString DesktopSettingsClient::defaultFont() const { return m_defaultFont; }
QString DesktopSettingsClient::documentFont() const { return m_documentFont; }
QString DesktopSettingsClient::monospaceFont() const { return m_monospaceFont; }
QString DesktopSettingsClient::titlebarFont() const { return m_titlebarFont; }
QString DesktopSettingsClient::wallpaperUri() const { return m_wallpaperUri; }

bool DesktopSettingsClient::setThemeMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized != QStringLiteral("light")
            && normalized != QStringLiteral("dark")
            && normalized != QStringLiteral("auto")) {
        return false;
    }
    if (setSetting(QStringLiteral("globalAppearance.colorMode"), normalized)) {
        setThemeModeLocal(normalized);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setAccentColor(const QString &color)
{
    const QString normalized = color.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    if (setSetting(QStringLiteral("globalAppearance.accentColor"), normalized)) {
        setAccentColorLocal(normalized);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setFontScale(double scale)
{
    const double normalized = qBound(0.8, scale, 1.5);
    if (setSetting(QStringLiteral("globalAppearance.uiScale"), normalized)) {
        setFontScaleLocal(normalized);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setGtkThemeLight(const QString &value)
{
    const QString v = value.trimmed();
    if (setSetting(QStringLiteral("gtkThemeRouting.themeLight"), v)) {
        setThemeStringLocal(m_gtkThemeLight, v, &DesktopSettingsClient::gtkThemeLightChanged);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setGtkThemeDark(const QString &value)
{
    const QString v = value.trimmed();
    if (setSetting(QStringLiteral("gtkThemeRouting.themeDark"), v)) {
        setThemeStringLocal(m_gtkThemeDark, v, &DesktopSettingsClient::gtkThemeDarkChanged);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setKdeColorSchemeLight(const QString &value)
{
    const QString v = value.trimmed();
    if (setSetting(QStringLiteral("kdeThemeRouting.themeLight"), v)) {
        setThemeStringLocal(m_kdeColorSchemeLight, v, &DesktopSettingsClient::kdeColorSchemeLightChanged);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setKdeColorSchemeDark(const QString &value)
{
    const QString v = value.trimmed();
    if (setSetting(QStringLiteral("kdeThemeRouting.themeDark"), v)) {
        setThemeStringLocal(m_kdeColorSchemeDark, v, &DesktopSettingsClient::kdeColorSchemeDarkChanged);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setGtkIconThemeLight(const QString &value)
{
    const QString v = value.trimmed();
    if (setSetting(QStringLiteral("gtkThemeRouting.iconThemeLight"), v)) {
        setThemeStringLocal(m_gtkIconThemeLight, v, &DesktopSettingsClient::gtkIconThemeLightChanged);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setGtkIconThemeDark(const QString &value)
{
    const QString v = value.trimmed();
    if (setSetting(QStringLiteral("gtkThemeRouting.iconThemeDark"), v)) {
        setThemeStringLocal(m_gtkIconThemeDark, v, &DesktopSettingsClient::gtkIconThemeDarkChanged);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setKdeIconThemeLight(const QString &value)
{
    const QString v = value.trimmed();
    if (setSetting(QStringLiteral("kdeThemeRouting.iconThemeLight"), v)) {
        setThemeStringLocal(m_kdeIconThemeLight, v, &DesktopSettingsClient::kdeIconThemeLightChanged);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setKdeIconThemeDark(const QString &value)
{
    const QString v = value.trimmed();
    if (setSetting(QStringLiteral("kdeThemeRouting.iconThemeDark"), v)) {
        setThemeStringLocal(m_kdeIconThemeDark, v, &DesktopSettingsClient::kdeIconThemeDarkChanged);
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setQtGenericAllowKdeCompat(bool enabled)
{
    if (setSetting(QStringLiteral("appThemePolicy.qtGenericAllowKdeCompat"), enabled)) {
        if (m_qtGenericAllowKdeCompat != enabled) {
            m_qtGenericAllowKdeCompat = enabled;
            emit qtGenericAllowKdeCompatChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setQtIncompatibleUseDesktopFallback(bool enabled)
{
    if (setSetting(QStringLiteral("appThemePolicy.qtIncompatibleUseDesktopFallback"), enabled)) {
        if (m_qtIncompatibleUseDesktopFallback != enabled) {
            m_qtIncompatibleUseDesktopFallback = enabled;
            emit qtIncompatibleUseDesktopFallbackChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setUnknownUsesSafeFallback(bool enabled)
{
    if (setSetting(QStringLiteral("fallbackPolicy.unknownUsesSafeFallback"), enabled)) {
        if (m_unknownUsesSafeFallback != enabled) {
            m_unknownUsesSafeFallback = enabled;
            emit unknownUsesSafeFallbackChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setContextAutoReduceAnimation(bool enabled)
{
    if (setSetting(QStringLiteral("contextAutomation.autoReduceAnimation"), enabled)) {
        if (m_contextAutoReduceAnimation != enabled) {
            m_contextAutoReduceAnimation = enabled;
            emit contextAutomationChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setContextAutoDisableBlur(bool enabled)
{
    if (setSetting(QStringLiteral("contextAutomation.autoDisableBlur"), enabled)) {
        if (m_contextAutoDisableBlur != enabled) {
            m_contextAutoDisableBlur = enabled;
            emit contextAutomationChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setContextAutoDisableHeavyEffects(bool enabled)
{
    if (setSetting(QStringLiteral("contextAutomation.autoDisableHeavyEffects"), enabled)) {
        if (m_contextAutoDisableHeavyEffects != enabled) {
            m_contextAutoDisableHeavyEffects = enabled;
            emit contextAutomationChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setContextTimeMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized != QStringLiteral("local") && normalized != QStringLiteral("sun")) {
        return false;
    }
    if (setSetting(QStringLiteral("contextTime.mode"), normalized)) {
        if (m_contextTimeMode != normalized) {
            m_contextTimeMode = normalized;
            emit contextTimeChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setContextTimeSunriseHour(int hour)
{
    const int normalized = qBound(0, hour, 23);
    if (setSetting(QStringLiteral("contextTime.sunriseHour"), normalized)) {
        if (m_contextTimeSunriseHour != normalized) {
            m_contextTimeSunriseHour = normalized;
            emit contextTimeChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setContextTimeSunsetHour(int hour)
{
    const int normalized = qBound(0, hour, 23);
    if (setSetting(QStringLiteral("contextTime.sunsetHour"), normalized)) {
        if (m_contextTimeSunsetHour != normalized) {
            m_contextTimeSunsetHour = normalized;
            emit contextTimeChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setHighContrast(bool enabled)
{
    if (setSetting(QStringLiteral("globalAppearance.highContrast"), enabled)) {
        if (m_highContrast != enabled) {
            m_highContrast = enabled;
            emit highContrastChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setDockMotionPreset(const QString &preset)
{
    const QString normalized = (preset.trimmed().toLower() == QLatin1String("macos-lively"))
            ? QStringLiteral("macos-lively")
            : QStringLiteral("subtle");
    if (setSetting(QStringLiteral("dock.motionPreset"), normalized)) {
        if (m_dockMotionPreset != normalized) {
            m_dockMotionPreset = normalized;
            emit dockMotionPresetChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setDockAutoHideEnabled(bool enabled)
{
    if (setSetting(QStringLiteral("dock.autoHideEnabled"), enabled)) {
        if (m_dockAutoHideEnabled != enabled) {
            m_dockAutoHideEnabled = enabled;
            emit dockAutoHideEnabledChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setDockDropPulseEnabled(bool enabled)
{
    if (setSetting(QStringLiteral("dock.dropPulseEnabled"), enabled)) {
        if (m_dockDropPulseEnabled != enabled) {
            m_dockDropPulseEnabled = enabled;
            emit dockDropPulseEnabledChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setDockDragThresholdMouse(int value)
{
    const int normalized = qBound(2, value, 24);
    if (setSetting(QStringLiteral("dock.dragThresholdMouse"), normalized)) {
        if (m_dockDragThresholdMouse != normalized) {
            m_dockDragThresholdMouse = normalized;
            emit dockDragThresholdMouseChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setDockDragThresholdTouchpad(int value)
{
    const int normalized = qBound(2, value, 24);
    if (setSetting(QStringLiteral("dock.dragThresholdTouchpad"), normalized)) {
        if (m_dockDragThresholdTouchpad != normalized) {
            m_dockDragThresholdTouchpad = normalized;
            emit dockDragThresholdTouchpadChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setDockIconSize(const QString &value)
{
    const QString normalized = [&]() {
        const QString v = value.trimmed().toLower();
        if (v == QLatin1String("small") || v == QLatin1String("large")) {
            return v;
        }
        return QStringLiteral("medium");
    }();
    if (setSetting(QStringLiteral("dock.iconSize"), normalized)) {
        if (m_dockIconSize != normalized) {
            m_dockIconSize = normalized;
            emit dockIconSizeChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setDockMagnificationEnabled(bool enabled)
{
    if (setSetting(QStringLiteral("dock.magnificationEnabled"), enabled)) {
        if (m_dockMagnificationEnabled != enabled) {
            m_dockMagnificationEnabled = enabled;
            emit dockMagnificationEnabledChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setWindowingAnimationEnabled(bool enabled)
{
    if (setSetting(QStringLiteral("windowing.animationEnabled"), enabled)) {
        if (m_windowingAnimationEnabled != enabled) {
            m_windowingAnimationEnabled = enabled;
            emit windowingAnimationEnabledChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setWindowControlsSide(const QString &side)
{
    const QString normalized = side.trimmed().toLower() == QLatin1String("left")
            ? QStringLiteral("left")
            : QStringLiteral("right");
    if (setSetting(QStringLiteral("windowing.controlsSide"), normalized)) {
        if (m_windowControlsSide != normalized) {
            m_windowControlsSide = normalized;
            emit windowControlsSideChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setPrintPdfFallbackPrinterId(const QString &printerId)
{
    const QString normalized = printerId.trimmed();
    if (setSetting(QStringLiteral("print.pdfFallbackPrinterId"), normalized)) {
        if (m_printPdfFallbackPrinterId != normalized) {
            m_printPdfFallbackPrinterId = normalized;
            emit printPdfFallbackPrinterIdChanged();
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setDefaultFont(const QString &spec)
{
    return setFontByPath(QStringLiteral("fonts.defaultFont"),
                         m_defaultFont,
                         spec,
                         &DesktopSettingsClient::defaultFontChanged);
}

bool DesktopSettingsClient::setDocumentFont(const QString &spec)
{
    return setFontByPath(QStringLiteral("fonts.documentFont"),
                         m_documentFont,
                         spec,
                         &DesktopSettingsClient::documentFontChanged);
}

bool DesktopSettingsClient::setMonospaceFont(const QString &spec)
{
    return setFontByPath(QStringLiteral("fonts.monospaceFont"),
                         m_monospaceFont,
                         spec,
                         &DesktopSettingsClient::monospaceFontChanged);
}

bool DesktopSettingsClient::setTitlebarFont(const QString &spec)
{
    return setFontByPath(QStringLiteral("fonts.titlebarFont"),
                         m_titlebarFont,
                         spec,
                         &DesktopSettingsClient::titlebarFontChanged);
}

bool DesktopSettingsClient::setWallpaperUri(const QString &uri)
{
    const QString normalized = uri.trimmed();
    if (setSetting(QStringLiteral("wallpaper.uri"), normalized)) {
        if (m_wallpaperUri != normalized) {
            m_wallpaperUri = normalized;
            emit wallpaperUriChanged();
        }
        return true;
    }
    return false;
}

QString DesktopSettingsClient::keyboardShortcut(const QString &path, const QString &defaultValue) const
{
    const QString normalizedPath = path.trimmed();
    if (!isManagedShortcutPath(normalizedPath)) {
        return defaultValue;
    }
    const QString stored = m_keyboardShortcuts.value(normalizedPath).toString().trimmed();
    return stored.isEmpty() ? defaultValue : stored;
}

bool DesktopSettingsClient::setKeyboardShortcut(const QString &path, const QString &value)
{
    const QString normalizedPath = path.trimmed();
    const QString normalizedValue = value.trimmed();
    if (!isManagedShortcutPath(normalizedPath) || normalizedValue.isEmpty()) {
        return false;
    }
    if (setSetting(normalizedPath, normalizedValue)) {
        const QString current = m_keyboardShortcuts.value(normalizedPath).toString();
        if (current != normalizedValue) {
            m_keyboardShortcuts.insert(normalizedPath, normalizedValue);
            emit keyboardShortcutChanged(normalizedPath);
        }
        return true;
    }
    return false;
}

bool DesktopSettingsClient::setPerAppOverride(const QString &appIdOrDesktopId, const QString &category)
{
    const QString key = appIdOrDesktopId.trimmed().toLower();
    const QString cat = category.trimmed().toLower();
    if (key.isEmpty() || cat.isEmpty()) {
        return false;
    }
    QVariantMap patch;
    patch.insert(QStringLiteral("perAppOverride.") + key, cat);
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("SetSettingsPatch"), patch);
    return reply.isValid() && reply.value().value(QStringLiteral("ok"), false).toBool();
}

bool DesktopSettingsClient::clearPerAppOverride(const QString &appIdOrDesktopId)
{
    const QString key = appIdOrDesktopId.trimmed().toLower();
    if (key.isEmpty()) {
        return false;
    }
    QVariantMap patch;
    patch.insert(QStringLiteral("perAppOverride.") + key, QVariant{});
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("SetSettingsPatch"), patch);
    return reply.isValid() && reply.value().value(QStringLiteral("ok"), false).toBool();
}

QVariantMap DesktopSettingsClient::classifyApp(const QVariantMap &appDescriptor)
{
    if (!ensureIface()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("settingsd-unavailable")}};
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("ClassifyApp"), appDescriptor);
    return reply.isValid()
               ? reply.value()
               : QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), reply.error().message()}};
}

QVariantMap DesktopSettingsClient::resolveThemeForApp(const QVariantMap &appDescriptor,
                                                      const QVariantMap &options)
{
    if (!ensureIface()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("settingsd-unavailable")}};
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("ResolveThemeForApp"), appDescriptor, options);
    return reply.isValid()
               ? reply.value()
               : QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), reply.error().message()}};
}

QVariant DesktopSettingsClient::settingValue(const QString &path, const QVariant &fallback) const
{
    const QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        return fallback;
    }
    bool found = false;
    const QVariant v = valueByPath(m_settingsSnapshot, normalizedPath, &found);
    return found ? v : fallback;
}

bool DesktopSettingsClient::setSettingValue(const QString &path, const QVariant &value)
{
    return setSetting(path.trimmed(), value);
}

void DesktopSettingsClient::refresh()
{
    if (ensureIface()) {
        loadFromService();
    }
}

void DesktopSettingsClient::onNameOwnerChanged(const QString &name,
                                               const QString &,
                                               const QString &newOwner)
{
    if (name != QLatin1String(kService)) {
        return;
    }
    Q_UNUSED(newOwner)
    refresh();
}

void DesktopSettingsClient::onSettingChanged(const QString &path, const QDBusVariant &value)
{
    const QVariant rawValue = value.variant();
    mapSetValueByPath(m_settingsSnapshot, path, rawValue);
    if (path == QLatin1String("globalAppearance.colorMode")) {
        setThemeModeLocal(rawValue.toString().trimmed().toLower());
    } else if (path == QLatin1String("globalAppearance.accentColor")) {
        setAccentColorLocal(rawValue.toString().trimmed());
    } else if (path == QLatin1String("globalAppearance.uiScale")) {
        setFontScaleLocal(rawValue.toDouble());
    } else if (path == QLatin1String("gtkThemeRouting.themeLight")) {
        setThemeStringLocal(m_gtkThemeLight, rawValue.toString().trimmed(), &DesktopSettingsClient::gtkThemeLightChanged);
    } else if (path == QLatin1String("gtkThemeRouting.themeDark")) {
        setThemeStringLocal(m_gtkThemeDark, rawValue.toString().trimmed(), &DesktopSettingsClient::gtkThemeDarkChanged);
    } else if (path == QLatin1String("kdeThemeRouting.themeLight")) {
        setThemeStringLocal(m_kdeColorSchemeLight, rawValue.toString().trimmed(), &DesktopSettingsClient::kdeColorSchemeLightChanged);
    } else if (path == QLatin1String("kdeThemeRouting.themeDark")) {
        setThemeStringLocal(m_kdeColorSchemeDark, rawValue.toString().trimmed(), &DesktopSettingsClient::kdeColorSchemeDarkChanged);
    } else if (path == QLatin1String("gtkThemeRouting.iconThemeLight")) {
        setThemeStringLocal(m_gtkIconThemeLight, rawValue.toString().trimmed(), &DesktopSettingsClient::gtkIconThemeLightChanged);
    } else if (path == QLatin1String("gtkThemeRouting.iconThemeDark")) {
        setThemeStringLocal(m_gtkIconThemeDark, rawValue.toString().trimmed(), &DesktopSettingsClient::gtkIconThemeDarkChanged);
    } else if (path == QLatin1String("kdeThemeRouting.iconThemeLight")) {
        setThemeStringLocal(m_kdeIconThemeLight, rawValue.toString().trimmed(), &DesktopSettingsClient::kdeIconThemeLightChanged);
    } else if (path == QLatin1String("kdeThemeRouting.iconThemeDark")) {
        setThemeStringLocal(m_kdeIconThemeDark, rawValue.toString().trimmed(), &DesktopSettingsClient::kdeIconThemeDarkChanged);
    } else if (path == QLatin1String("appThemePolicy.qtGenericAllowKdeCompat")) {
        const bool v = rawValue.toBool();
        if (m_qtGenericAllowKdeCompat != v) {
            m_qtGenericAllowKdeCompat = v;
            emit qtGenericAllowKdeCompatChanged();
        }
    } else if (path == QLatin1String("appThemePolicy.qtIncompatibleUseDesktopFallback")) {
        const bool v = rawValue.toBool();
        if (m_qtIncompatibleUseDesktopFallback != v) {
            m_qtIncompatibleUseDesktopFallback = v;
            emit qtIncompatibleUseDesktopFallbackChanged();
        }
    } else if (path == QLatin1String("fallbackPolicy.unknownUsesSafeFallback")) {
        const bool v = rawValue.toBool();
        if (m_unknownUsesSafeFallback != v) {
            m_unknownUsesSafeFallback = v;
            emit unknownUsesSafeFallbackChanged();
        }
    } else if (path == QLatin1String("contextAutomation.autoReduceAnimation")) {
        const bool v = rawValue.toBool();
        if (m_contextAutoReduceAnimation != v) {
            m_contextAutoReduceAnimation = v;
            emit contextAutomationChanged();
        }
    } else if (path == QLatin1String("contextAutomation.autoDisableBlur")) {
        const bool v = rawValue.toBool();
        if (m_contextAutoDisableBlur != v) {
            m_contextAutoDisableBlur = v;
            emit contextAutomationChanged();
        }
    } else if (path == QLatin1String("contextAutomation.autoDisableHeavyEffects")) {
        const bool v = rawValue.toBool();
        if (m_contextAutoDisableHeavyEffects != v) {
            m_contextAutoDisableHeavyEffects = v;
            emit contextAutomationChanged();
        }
    } else if (path == QLatin1String("contextTime.mode")) {
        const QString v = rawValue.toString().trimmed().toLower();
        if (m_contextTimeMode != v) {
            m_contextTimeMode = v;
            emit contextTimeChanged();
        }
    } else if (path == QLatin1String("contextTime.sunriseHour")) {
        const int v = qBound(0, rawValue.toInt(), 23);
        if (m_contextTimeSunriseHour != v) {
            m_contextTimeSunriseHour = v;
            emit contextTimeChanged();
        }
    } else if (path == QLatin1String("contextTime.sunsetHour")) {
        const int v = qBound(0, rawValue.toInt(), 23);
        if (m_contextTimeSunsetHour != v) {
            m_contextTimeSunsetHour = v;
            emit contextTimeChanged();
        }
    } else if (path == QLatin1String("globalAppearance.highContrast")) {
        const bool v = rawValue.toBool();
        if (m_highContrast != v) {
            m_highContrast = v;
            emit highContrastChanged();
        }
    } else if (path == QLatin1String("dock.motionPreset")) {
        const QString v = (rawValue.toString().trimmed().toLower() == QLatin1String("macos-lively"))
                ? QStringLiteral("macos-lively")
                : QStringLiteral("subtle");
        if (m_dockMotionPreset != v) {
            m_dockMotionPreset = v;
            emit dockMotionPresetChanged();
        }
    } else if (path == QLatin1String("dock.autoHideEnabled")) {
        const bool v = rawValue.toBool();
        if (m_dockAutoHideEnabled != v) {
            m_dockAutoHideEnabled = v;
            emit dockAutoHideEnabledChanged();
        }
    } else if (path == QLatin1String("dock.dropPulseEnabled")) {
        const bool v = rawValue.toBool();
        if (m_dockDropPulseEnabled != v) {
            m_dockDropPulseEnabled = v;
            emit dockDropPulseEnabledChanged();
        }
    } else if (path == QLatin1String("dock.dragThresholdMouse")) {
        const int v = qBound(2, rawValue.toInt(), 24);
        if (m_dockDragThresholdMouse != v) {
            m_dockDragThresholdMouse = v;
            emit dockDragThresholdMouseChanged();
        }
    } else if (path == QLatin1String("dock.dragThresholdTouchpad")) {
        const int v = qBound(2, rawValue.toInt(), 24);
        if (m_dockDragThresholdTouchpad != v) {
            m_dockDragThresholdTouchpad = v;
            emit dockDragThresholdTouchpadChanged();
        }
    } else if (path == QLatin1String("dock.iconSize")) {
        const QString v = [&]() {
            const QString raw = rawValue.toString().trimmed().toLower();
            if (raw == QLatin1String("small") || raw == QLatin1String("large")) {
                return raw;
            }
            return QStringLiteral("medium");
        }();
        if (m_dockIconSize != v) {
            m_dockIconSize = v;
            emit dockIconSizeChanged();
        }
    } else if (path == QLatin1String("dock.magnificationEnabled")) {
        const bool v = rawValue.toBool();
        if (m_dockMagnificationEnabled != v) {
            m_dockMagnificationEnabled = v;
            emit dockMagnificationEnabledChanged();
        }
    } else if (path == QLatin1String("print.pdfFallbackPrinterId")) {
        const QString v = rawValue.toString().trimmed();
        if (m_printPdfFallbackPrinterId != v) {
            m_printPdfFallbackPrinterId = v;
            emit printPdfFallbackPrinterIdChanged();
        }
    } else if (path == QLatin1String("windowing.animationEnabled")) {
        const bool v = rawValue.toBool();
        if (m_windowingAnimationEnabled != v) {
            m_windowingAnimationEnabled = v;
            emit windowingAnimationEnabledChanged();
        }
    } else if (path == QLatin1String("globalAppearance.animationPreset")) {
        // Map settingsd animationPreset → Theme animationMode: balanced→full, minimal, reduced pass through.
        const QString raw = rawValue.toString().trimmed().toLower();
        const QString mode = (raw == QLatin1String("minimal"))  ? QStringLiteral("minimal")
                           : (raw == QLatin1String("reduced"))  ? QStringLiteral("reduced")
                                                                : QStringLiteral("full");
        if (m_animationMode != mode) {
            m_animationMode = mode;
            emit animationModeChanged();
        }
    } else if (path == QLatin1String("windowing.controlsSide")) {
        const QString v = rawValue.toString().trimmed().toLower() == QLatin1String("left")
                ? QStringLiteral("left")
                : QStringLiteral("right");
        if (m_windowControlsSide != v) {
            m_windowControlsSide = v;
            emit windowControlsSideChanged();
        }
    } else if (path == QLatin1String("fonts.defaultFont")) {
        const QString v = rawValue.toString().trimmed();
        if (m_defaultFont != v) {
            m_defaultFont = v;
            emit defaultFontChanged();
        }
    } else if (path == QLatin1String("fonts.documentFont")) {
        const QString v = rawValue.toString().trimmed();
        if (m_documentFont != v) {
            m_documentFont = v;
            emit documentFontChanged();
        }
    } else if (path == QLatin1String("fonts.monospaceFont")) {
        const QString v = rawValue.toString().trimmed();
        if (m_monospaceFont != v) {
            m_monospaceFont = v;
            emit monospaceFontChanged();
        }
    } else if (path == QLatin1String("fonts.titlebarFont")) {
        const QString v = rawValue.toString().trimmed();
        if (m_titlebarFont != v) {
            m_titlebarFont = v;
            emit titlebarFontChanged();
        }
    } else if (path == QLatin1String("wallpaper.uri")) {
        const QString v = rawValue.toString().trimmed();
        if (m_wallpaperUri != v) {
            m_wallpaperUri = v;
            emit wallpaperUriChanged();
        }
    } else if (isManagedShortcutPath(path)) {
        const QString normalizedPath = path.trimmed();
        const QString v = rawValue.toString().trimmed();
        if (m_keyboardShortcuts.value(normalizedPath).toString() != v) {
            m_keyboardShortcuts.insert(normalizedPath, v);
            emit keyboardShortcutChanged(normalizedPath);
        }
    }
    emit settingChanged(path);
}

void DesktopSettingsClient::onAppearanceModeChanged(const QString &mode)
{
    setThemeModeLocal(mode.trimmed().toLower());
}

bool DesktopSettingsClient::ensureIface()
{
    if (m_iface && m_iface->isValid()) {
        return true;
    }

    delete m_iface;
    m_iface = new QDBusInterface(QLatin1String(kService),
                                 QLatin1String(kPath),
                                 QLatin1String(kIface),
                                 QDBusConnection::sessionBus(),
                                 this);
    const bool nowAvailable = m_iface->isValid();
    if (nowAvailable != m_available) {
        m_available = nowAvailable;
        emit availableChanged();
    }

    if (nowAvailable) {
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.connect(QLatin1String(kService), QLatin1String(kPath), QLatin1String(kIface),
                    QStringLiteral("SettingChanged"),
                    this, SLOT(onSettingChanged(QString,QDBusVariant)));
        bus.connect(QLatin1String(kService), QLatin1String(kPath), QLatin1String(kIface),
                    QStringLiteral("AppearanceModeChanged"),
                    this, SLOT(onAppearanceModeChanged(QString)));
    }
    return nowAvailable;
}

bool DesktopSettingsClient::setSetting(const QString &path, const QVariant &value)
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(
                QStringLiteral("SetSetting"),
                path,
                QVariant::fromValue(QDBusVariant(value)));
    if (!reply.isValid()) {
        return false;
    }
    return reply.value().value(QStringLiteral("ok"), false).toBool();
}

bool DesktopSettingsClient::setFontByPath(const QString &path,
                                          QString &slot,
                                          const QString &spec,
                                          void (DesktopSettingsClient::*signal)())
{
    const QString normalized = spec.trimmed();
    if (setSetting(path, normalized)) {
        if (slot != normalized) {
            slot = normalized;
            emit (this->*signal)();
        }
        return true;
    }
    return false;
}

void DesktopSettingsClient::loadFromService()
{
    if (!ensureIface()) {
        return;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("GetSettings"));
    if (!reply.isValid()) {
        return;
    }

    const QVariantMap root = reply.value();
    if (!root.value(QStringLiteral("ok"), false).toBool()) {
        return;
    }
    const QVariantMap settings = root.value(QStringLiteral("settings")).toMap();
    m_settingsSnapshot = settings;
    const QVariantMap appearance = settings.value(QStringLiteral("globalAppearance")).toMap();
    const QVariantMap gtk = settings.value(QStringLiteral("gtkThemeRouting")).toMap();
    const QVariantMap kde = settings.value(QStringLiteral("kdeThemeRouting")).toMap();
    const QVariantMap appThemePolicy = settings.value(QStringLiteral("appThemePolicy")).toMap();
    const QVariantMap fallback = settings.value(QStringLiteral("fallbackPolicy")).toMap();
    const QVariantMap contextAutomation = settings.value(QStringLiteral("contextAutomation")).toMap();
    const QVariantMap contextTime = settings.value(QStringLiteral("contextTime")).toMap();
    const QVariantMap dock = settings.value(QStringLiteral("dock")).toMap();
    const QVariantMap print = settings.value(QStringLiteral("print")).toMap();
    const QVariantMap windowing = settings.value(QStringLiteral("windowing")).toMap();
    const QVariantMap shortcuts = settings.value(QStringLiteral("shortcuts")).toMap();
    const QVariantMap fonts = settings.value(QStringLiteral("fonts")).toMap();
    const QVariantMap wallpaper = settings.value(QStringLiteral("wallpaper")).toMap();

    const bool highContrastValue = appearance.value(QStringLiteral("highContrast"), false).toBool();
    if (m_highContrast != highContrastValue) {
        m_highContrast = highContrastValue;
        emit highContrastChanged();
    }

    {
        const QString preset = appearance.value(QStringLiteral("animationPreset"),
                                                QStringLiteral("balanced")).toString().trimmed().toLower();
        const QString mode = (preset == QLatin1String("minimal"))  ? QStringLiteral("minimal")
                           : (preset == QLatin1String("reduced"))  ? QStringLiteral("reduced")
                                                                   : QStringLiteral("full");
        if (m_animationMode != mode) {
            m_animationMode = mode;
            emit animationModeChanged();
        }
    }

    setThemeModeLocal(appearance.value(QStringLiteral("colorMode"), QStringLiteral("dark"))
                          .toString().trimmed().toLower());
    setAccentColorLocal(appearance.value(QStringLiteral("accentColor"), QStringLiteral("#0a84ff"))
                            .toString().trimmed());
    setFontScaleLocal(appearance.value(QStringLiteral("uiScale"), 1.0).toDouble());
    setThemeStringLocal(m_gtkThemeLight,
                        gtk.value(QStringLiteral("themeLight"), QString()).toString().trimmed(),
                        &DesktopSettingsClient::gtkThemeLightChanged);
    setThemeStringLocal(m_gtkThemeDark,
                        gtk.value(QStringLiteral("themeDark"), QString()).toString().trimmed(),
                        &DesktopSettingsClient::gtkThemeDarkChanged);
    setThemeStringLocal(m_kdeColorSchemeLight,
                        kde.value(QStringLiteral("themeLight"), QString()).toString().trimmed(),
                        &DesktopSettingsClient::kdeColorSchemeLightChanged);
    setThemeStringLocal(m_kdeColorSchemeDark,
                        kde.value(QStringLiteral("themeDark"), QString()).toString().trimmed(),
                        &DesktopSettingsClient::kdeColorSchemeDarkChanged);
    setThemeStringLocal(m_gtkIconThemeLight,
                        gtk.value(QStringLiteral("iconThemeLight"), QString()).toString().trimmed(),
                        &DesktopSettingsClient::gtkIconThemeLightChanged);
    setThemeStringLocal(m_gtkIconThemeDark,
                        gtk.value(QStringLiteral("iconThemeDark"), QString()).toString().trimmed(),
                        &DesktopSettingsClient::gtkIconThemeDarkChanged);
    setThemeStringLocal(m_kdeIconThemeLight,
                        kde.value(QStringLiteral("iconThemeLight"), QString()).toString().trimmed(),
                        &DesktopSettingsClient::kdeIconThemeLightChanged);
    setThemeStringLocal(m_kdeIconThemeDark,
                        kde.value(QStringLiteral("iconThemeDark"), QString()).toString().trimmed(),
                        &DesktopSettingsClient::kdeIconThemeDarkChanged);

    const bool qtCompat = appThemePolicy.value(QStringLiteral("qtGenericAllowKdeCompat"), true).toBool();
    if (m_qtGenericAllowKdeCompat != qtCompat) {
        m_qtGenericAllowKdeCompat = qtCompat;
        emit qtGenericAllowKdeCompatChanged();
    }
    const bool qtFallback = appThemePolicy.value(QStringLiteral("qtIncompatibleUseDesktopFallback"), true).toBool();
    if (m_qtIncompatibleUseDesktopFallback != qtFallback) {
        m_qtIncompatibleUseDesktopFallback = qtFallback;
        emit qtIncompatibleUseDesktopFallbackChanged();
    }
    const bool unknownFallback = fallback.value(QStringLiteral("unknownUsesSafeFallback"), true).toBool();
    if (m_unknownUsesSafeFallback != unknownFallback) {
        m_unknownUsesSafeFallback = unknownFallback;
        emit unknownUsesSafeFallbackChanged();
    }
    const bool autoReduceAnimation = contextAutomation.value(QStringLiteral("autoReduceAnimation"), true).toBool();
    const bool autoDisableBlur = contextAutomation.value(QStringLiteral("autoDisableBlur"), true).toBool();
    const bool autoDisableHeavyEffects = contextAutomation.value(QStringLiteral("autoDisableHeavyEffects"), true).toBool();
    bool contextChanged = false;
    if (m_contextAutoReduceAnimation != autoReduceAnimation) {
        m_contextAutoReduceAnimation = autoReduceAnimation;
        contextChanged = true;
    }
    if (m_contextAutoDisableBlur != autoDisableBlur) {
        m_contextAutoDisableBlur = autoDisableBlur;
        contextChanged = true;
    }
    if (m_contextAutoDisableHeavyEffects != autoDisableHeavyEffects) {
        m_contextAutoDisableHeavyEffects = autoDisableHeavyEffects;
        contextChanged = true;
    }
    if (contextChanged) {
        emit contextAutomationChanged();
    }

    const QString timeMode = contextTime.value(QStringLiteral("mode"), QStringLiteral("local"))
                                 .toString()
                                 .trimmed()
                                 .toLower();
    const int sunriseHour = qBound(0, contextTime.value(QStringLiteral("sunriseHour"), 6).toInt(), 23);
    const int sunsetHour = qBound(0, contextTime.value(QStringLiteral("sunsetHour"), 18).toInt(), 23);
    bool timeChanged = false;
    if (m_contextTimeMode != timeMode) {
        m_contextTimeMode = timeMode;
        timeChanged = true;
    }
    if (m_contextTimeSunriseHour != sunriseHour) {
        m_contextTimeSunriseHour = sunriseHour;
        timeChanged = true;
    }
    if (m_contextTimeSunsetHour != sunsetHour) {
        m_contextTimeSunsetHour = sunsetHour;
        timeChanged = true;
    }
    if (timeChanged) {
        emit contextTimeChanged();
    }

    const QString dockMotion = (dock.value(QStringLiteral("motionPreset"), QStringLiteral("subtle"))
                                        .toString().trimmed().toLower() == QLatin1String("macos-lively"))
            ? QStringLiteral("macos-lively")
            : QStringLiteral("subtle");
    if (m_dockMotionPreset != dockMotion) {
        m_dockMotionPreset = dockMotion;
        emit dockMotionPresetChanged();
    }
    const bool dockAutoHide = dock.value(QStringLiteral("autoHideEnabled"), false).toBool();
    if (m_dockAutoHideEnabled != dockAutoHide) {
        m_dockAutoHideEnabled = dockAutoHide;
        emit dockAutoHideEnabledChanged();
    }
    const bool dockDropPulse = dock.value(QStringLiteral("dropPulseEnabled"), true).toBool();
    if (m_dockDropPulseEnabled != dockDropPulse) {
        m_dockDropPulseEnabled = dockDropPulse;
        emit dockDropPulseEnabledChanged();
    }
    const int dockMouse = qBound(2, dock.value(QStringLiteral("dragThresholdMouse"), 6).toInt(), 24);
    if (m_dockDragThresholdMouse != dockMouse) {
        m_dockDragThresholdMouse = dockMouse;
        emit dockDragThresholdMouseChanged();
    }
    const int dockTouchpad = qBound(2, dock.value(QStringLiteral("dragThresholdTouchpad"), 3).toInt(), 24);
    if (m_dockDragThresholdTouchpad != dockTouchpad) {
        m_dockDragThresholdTouchpad = dockTouchpad;
        emit dockDragThresholdTouchpadChanged();
    }
    const QString dockIconSize = [&]() {
        const QString raw = dock.value(QStringLiteral("iconSize"), QStringLiteral("medium"))
                                .toString().trimmed().toLower();
        if (raw == QLatin1String("small") || raw == QLatin1String("large")) {
            return raw;
        }
        return QStringLiteral("medium");
    }();
    if (m_dockIconSize != dockIconSize) {
        m_dockIconSize = dockIconSize;
        emit dockIconSizeChanged();
    }
    const bool dockMagnification = dock.value(QStringLiteral("magnificationEnabled"), true).toBool();
    if (m_dockMagnificationEnabled != dockMagnification) {
        m_dockMagnificationEnabled = dockMagnification;
        emit dockMagnificationEnabledChanged();
    }
    const bool windowingAnimation = windowing.value(QStringLiteral("animationEnabled"), true).toBool();
    if (m_windowingAnimationEnabled != windowingAnimation) {
        m_windowingAnimationEnabled = windowingAnimation;
        emit windowingAnimationEnabledChanged();
    }
    const QString windowControlsSide = windowing.value(QStringLiteral("controlsSide"), QStringLiteral("right"))
                                           .toString()
                                           .trimmed()
                                           .toLower() == QLatin1String("left")
            ? QStringLiteral("left")
            : QStringLiteral("right");
    if (m_windowControlsSide != windowControlsSide) {
        m_windowControlsSide = windowControlsSide;
        emit windowControlsSideChanged();
    }

    const QString fallbackPrinterId = print.value(
                QStringLiteral("pdfFallbackPrinterId"),
                QString()).toString().trimmed();
    if (m_printPdfFallbackPrinterId != fallbackPrinterId) {
        m_printPdfFallbackPrinterId = fallbackPrinterId;
        emit printPdfFallbackPrinterIdChanged();
    }

    const QString defaultFont = fonts.value(QStringLiteral("defaultFont"), QString()).toString().trimmed();
    if (m_defaultFont != defaultFont) {
        m_defaultFont = defaultFont;
        emit defaultFontChanged();
    }
    const QString documentFont = fonts.value(QStringLiteral("documentFont"), QString()).toString().trimmed();
    if (m_documentFont != documentFont) {
        m_documentFont = documentFont;
        emit documentFontChanged();
    }
    const QString monospaceFont = fonts.value(QStringLiteral("monospaceFont"), QString()).toString().trimmed();
    if (m_monospaceFont != monospaceFont) {
        m_monospaceFont = monospaceFont;
        emit monospaceFontChanged();
    }
    const QString titlebarFont = fonts.value(QStringLiteral("titlebarFont"), QString()).toString().trimmed();
    if (m_titlebarFont != titlebarFont) {
        m_titlebarFont = titlebarFont;
        emit titlebarFontChanged();
    }
    const QString wallpaperUri = wallpaper.value(QStringLiteral("uri"), QString()).toString().trimmed();
    if (m_wallpaperUri != wallpaperUri) {
        m_wallpaperUri = wallpaperUri;
        emit wallpaperUriChanged();
    }

    for (const QString &path : kManagedShortcutPaths) {
        const QStringList seg = path.split('.');
        if (seg.size() != 2) {
            continue;
        }
        const QString rootKey = seg.at(0);
        const QString childKey = seg.at(1);
        QVariantMap source;
        if (rootKey == QLatin1String("windowing")) {
            source = windowing;
        } else if (rootKey == QLatin1String("shortcuts")) {
            source = shortcuts;
        }
        const QString value = source.value(childKey, QString()).toString().trimmed();
        if (m_keyboardShortcuts.value(path).toString() != value) {
            m_keyboardShortcuts.insert(path, value);
            emit keyboardShortcutChanged(path);
        }
    }
}

QVariant DesktopSettingsClient::valueByPath(const QVariantMap &root,
                                            const QString &path,
                                            bool *ok)
{
    return mapValueByPath(root, path, ok);
}

bool DesktopSettingsClient::setValueByPath(QVariantMap &root,
                                           const QString &path,
                                           const QVariant &value)
{
    return mapSetValueByPath(root, path, value);
}

void DesktopSettingsClient::setThemeModeLocal(const QString &mode)
{
    if (m_themeMode == mode) {
        return;
    }
    m_themeMode = mode;
    emit themeModeChanged();
}

void DesktopSettingsClient::setAccentColorLocal(const QString &color)
{
    if (m_accentColor == color) {
        return;
    }
    m_accentColor = color;
    emit accentColorChanged();
}

void DesktopSettingsClient::setFontScaleLocal(double scale)
{
    if (qFuzzyCompare(m_fontScale, scale)) {
        return;
    }
    m_fontScale = scale;
    emit fontScaleChanged();
}

void DesktopSettingsClient::setThemeStringLocal(QString &slot, const QString &value,
                                                void (DesktopSettingsClient::*signal)())
{
    if (slot == value) {
        return;
    }
    slot = value;
    emit (this->*signal)();
}
