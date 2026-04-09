#include "settingsstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QSaveFile>
#include <functional>

namespace {
constexpr int kSchemaVersion = 1;
}

SettingsStore::SettingsStore()
    : m_root(defaultSettings())
{
}

bool SettingsStore::start(QString *error)
{
    const QString path = storagePath();
    const QFileInfo info(path);
    QDir().mkpath(info.absolutePath());

    QFile file(path);
    if (!file.exists()) {
        return save(error);
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("failed to read settings file: %1").arg(file.errorString());
        }
        m_root = defaultSettings();
        return save(nullptr);
    }

    const QByteArray payload = file.readAll();
    file.close();

    bool ok = false;
    const QVariantMap raw = parseJsonObject(payload, &ok);
    if (!ok) {
        const QString corruptPath = path + QStringLiteral(".corrupt.")
                                    + QString::number(QDateTime::currentSecsSinceEpoch());
        QFile::rename(path, corruptPath);
        QVariantMap recovered;
        QString recoveryError;
        if (loadLastKnownGood(&recovered, &recoveryError)) {
            m_root = normalizeRoot(recovered);
            if (!save(error)) {
                return false;
            }
            return true;
        }
        m_root = defaultSettings();
        return save(error);
    }

    QString validationError;
    if (!validateRoot(raw, &validationError)) {
        QVariantMap recovered;
        QString recoveryError;
        if (loadLastKnownGood(&recovered, &recoveryError)) {
            m_root = normalizeRoot(recovered);
            if (!save(error)) {
                return false;
            }
            return true;
        }
        m_root = defaultSettings();
        return save(error);
    }

    m_root = normalizeRoot(raw);
    if (!save(error)) {
        return false;
    }
    return saveLastKnownGood(nullptr);
}

QVariantMap SettingsStore::settings() const
{
    return m_root;
}

bool SettingsStore::setSetting(const QString &path,
                               const QVariant &value,
                               QStringList *changedPaths,
                               QString *error)
{
    if (path.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("path must not be empty");
        }
        return false;
    }
    if (!validateValue(path, value, error)) {
        return false;
    }

    bool hadCurrent = false;
    const QVariant previousValue = valueByPath(m_root, path, &hadCurrent);
    if (hadCurrent && previousValue == value) {
        if (changedPaths) {
            changedPaths->clear();
        }
        return true;
    }

    QVariantMap next = m_root;
    if (!setValueByPath(next, path, value, error)) {
        return false;
    }
    const QVariantMap before = m_root;
    m_root = normalizeRoot(next);

    if (!save(error)) {
        m_root = before;
        return false;
    }
    if (!saveLastKnownGood(error)) {
        m_root = before;
        save(nullptr);
        return false;
    }

    if (changedPaths) {
        changedPaths->clear();
        changedPaths->append(path);
    }
    return true;
}

bool SettingsStore::setSettingsPatch(const QVariantMap &patch,
                                     QStringList *changedPaths,
                                     QString *error)
{
    if (changedPaths) {
        changedPaths->clear();
    }
    if (patch.isEmpty()) {
        return true;
    }

    QVariantMap next = m_root;
    QStringList localChanged;
    for (auto it = patch.constBegin(); it != patch.constEnd(); ++it) {
        const QString path = it.key().trimmed();
        if (path.isEmpty()) {
            continue;
        }
        if (!validateValue(path, it.value(), error)) {
            return false;
        }
        bool hadCurrent = false;
        const QVariant before = valueByPath(next, path, &hadCurrent);
        if (hadCurrent && before == it.value()) {
            continue;
        }
        if (!setValueByPath(next, path, it.value(), error)) {
            return false;
        }
        localChanged.append(path);
    }

    if (localChanged.isEmpty()) {
        return true;
    }

    const QVariantMap before = m_root;
    m_root = normalizeRoot(next);
    if (!save(error)) {
        m_root = before;
        return false;
    }
    if (!saveLastKnownGood(error)) {
        m_root = before;
        save(nullptr);
        return false;
    }
    if (changedPaths) {
        *changedPaths = localChanged;
    }
    return true;
}

