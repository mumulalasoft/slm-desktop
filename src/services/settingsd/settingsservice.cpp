#include "settingsservice.h"
#include "src/services/theme-policy/themepolicyengine.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QMetaType>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Settings";
constexpr const char kPath[] = "/org/slm/Desktop/Settings";
constexpr const char kApiVersion[] = "1.0";
}

SettingsService::SettingsService(QObject *parent)
    : QObject(parent)
{
}

SettingsService::~SettingsService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool SettingsService::start(QString *error)
{
    if (!m_store.start(error)) {
        return false;
    }
    registerDbusService();
    return m_serviceRegistered;
}

bool SettingsService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString SettingsService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap SettingsService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_serviceRegistered},
        {QStringLiteral("apiVersion"), apiVersion()},
    };
}

QVariantMap SettingsService::GetSettings() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("settings"), m_store.settings()},
    };
}

QVariantMap SettingsService::SetSetting(const QString &path, const QDBusVariant &value)
{
    QString error;
    QStringList changedPaths;
    if (!m_store.setSetting(path, value.variant(), &changedPaths, &error)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), error},
        };
    }

    for (const QString &changedPath : std::as_const(changedPaths)) {
        bool found = false;
        const QVariant current = SettingsStore::valueByPath(m_store.settings(), changedPath, &found);
        emit SettingChanged(changedPath, QDBusVariant(found ? current : QVariant{}));
    }
    emitSemanticSignals(changedPaths);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("changedPaths"), changedPaths},
    };
}

QVariantMap SettingsService::SetSettingsPatch(const QVariantMap &patch)
{
    QString error;
    QStringList changedPaths;
    if (!m_store.setSettingsPatch(patch, &changedPaths, &error)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), error},
        };
    }

    for (const QString &changedPath : std::as_const(changedPaths)) {
        bool found = false;
        const QVariant current = SettingsStore::valueByPath(m_store.settings(), changedPath, &found);
        emit SettingChanged(changedPath, QDBusVariant(found ? current : QVariant{}));
    }
    emitSemanticSignals(changedPaths);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("changedPaths"), changedPaths},
    };
}

QVariantMap SettingsService::SubscribeChanges() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("signals"), QStringList{
             QStringLiteral("SettingChanged"),
             QStringLiteral("AppearanceModeChanged"),
             QStringLiteral("ThemePolicyChanged"),
             QStringLiteral("IconPolicyChanged"),
         }},
    };
}

QVariantMap SettingsService::ClassifyApp(const QVariantMap &appDescriptor) const
{
    const QVariantMap settings = m_store.settings();
    const auto descriptor = appDescriptorFromVariant(appDescriptor);
    const auto rules = rulesFromSettings(settings);
    const auto category = Slm::ThemePolicy::AppThemeClassifier::classify(descriptor, rules);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("category"), Slm::ThemePolicy::AppThemeClassifier::categoryToString(category)},
    };
}

QVariantMap SettingsService::ResolveThemeForApp(const QVariantMap &appDescriptor,
                                                const QVariantMap &options)
{
    const QVariantMap settings = m_store.settings();
    const auto descriptor = appDescriptorFromVariant(appDescriptor);
    const auto rules = rulesFromSettings(settings);
    auto category = Slm::ThemePolicy::AppThemeClassifier::classify(descriptor, rules);

    Slm::ThemePolicy::ThemeResolveInput input;
    input.settings = settings;
    input.qtKdeCompatible = options.value(QStringLiteral("qtKdeCompatible"), false).toBool();
    const auto resolved = Slm::ThemePolicy::ThemePolicyEngine::resolve(category, input);

    QVariantMap out = Slm::ThemePolicy::ThemePolicyEngine::toVariantMap(resolved);
    out.insert(QStringLiteral("ok"), true);
    return out;
}

QString SettingsService::ServiceVersion() const
{
    return apiVersion();
}

Slm::ThemePolicy::AppDescriptor SettingsService::appDescriptorFromVariant(const QVariantMap &map)
{
    Slm::ThemePolicy::AppDescriptor app;
    app.appId = map.value(QStringLiteral("appId")).toString().trimmed();
    app.desktopFileId = map.value(QStringLiteral("desktopFileId")).toString().trimmed();
    app.executable = map.value(QStringLiteral("executable")).toString().trimmed();
    app.categories = map.value(QStringLiteral("categories")).toStringList();
    app.desktopKeys = map.value(QStringLiteral("desktopKeys")).toStringList();
    return app;
}

