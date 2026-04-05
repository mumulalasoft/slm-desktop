#include "desktopsettingsclient.h"

#include "../../core/prefs/uipreferences.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

namespace {
constexpr auto kService = "org.slm.Desktop.Settings";
constexpr auto kPath = "/org/slm/Desktop/Settings";
constexpr auto kIface = "org.slm.Desktop.Settings";
}

DesktopSettingsClient::DesktopSettingsClient(UIPreferences *fallbackPrefs, QObject *parent)
    : QObject(parent)
    , m_fallbackPrefs(fallbackPrefs)
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setThemeMode(normalized);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setAccentColor(normalized);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setFontScale(normalized);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setGtkThemeLight(v);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setGtkThemeDark(v);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setKdeColorSchemeLight(v);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setKdeColorSchemeDark(v);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setGtkIconThemeLight(v);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setGtkIconThemeDark(v);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setKdeIconThemeLight(v);
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
    if (m_fallbackPrefs) {
        m_fallbackPrefs->setKdeIconThemeDark(v);
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

void DesktopSettingsClient::refresh()
{
    if (ensureIface()) {
        loadFromService();
    } else {
        loadFromFallback();
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

void DesktopSettingsClient::onSettingChanged(const QString &path, const QVariant &value)
{
    if (path == QLatin1String("globalAppearance.colorMode")) {
        setThemeModeLocal(value.toString().trimmed().toLower());
    } else if (path == QLatin1String("globalAppearance.accentColor")) {
        setAccentColorLocal(value.toString().trimmed());
    } else if (path == QLatin1String("globalAppearance.uiScale")) {
        setFontScaleLocal(value.toDouble());
    } else if (path == QLatin1String("gtkThemeRouting.themeLight")) {
        setThemeStringLocal(m_gtkThemeLight, value.toString().trimmed(), &DesktopSettingsClient::gtkThemeLightChanged);
    } else if (path == QLatin1String("gtkThemeRouting.themeDark")) {
        setThemeStringLocal(m_gtkThemeDark, value.toString().trimmed(), &DesktopSettingsClient::gtkThemeDarkChanged);
    } else if (path == QLatin1String("kdeThemeRouting.themeLight")) {
        setThemeStringLocal(m_kdeColorSchemeLight, value.toString().trimmed(), &DesktopSettingsClient::kdeColorSchemeLightChanged);
    } else if (path == QLatin1String("kdeThemeRouting.themeDark")) {
        setThemeStringLocal(m_kdeColorSchemeDark, value.toString().trimmed(), &DesktopSettingsClient::kdeColorSchemeDarkChanged);
    } else if (path == QLatin1String("gtkThemeRouting.iconThemeLight")) {
        setThemeStringLocal(m_gtkIconThemeLight, value.toString().trimmed(), &DesktopSettingsClient::gtkIconThemeLightChanged);
    } else if (path == QLatin1String("gtkThemeRouting.iconThemeDark")) {
        setThemeStringLocal(m_gtkIconThemeDark, value.toString().trimmed(), &DesktopSettingsClient::gtkIconThemeDarkChanged);
    } else if (path == QLatin1String("kdeThemeRouting.iconThemeLight")) {
        setThemeStringLocal(m_kdeIconThemeLight, value.toString().trimmed(), &DesktopSettingsClient::kdeIconThemeLightChanged);
    } else if (path == QLatin1String("kdeThemeRouting.iconThemeDark")) {
        setThemeStringLocal(m_kdeIconThemeDark, value.toString().trimmed(), &DesktopSettingsClient::kdeIconThemeDarkChanged);
    } else if (path == QLatin1String("appThemePolicy.qtGenericAllowKdeCompat")) {
        const bool v = value.toBool();
        if (m_qtGenericAllowKdeCompat != v) {
            m_qtGenericAllowKdeCompat = v;
            emit qtGenericAllowKdeCompatChanged();
        }
    } else if (path == QLatin1String("appThemePolicy.qtIncompatibleUseDesktopFallback")) {
        const bool v = value.toBool();
        if (m_qtIncompatibleUseDesktopFallback != v) {
            m_qtIncompatibleUseDesktopFallback = v;
            emit qtIncompatibleUseDesktopFallbackChanged();
        }
    } else if (path == QLatin1String("fallbackPolicy.unknownUsesSafeFallback")) {
        const bool v = value.toBool();
        if (m_unknownUsesSafeFallback != v) {
            m_unknownUsesSafeFallback = v;
            emit unknownUsesSafeFallbackChanged();
        }
    } else if (path == QLatin1String("contextAutomation.autoReduceAnimation")) {
        const bool v = value.toBool();
        if (m_contextAutoReduceAnimation != v) {
            m_contextAutoReduceAnimation = v;
            emit contextAutomationChanged();
        }
    } else if (path == QLatin1String("contextAutomation.autoDisableBlur")) {
        const bool v = value.toBool();
        if (m_contextAutoDisableBlur != v) {
            m_contextAutoDisableBlur = v;
            emit contextAutomationChanged();
        }
    } else if (path == QLatin1String("contextAutomation.autoDisableHeavyEffects")) {
        const bool v = value.toBool();
        if (m_contextAutoDisableHeavyEffects != v) {
            m_contextAutoDisableHeavyEffects = v;
            emit contextAutomationChanged();
        }
    } else if (path == QLatin1String("contextTime.mode")) {
        const QString v = value.toString().trimmed().toLower();
        if (m_contextTimeMode != v) {
            m_contextTimeMode = v;
            emit contextTimeChanged();
        }
    } else if (path == QLatin1String("contextTime.sunriseHour")) {
        const int v = qBound(0, value.toInt(), 23);
        if (m_contextTimeSunriseHour != v) {
            m_contextTimeSunriseHour = v;
            emit contextTimeChanged();
        }
    } else if (path == QLatin1String("contextTime.sunsetHour")) {
        const int v = qBound(0, value.toInt(), 23);
        if (m_contextTimeSunsetHour != v) {
            m_contextTimeSunsetHour = v;
            emit contextTimeChanged();
        }
    }
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
                    this, SLOT(onSettingChanged(QString,QVariant)));
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
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("SetSetting"), path, value);
    if (!reply.isValid()) {
        return false;
    }
    return reply.value().value(QStringLiteral("ok"), false).toBool();
}

void DesktopSettingsClient::loadFromService()
{
    if (!ensureIface()) {
        loadFromFallback();
        return;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("GetSettings"));
    if (!reply.isValid()) {
        loadFromFallback();
        return;
    }

    const QVariantMap root = reply.value();
    if (!root.value(QStringLiteral("ok"), false).toBool()) {
        loadFromFallback();
        return;
    }
    const QVariantMap settings = root.value(QStringLiteral("settings")).toMap();
    const QVariantMap appearance = settings.value(QStringLiteral("globalAppearance")).toMap();
    const QVariantMap gtk = settings.value(QStringLiteral("gtkThemeRouting")).toMap();
    const QVariantMap kde = settings.value(QStringLiteral("kdeThemeRouting")).toMap();
    const QVariantMap appThemePolicy = settings.value(QStringLiteral("appThemePolicy")).toMap();
    const QVariantMap fallback = settings.value(QStringLiteral("fallbackPolicy")).toMap();
    const QVariantMap contextAutomation = settings.value(QStringLiteral("contextAutomation")).toMap();
    const QVariantMap contextTime = settings.value(QStringLiteral("contextTime")).toMap();

    setThemeModeLocal(appearance.value(QStringLiteral("colorMode"),
                                       m_fallbackPrefs ? m_fallbackPrefs->themeMode() : QStringLiteral("dark"))
                          .toString().trimmed().toLower());
    setAccentColorLocal(appearance.value(QStringLiteral("accentColor"),
                                         m_fallbackPrefs ? m_fallbackPrefs->accentColor() : QStringLiteral("#0a84ff"))
                            .toString().trimmed());
    setFontScaleLocal(appearance.value(QStringLiteral("uiScale"),
                                       m_fallbackPrefs ? m_fallbackPrefs->fontScale() : 1.0)
                          .toDouble());
    setThemeStringLocal(m_gtkThemeLight,
                        gtk.value(QStringLiteral("themeLight"),
                                  m_fallbackPrefs ? m_fallbackPrefs->gtkThemeLight() : QString()).toString().trimmed(),
                        &DesktopSettingsClient::gtkThemeLightChanged);
    setThemeStringLocal(m_gtkThemeDark,
                        gtk.value(QStringLiteral("themeDark"),
                                  m_fallbackPrefs ? m_fallbackPrefs->gtkThemeDark() : QString()).toString().trimmed(),
                        &DesktopSettingsClient::gtkThemeDarkChanged);
    setThemeStringLocal(m_kdeColorSchemeLight,
                        kde.value(QStringLiteral("themeLight"),
                                  m_fallbackPrefs ? m_fallbackPrefs->kdeColorSchemeLight() : QString()).toString().trimmed(),
                        &DesktopSettingsClient::kdeColorSchemeLightChanged);
    setThemeStringLocal(m_kdeColorSchemeDark,
                        kde.value(QStringLiteral("themeDark"),
                                  m_fallbackPrefs ? m_fallbackPrefs->kdeColorSchemeDark() : QString()).toString().trimmed(),
                        &DesktopSettingsClient::kdeColorSchemeDarkChanged);
    setThemeStringLocal(m_gtkIconThemeLight,
                        gtk.value(QStringLiteral("iconThemeLight"),
                                  m_fallbackPrefs ? m_fallbackPrefs->gtkIconThemeLight() : QString()).toString().trimmed(),
                        &DesktopSettingsClient::gtkIconThemeLightChanged);
    setThemeStringLocal(m_gtkIconThemeDark,
                        gtk.value(QStringLiteral("iconThemeDark"),
                                  m_fallbackPrefs ? m_fallbackPrefs->gtkIconThemeDark() : QString()).toString().trimmed(),
                        &DesktopSettingsClient::gtkIconThemeDarkChanged);
    setThemeStringLocal(m_kdeIconThemeLight,
                        kde.value(QStringLiteral("iconThemeLight"),
                                  m_fallbackPrefs ? m_fallbackPrefs->kdeIconThemeLight() : QString()).toString().trimmed(),
                        &DesktopSettingsClient::kdeIconThemeLightChanged);
    setThemeStringLocal(m_kdeIconThemeDark,
                        kde.value(QStringLiteral("iconThemeDark"),
                                  m_fallbackPrefs ? m_fallbackPrefs->kdeIconThemeDark() : QString()).toString().trimmed(),
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
}

void DesktopSettingsClient::loadFromFallback()
{
    if (!m_fallbackPrefs) {
        return;
    }
    setThemeModeLocal(m_fallbackPrefs->themeMode());
    setAccentColorLocal(m_fallbackPrefs->accentColor());
    setFontScaleLocal(m_fallbackPrefs->fontScale());
    setThemeStringLocal(m_gtkThemeLight, m_fallbackPrefs->gtkThemeLight(), &DesktopSettingsClient::gtkThemeLightChanged);
    setThemeStringLocal(m_gtkThemeDark, m_fallbackPrefs->gtkThemeDark(), &DesktopSettingsClient::gtkThemeDarkChanged);
    setThemeStringLocal(m_kdeColorSchemeLight, m_fallbackPrefs->kdeColorSchemeLight(), &DesktopSettingsClient::kdeColorSchemeLightChanged);
    setThemeStringLocal(m_kdeColorSchemeDark, m_fallbackPrefs->kdeColorSchemeDark(), &DesktopSettingsClient::kdeColorSchemeDarkChanged);
    setThemeStringLocal(m_gtkIconThemeLight, m_fallbackPrefs->gtkIconThemeLight(), &DesktopSettingsClient::gtkIconThemeLightChanged);
    setThemeStringLocal(m_gtkIconThemeDark, m_fallbackPrefs->gtkIconThemeDark(), &DesktopSettingsClient::gtkIconThemeDarkChanged);
    setThemeStringLocal(m_kdeIconThemeLight, m_fallbackPrefs->kdeIconThemeLight(), &DesktopSettingsClient::kdeIconThemeLightChanged);
    setThemeStringLocal(m_kdeIconThemeDark, m_fallbackPrefs->kdeIconThemeDark(), &DesktopSettingsClient::kdeIconThemeDarkChanged);
    bool contextChanged = false;
    if (!m_contextAutoReduceAnimation) {
        m_contextAutoReduceAnimation = true;
        contextChanged = true;
    }
    if (!m_contextAutoDisableBlur) {
        m_contextAutoDisableBlur = true;
        contextChanged = true;
    }
    if (!m_contextAutoDisableHeavyEffects) {
        m_contextAutoDisableHeavyEffects = true;
        contextChanged = true;
    }
    if (contextChanged) {
        emit contextAutomationChanged();
    }

    bool timeChanged = false;
    if (m_contextTimeMode != QLatin1String("local")) {
        m_contextTimeMode = QStringLiteral("local");
        timeChanged = true;
    }
    if (m_contextTimeSunriseHour != 6) {
        m_contextTimeSunriseHour = 6;
        timeChanged = true;
    }
    if (m_contextTimeSunsetHour != 18) {
        m_contextTimeSunsetHour = 18;
        timeChanged = true;
    }
    if (timeChanged) {
        emit contextTimeChanged();
    }
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