QVariantMap SettingsStore::defaultSettings()
{
    return {
        {QStringLiteral("schemaVersion"), kSchemaVersion},
        {QStringLiteral("globalAppearance"), QVariantMap{
             {QStringLiteral("colorMode"), QStringLiteral("dark")},
             {QStringLiteral("accentColor"), QStringLiteral("#4aa3ff")},
             {QStringLiteral("fontFamily"), QStringLiteral("Sans Serif")},
             {QStringLiteral("fontSize"), 10},
             {QStringLiteral("uiScale"), 1.0},
             {QStringLiteral("iconSizeClass"), QStringLiteral("medium")},
             {QStringLiteral("cursorTheme"), QStringLiteral("default")},
             {QStringLiteral("highContrast"), false},
         }},
        {QStringLiteral("shellTheme"), QVariantMap{
             {QStringLiteral("name"), QStringLiteral("slm")},
             {QStringLiteral("mode"), QStringLiteral("dark")},
             {QStringLiteral("accentColor"), QStringLiteral("#4aa3ff")},
             {QStringLiteral("radius"), 12},
             {QStringLiteral("blur"), true},
             {QStringLiteral("panelStyle"), QStringLiteral("floating")},
             {QStringLiteral("surfaceStyle"), QStringLiteral("default")},
             {QStringLiteral("animationPreset"), QStringLiteral("balanced")},
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
             {QStringLiteral("gtkAppsUseGtkTheme"), true},
             {QStringLiteral("kdeAppsUseKdeTheme"), true},
             {QStringLiteral("qtGenericAllowKdeCompat"), true},
             {QStringLiteral("qtIncompatibleUseDesktopFallback"), true},
             {QStringLiteral("shellUsesShellThemeOnly"), true},
         }},
        {QStringLiteral("fallbackPolicy"), QVariantMap{
             {QStringLiteral("safeFallbackCategory"), QStringLiteral("qt-desktop-fallback")},
             {QStringLiteral("unknownUsesSafeFallback"), true},
             {QStringLiteral("desktopFallbackThemeDark"), QStringLiteral("slm-dark")},
             {QStringLiteral("desktopFallbackThemeLight"), QStringLiteral("slm-light")},
             {QStringLiteral("desktopFallbackIconDark"), QStringLiteral("SLM-Icons-Dark")},
             {QStringLiteral("desktopFallbackIconLight"), QStringLiteral("SLM-Icons-Light")},
         }},
        {QStringLiteral("contextAutomation"), QVariantMap{
             {QStringLiteral("autoReduceAnimation"), true},
             {QStringLiteral("autoDisableBlur"), true},
             {QStringLiteral("autoDisableHeavyEffects"), true},
         }},
        {QStringLiteral("contextTime"), QVariantMap{
             {QStringLiteral("mode"), QStringLiteral("local")},
             {QStringLiteral("sunriseHour"), 6},
             {QStringLiteral("sunsetHour"), 18},
         }},
        {QStringLiteral("dock"), QVariantMap{
             {QStringLiteral("motionPreset"), QStringLiteral("subtle")},
             {QStringLiteral("autoHideEnabled"), false},
             {QStringLiteral("dropPulseEnabled"), true},
             {QStringLiteral("dragThresholdMouse"), 6},
             {QStringLiteral("dragThresholdTouchpad"), 3},
             {QStringLiteral("iconSize"), QStringLiteral("medium")},
             {QStringLiteral("magnificationEnabled"), true},
         }},
        {QStringLiteral("print"), QVariantMap{
             {QStringLiteral("pdfFallbackPrinterId"), QStringLiteral("")},
         }},
        {QStringLiteral("windowing"), QVariantMap{
             {QStringLiteral("animationEnabled"), true},
             {QStringLiteral("controlsSide"), QStringLiteral("right")},
             {QStringLiteral("bindClose"), QStringLiteral("Alt+F4")},
             {QStringLiteral("bindMinimize"), QStringLiteral("Alt+F9")},
             {QStringLiteral("bindMaximize"), QStringLiteral("Alt+Up")},
             {QStringLiteral("bindTileLeft"), QStringLiteral("Alt+Left")},
             {QStringLiteral("bindTileRight"), QStringLiteral("Alt+Right")},
             {QStringLiteral("bindSwitchNext"), QStringLiteral("Alt+F11")},
             {QStringLiteral("bindSwitchPrev"), QStringLiteral("Alt+F12")},
             {QStringLiteral("bindWorkspace"), QStringLiteral("Alt+F3")},
         }},
        {QStringLiteral("shortcuts"), QVariantMap{
             {QStringLiteral("workspaceOverview"), QStringLiteral("Meta+S")},
             {QStringLiteral("workspacePrev"), QStringLiteral("Meta+Left")},
             {QStringLiteral("workspaceNext"), QStringLiteral("Meta+Right")},
             {QStringLiteral("moveWindowPrev"), QStringLiteral("Meta+Shift+Left")},
             {QStringLiteral("moveWindowNext"), QStringLiteral("Meta+Shift+Right")},
         }},
        {QStringLiteral("fonts"), QVariantMap{
             {QStringLiteral("defaultFont"), QStringLiteral("")},
             {QStringLiteral("documentFont"), QStringLiteral("")},
             {QStringLiteral("monospaceFont"), QStringLiteral("")},
             {QStringLiteral("titlebarFont"), QStringLiteral("")},
         }},
        {QStringLiteral("wallpaper"), QVariantMap{
             {QStringLiteral("uri"), QStringLiteral("")},
         }},
        {QStringLiteral("firewall"), QVariantMap{
             {QStringLiteral("enabled"), true},
             {QStringLiteral("mode"), QStringLiteral("home")},
             {QStringLiteral("defaultIncomingPolicy"), QStringLiteral("deny")},
             {QStringLiteral("defaultOutgoingPolicy"), QStringLiteral("allow")},
             {QStringLiteral("networkProfiles"), QVariantMap{}},
             {QStringLiteral("rules"), QVariantMap{
                  {QStringLiteral("apps"), QVariantMap{}},
                  {QStringLiteral("ipBlocks"), QVariantMap{}},
              }},
         }},
        {QStringLiteral("perAppOverride"), QVariantMap{}},
    };
}