Slm::ThemePolicy::ClassificationRules SettingsService::rulesFromSettings(const QVariantMap &settings)
{
    Slm::ThemePolicy::ClassificationRules rules;
    const QVariantMap overrides = settings.value(QStringLiteral("perAppOverride")).toMap();
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        const QString ruleKey = it.key().trimmed().toLower();
        if (ruleKey.isEmpty()) {
            continue;
        }
        const QVariant value = it.value();
        if (value.userType() == QMetaType::fromType<QString>().id()) {
            const auto category = Slm::ThemePolicy::AppThemeClassifier::categoryFromString(value.toString());
            if (category != Slm::ThemePolicy::AppThemeCategory::Unknown) {
                rules.byAppId.insert(ruleKey, category);
            }
            continue;
        }
        const QVariantMap rule = value.toMap();
        const auto category = Slm::ThemePolicy::AppThemeClassifier::categoryFromString(
            rule.value(QStringLiteral("category")).toString());
        if (category == Slm::ThemePolicy::AppThemeCategory::Unknown) {
            continue;
        }
        const QString appId = rule.value(QStringLiteral("matchAppId")).toString().trimmed().toLower();
        const QString desktopId = rule.value(QStringLiteral("matchDesktopFileId")).toString().trimmed().toLower();
        const QString exec = rule.value(QStringLiteral("matchExecutable")).toString().trimmed().toLower();
        if (!appId.isEmpty()) {
            rules.byAppId.insert(appId, category);
        }
        if (!desktopId.isEmpty()) {
            rules.byDesktopFileId.insert(desktopId, category);
        }
        if (!exec.isEmpty()) {
            rules.byExecutable.insert(exec, category);
        }
    }
    return rules;
}

void SettingsService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllSignals |
                                           QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

void SettingsService::emitSemanticSignals(const QStringList &changedPaths)
{
    bool appearanceChanged = false;
    bool iconPolicyChanged = false;
    bool themePolicyChanged = false;

    for (const QString &path : changedPaths) {
        if (path.startsWith(QStringLiteral("globalAppearance.colorMode"))
                || path.startsWith(QStringLiteral("shellTheme.mode"))) {
            appearanceChanged = true;
            themePolicyChanged = true;
            iconPolicyChanged = true;
            continue;
        }
        if (path.startsWith(QStringLiteral("gtkThemeRouting."))
                || path.startsWith(QStringLiteral("kdeThemeRouting."))
                || path.startsWith(QStringLiteral("shellTheme."))
                || path.startsWith(QStringLiteral("fallbackPolicy."))) {
            themePolicyChanged = true;
            if (path.contains(QStringLiteral("icon"), Qt::CaseInsensitive)) {
                iconPolicyChanged = true;
            }
            continue;
        }
        if (path.startsWith(QStringLiteral("appThemePolicy."))
                || path.startsWith(QStringLiteral("perAppOverride."))) {
            themePolicyChanged = true;
        }
    }

    const QVariantMap root = m_store.settings();
    if (appearanceChanged) {
        const QString mode = root.value(QStringLiteral("globalAppearance")).toMap()
                                 .value(QStringLiteral("colorMode")).toString();
        emit AppearanceModeChanged(mode);
    }
    if (themePolicyChanged) {
        emit ThemePolicyChanged(root.value(QStringLiteral("appThemePolicy")).toMap());
    }
    if (iconPolicyChanged) {
        QVariantMap iconPolicy;
        iconPolicy.insert(QStringLiteral("gtk"), root.value(QStringLiteral("gtkThemeRouting")).toMap());
        iconPolicy.insert(QStringLiteral("kde"), root.value(QStringLiteral("kdeThemeRouting")).toMap());
        iconPolicy.insert(QStringLiteral("fallback"), root.value(QStringLiteral("fallbackPolicy")).toMap());
        emit IconPolicyChanged(iconPolicy);
    }
}