QVariantMap SettingsStore::normalizeRoot(const QVariantMap &raw)
{
    QVariantMap normalized = defaultSettings();

    int sourceVersion = raw.value(QStringLiteral("schemaVersion"), 0).toInt();
    if (sourceVersion <= 0) {
        sourceVersion = 1;
    }

    QVariantMap migrated = raw;
    if (sourceVersion < kSchemaVersion) {
        // Reserved migration hook for future schema updates.
        migrated.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    }

    for (auto it = migrated.constBegin(); it != migrated.constEnd(); ++it) {
        normalized.insert(it.key(), it.value());
    }
    normalized.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    return normalized;
}

bool SettingsStore::validateRoot(const QVariantMap &raw, QString *error)
{
    const int schemaVersion = raw.value(QStringLiteral("schemaVersion"), 1).toInt();
    if (schemaVersion <= 0) {
        if (error) {
            *error = QStringLiteral("invalid schemaVersion");
        }
        return false;
    }

    const QStringList requiredObjects{
        QStringLiteral("globalAppearance"),
        QStringLiteral("shellTheme"),
        QStringLiteral("gtkThemeRouting"),
        QStringLiteral("kdeThemeRouting"),
        QStringLiteral("appThemePolicy"),
        QStringLiteral("fallbackPolicy"),
        QStringLiteral("contextAutomation"),
        QStringLiteral("contextTime"),
        QStringLiteral("dock"),
        QStringLiteral("print"),
        QStringLiteral("windowing"),
        QStringLiteral("shortcuts"),
        QStringLiteral("fonts"),
        QStringLiteral("wallpaper"),
        QStringLiteral("firewall"),
        QStringLiteral("perAppOverride"),
    };

    for (const QString &key : requiredObjects) {
        const QVariant value = raw.value(key);
        if (value.isNull()) {
            continue;
        }
        if (!value.canConvert<QVariantMap>()) {
            if (error) {
                *error = QStringLiteral("root key '%1' must be an object").arg(key);
            }
            return false;
        }
    }
    return true;
}

bool SettingsStore::validateValue(const QString &path, const QVariant &value, QString *error)
{
    if (path == QStringLiteral("globalAppearance.colorMode")
            || path == QStringLiteral("shellTheme.mode")) {
        const QString mode = value.toString().trimmed();
        if (mode != QStringLiteral("dark")
                && mode != QStringLiteral("light")
                && mode != QStringLiteral("auto")) {
            if (error) {
                *error = QStringLiteral("invalid mode: expected dark/light/auto");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("globalAppearance.uiScale")) {
        const double v = value.toDouble();
        if (v < 0.5 || v > 4.0) {
            if (error) {
                *error = QStringLiteral("uiScale out of supported range 0.5..4.0");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("contextAutomation.autoReduceAnimation")
            || path == QStringLiteral("contextAutomation.autoDisableBlur")
            || path == QStringLiteral("contextAutomation.autoDisableHeavyEffects")) {
        if (value.metaType().id() != QMetaType::Bool) {
            if (error) {
                *error = QStringLiteral("context automation flags must be boolean");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("contextTime.mode")) {
        const QString mode = value.toString().trimmed().toLower();
        if (mode != QStringLiteral("local") && mode != QStringLiteral("sun")) {
            if (error) {
                *error = QStringLiteral("invalid context time mode: expected local/sun");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("contextTime.sunriseHour")
            || path == QStringLiteral("contextTime.sunsetHour")) {
        bool ok = false;
        const int hour = value.toInt(&ok);
        if (!ok || hour < 0 || hour > 23) {
            if (error) {
                *error = QStringLiteral("invalid hour: expected integer in range 0..23");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("globalAppearance.highContrast")) {
        if (value.metaType().id() != QMetaType::Bool) {
            if (error) {
                *error = QStringLiteral("highContrast must be boolean");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("globalAppearance.fontSize")) {
        const int v = value.toInt();
        if (v < 6 || v > 72) {
            if (error) {
                *error = QStringLiteral("fontSize out of supported range 6..72");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("dock.motionPreset")) {
        const QString v = value.toString().trimmed().toLower();
        if (v != QStringLiteral("subtle") && v != QStringLiteral("macos-lively")) {
            if (error) {
                *error = QStringLiteral("invalid dock motion preset: expected subtle/macos-lively");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("dock.autoHideEnabled")
            || path == QStringLiteral("dock.dropPulseEnabled")
            || path == QStringLiteral("dock.magnificationEnabled")) {
        if (value.metaType().id() != QMetaType::Bool) {
            if (error) {
                *error = QStringLiteral("dock flags must be boolean");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("dock.iconSize")) {
        const QString v = value.toString().trimmed().toLower();
        if (v != QStringLiteral("small")
                && v != QStringLiteral("medium")
                && v != QStringLiteral("large")) {
            if (error) {
                *error = QStringLiteral("invalid dock icon size: expected small/medium/large");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("dock.dragThresholdMouse")
            || path == QStringLiteral("dock.dragThresholdTouchpad")) {
        bool ok = false;
        const int v = value.toInt(&ok);
        if (!ok || v < 2 || v > 24) {
            if (error) {
                *error = QStringLiteral("dock drag threshold out of supported range 2..24");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("print.pdfFallbackPrinterId")) {
        if (!value.canConvert<QString>()) {
            if (error) {
                *error = QStringLiteral("pdfFallbackPrinterId must be a string");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("windowing.animationEnabled")) {
        if (value.metaType().id() != QMetaType::Bool) {
            if (error) {
                *error = QStringLiteral("windowing.animationEnabled must be boolean");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("windowing.controlsSide")) {
        const QString v = value.toString().trimmed().toLower();
        if (v != QStringLiteral("left") && v != QStringLiteral("right")) {
            if (error) {
                *error = QStringLiteral("windowing.controlsSide must be left/right");
            }
            return false;
        }
        return true;
    }
    if (path.startsWith(QStringLiteral("windowing.bind"))
            || path.startsWith(QStringLiteral("shortcuts."))) {
        const QString v = value.toString().trimmed();
        if (v.isEmpty()) {
            if (error) {
                *error = QStringLiteral("shortcut value must be a non-empty string");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("fonts.defaultFont")
            || path == QStringLiteral("fonts.documentFont")
            || path == QStringLiteral("fonts.monospaceFont")
            || path == QStringLiteral("fonts.titlebarFont")) {
        if (value.metaType().id() != QMetaType::QString) {
            if (error) {
                *error = QStringLiteral("font spec must be a string");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("wallpaper.uri")) {
        if (value.metaType().id() != QMetaType::QString) {
            if (error) {
                *error = QStringLiteral("wallpaper uri must be a string");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("firewall.enabled")) {
        if (value.metaType().id() != QMetaType::Bool) {
            if (error) {
                *error = QStringLiteral("firewall.enabled must be boolean");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("firewall.mode")) {
        const QString mode = value.toString().trimmed().toLower();
        if (mode != QStringLiteral("home")
                && mode != QStringLiteral("public")
                && mode != QStringLiteral("custom")) {
            if (error) {
                *error = QStringLiteral("firewall.mode must be home/public/custom");
            }
            return false;
        }
        return true;
    }
    if (path == QStringLiteral("firewall.defaultIncomingPolicy")
            || path == QStringLiteral("firewall.defaultOutgoingPolicy")) {
        const QString policy = value.toString().trimmed().toLower();
        if (policy != QStringLiteral("allow")
                && policy != QStringLiteral("deny")
                && policy != QStringLiteral("prompt")) {
            if (error) {
                *error = QStringLiteral("firewall default policy must be allow/deny/prompt");
            }
            return false;
        }
        return true;
    }
    return true;
}

QVariantMap SettingsStore::parseJsonObject(const QByteArray &json, bool *ok)
{
    if (ok) {
        *ok = false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    if (ok) {
        *ok = true;
    }
    return doc.object().toVariantMap();
}

QVariant SettingsStore::valueByPath(const QVariantMap &root, const QString &path, bool *ok)
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
        const QVariantMap map = current.toMap();
        if (!map.contains(segments.at(i))) {
            return {};
        }
        current = map.value(segments.at(i));
        if (i < segments.size() - 1 && !current.canConvert<QVariantMap>()) {
            return {};
        }
    }
    if (ok) {
        *ok = true;
    }
    return current;
}

bool SettingsStore::setValueByPath(QVariantMap &root,
                                   const QString &path,
                                   const QVariant &value,
                                   QString *error)
{
    const QStringList segments = path.split('.', Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        if (error) {
            *error = QStringLiteral("invalid path");
        }
        return false;
    }

    std::function<bool(QVariantMap &, int)> assign = [&](QVariantMap &node, int index) -> bool {
        const QString key = segments.at(index);
        if (index == segments.size() - 1) {
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
            if (error) {
                *error = QStringLiteral("path segment is not an object: %1").arg(key);
            }
            return false;
        }
        if (!assign(childMap, index + 1)) {
            return false;
        }
        node.insert(key, childMap);
        return true;
    };

    if (!assign(root, 0)) {
        return false;
    }
    return true;
}

bool SettingsStore::save(QString *error)
{
    QSaveFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("failed to write settings file: %1").arg(file.errorString());
        }
        return false;
    }
    const QJsonDocument doc(QJsonObject::fromVariantMap(m_root));
    file.write(doc.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("failed to commit settings file");
        }
        return false;
    }
    return true;
}

bool SettingsStore::saveLastKnownGood(QString *error)
{
    QSaveFile file(lastKnownGoodPath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("failed to write lastKnownGood file: %1").arg(file.errorString());
        }
        return false;
    }
    const QJsonDocument doc(QJsonObject::fromVariantMap(m_root));
    file.write(doc.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("failed to commit lastKnownGood file");
        }
        return false;
    }
    return true;
}

bool SettingsStore::loadLastKnownGood(QVariantMap *out, QString *error) const
{
    if (!out) {
        if (error) {
            *error = QStringLiteral("internal error: output is null");
        }
        return false;
    }
    QFile file(lastKnownGoodPath());
    if (!file.exists()) {
        if (error) {
            *error = QStringLiteral("lastKnownGood file does not exist");
        }
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("failed to read lastKnownGood file: %1").arg(file.errorString());
        }
        return false;
    }
    bool ok = false;
    const QVariantMap raw = parseJsonObject(file.readAll(), &ok);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("lastKnownGood file is invalid");
        }
        return false;
    }
    QString validationError;
    if (!validateRoot(raw, &validationError)) {
        if (error) {
            *error = QStringLiteral("lastKnownGood validation failed: %1").arg(validationError);
        }
        return false;
    }
    *out = raw;
    return true;
}

QString SettingsStore::storagePath() const
{
    const QString overridePath = qEnvironmentVariable("SLM_SETTINGSD_STORE_PATH").trimmed();
    if (!overridePath.isEmpty()) {
        return overridePath;
    }
    const QString base = QDir::homePath() + QStringLiteral("/.config/slm-desktop/settings");
    return base + QStringLiteral("/settings.json");
}

QString SettingsStore::lastKnownGoodPath() const
{
    const QString primary = storagePath();
    if (primary.endsWith(QStringLiteral(".json"))) {
        QString alt = primary;
        alt.chop(5);
        alt += QStringLiteral(".last-good.json");
        return alt;
    }
    return primary + QStringLiteral(".last-good.json");
}
