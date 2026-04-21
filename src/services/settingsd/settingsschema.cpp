#include "settingsschema.h"

namespace Slm {

namespace {

KeySpec strKey(const QString &k, const QString &def,
               bool restart = false, bool lockable = false)
{
    KeySpec s;
    s.key = k; s.type = SettingsType::String; s.defaultValue = def;
    s.requiresRestart = restart; s.lockableByPolicy = lockable;
    return s;
}

KeySpec boolKey(const QString &k, bool def,
                bool restart = false, bool lockable = false)
{
    KeySpec s;
    s.key = k; s.type = SettingsType::Bool; s.defaultValue = def;
    s.requiresRestart = restart; s.lockableByPolicy = lockable;
    return s;
}

KeySpec intKey(const QString &k, int def, int lo, int hi,
               bool restart = false, bool lockable = false)
{
    KeySpec s;
    s.key = k; s.type = SettingsType::Int; s.defaultValue = def;
    s.hasRange = true; s.rangeMin = lo; s.rangeMax = hi;
    s.requiresRestart = restart; s.lockableByPolicy = lockable;
    return s;
}

KeySpec realKey(const QString &k, double def, double lo, double hi,
                bool restart = false)
{
    KeySpec s;
    s.key = k; s.type = SettingsType::Real; s.defaultValue = def;
    s.hasRange = true; s.rangeMin = lo; s.rangeMax = hi;
    s.requiresRestart = restart;
    return s;
}

KeySpec enumKey(const QString &k, const QString &def, QStringList vals,
                bool restart = false, bool lockable = false)
{
    KeySpec s;
    s.key = k; s.type = SettingsType::String; s.defaultValue = def;
    s.enumValues = std::move(vals);
    s.requiresRestart = restart; s.lockableByPolicy = lockable;
    return s;
}

KeySpec roKey(KeySpec s)
{
    s.userWritable = false;
    return s;
}

} // namespace

// ── Registration ─────────────────────────────────────────────────────────────

void SettingsSchema::reg(KeySpec spec)
{
    m_specs.insert(spec.key, std::move(spec));
}

void SettingsSchema::regDynamic(const QString &prefix)
{
    m_dynamicPrefixes.append(prefix);
}

SettingsSchema::SettingsSchema()
{
    const QStringList themeMode{QStringLiteral("dark"),
                                QStringLiteral("light"),
                                QStringLiteral("auto")};
    const QStringList lrEnum{QStringLiteral("left"), QStringLiteral("right")};

    // ── globalAppearance ─────────────────────────────────────────────────────
    reg(enumKey(QStringLiteral("globalAppearance.colorMode"),
                QStringLiteral("dark"), themeMode));
    reg(strKey(QStringLiteral("globalAppearance.accentColor"),
               QStringLiteral("#4aa3ff")));
    reg(strKey(QStringLiteral("globalAppearance.fontFamily"),
               QStringLiteral("Sans Serif"), /*restart=*/true));
    reg(intKey(QStringLiteral("globalAppearance.fontSize"),  10, 6, 72, true));
    reg(realKey(QStringLiteral("globalAppearance.uiScale"),  1.0, 0.5, 4.0, true));
    reg(enumKey(QStringLiteral("globalAppearance.iconSizeClass"),
                QStringLiteral("medium"),
                {QStringLiteral("small"), QStringLiteral("medium"), QStringLiteral("large")}));
    reg(strKey(QStringLiteral("globalAppearance.cursorTheme"), QStringLiteral("default"), true));
    reg(boolKey(QStringLiteral("globalAppearance.highContrast"), false));

    // ── shellTheme ────────────────────────────────────────────────────────────
    reg(strKey(QStringLiteral("shellTheme.name"), QStringLiteral("slm")));
    reg(enumKey(QStringLiteral("shellTheme.mode"),
                QStringLiteral("dark"), themeMode));
    reg(strKey(QStringLiteral("shellTheme.accentColor"), QStringLiteral("#4aa3ff")));
    reg(intKey(QStringLiteral("shellTheme.radius"), 12, 0, 32));
    reg(boolKey(QStringLiteral("shellTheme.blur"), true));
    reg(enumKey(QStringLiteral("shellTheme.panelStyle"),
                QStringLiteral("floating"),
                {QStringLiteral("floating"), QStringLiteral("solid"), QStringLiteral("minimal")}));
    reg(strKey(QStringLiteral("shellTheme.surfaceStyle"), QStringLiteral("default")));
    reg(enumKey(QStringLiteral("shellTheme.animationPreset"),
                QStringLiteral("balanced"),
                {QStringLiteral("balanced"), QStringLiteral("reduced"), QStringLiteral("none")}));
    reg(boolKey(QStringLiteral("shellTheme.applets.bluetooth"),   false));
    reg(boolKey(QStringLiteral("shellTheme.applets.notification"), false));
    reg(boolKey(QStringLiteral("shellTheme.applets.clipboard"),   false));
    reg(boolKey(QStringLiteral("shellTheme.applets.print"),       false));
    reg(boolKey(QStringLiteral("shellTheme.networkShowIp"),       false));

    // ── gtkThemeRouting ──────────────────────────────────────────────────────
    reg(strKey(QStringLiteral("gtkThemeRouting.themeDark"),      QStringLiteral("SLM-GTK-Dark")));
    reg(strKey(QStringLiteral("gtkThemeRouting.themeLight"),     QStringLiteral("SLM-GTK-Light")));
    reg(strKey(QStringLiteral("gtkThemeRouting.iconThemeDark"),  QStringLiteral("SLM-Icons-Dark")));
    reg(strKey(QStringLiteral("gtkThemeRouting.iconThemeLight"), QStringLiteral("SLM-Icons-Light")));

    // ── kdeThemeRouting ──────────────────────────────────────────────────────
    reg(strKey(QStringLiteral("kdeThemeRouting.themeDark"),      QStringLiteral("SLM-KDE-Dark")));
    reg(strKey(QStringLiteral("kdeThemeRouting.themeLight"),     QStringLiteral("SLM-KDE-Light")));
    reg(strKey(QStringLiteral("kdeThemeRouting.iconThemeDark"),  QStringLiteral("SLM-Icons-Dark")));
    reg(strKey(QStringLiteral("kdeThemeRouting.iconThemeLight"), QStringLiteral("SLM-Icons-Light")));

    // ── appThemePolicy ────────────────────────────────────────────────────────
    reg(boolKey(QStringLiteral("appThemePolicy.gtkAppsUseGtkTheme"),             true,  false, true));
    reg(boolKey(QStringLiteral("appThemePolicy.kdeAppsUseKdeTheme"),             true,  false, true));
    reg(boolKey(QStringLiteral("appThemePolicy.qtGenericAllowKdeCompat"),        true,  false, true));
    reg(boolKey(QStringLiteral("appThemePolicy.qtIncompatibleUseDesktopFallback"), true, false, true));
    reg(boolKey(QStringLiteral("appThemePolicy.shellUsesShellThemeOnly"),        true,  false, true));

    // ── fallbackPolicy ────────────────────────────────────────────────────────
    reg(strKey(QStringLiteral("fallbackPolicy.safeFallbackCategory"),    QStringLiteral("qt-desktop-fallback"), false, true));
    reg(boolKey(QStringLiteral("fallbackPolicy.unknownUsesSafeFallback"), true,                                 false, true));
    reg(strKey(QStringLiteral("fallbackPolicy.desktopFallbackThemeDark"),  QStringLiteral("slm-dark"),  false, true));
    reg(strKey(QStringLiteral("fallbackPolicy.desktopFallbackThemeLight"), QStringLiteral("slm-light"), false, true));
    reg(strKey(QStringLiteral("fallbackPolicy.desktopFallbackIconDark"),   QStringLiteral("SLM-Icons-Dark"),  false, true));
    reg(strKey(QStringLiteral("fallbackPolicy.desktopFallbackIconLight"),  QStringLiteral("SLM-Icons-Light"), false, true));

    // ── contextAutomation ─────────────────────────────────────────────────────
    reg(boolKey(QStringLiteral("contextAutomation.autoReduceAnimation"),    true));
    reg(boolKey(QStringLiteral("contextAutomation.autoDisableBlur"),        true));
    reg(boolKey(QStringLiteral("contextAutomation.autoDisableHeavyEffects"), true));

    // ── contextTime ───────────────────────────────────────────────────────────
    reg(enumKey(QStringLiteral("contextTime.mode"),
                QStringLiteral("local"),
                {QStringLiteral("local"), QStringLiteral("sun")}));
    reg(intKey(QStringLiteral("contextTime.sunriseHour"), 6,  0, 23));
    reg(intKey(QStringLiteral("contextTime.sunsetHour"),  18, 0, 23));

    // ── dock ──────────────────────────────────────────────────────────────────
    reg(enumKey(QStringLiteral("dock.motionPreset"),
                QStringLiteral("subtle"),
                {QStringLiteral("subtle"), QStringLiteral("macos-lively")}));
    reg(boolKey(QStringLiteral("dock.autoHideEnabled"),    false));
    reg(boolKey(QStringLiteral("dock.dropPulseEnabled"),   true));
    reg(boolKey(QStringLiteral("dock.magnificationEnabled"), true));
    reg(enumKey(QStringLiteral("dock.iconSize"),
                QStringLiteral("medium"),
                {QStringLiteral("small"), QStringLiteral("medium"), QStringLiteral("large")}));
    reg(intKey(QStringLiteral("dock.dragThresholdMouse"),                  6,   2,   24));
    reg(intKey(QStringLiteral("dock.dragThresholdTouchpad"),               3,   2,   24));
    reg(intKey(QStringLiteral("dock.desktopExportMinUpwardPx"),           28,   8,   96));
    reg(intKey(QStringLiteral("dock.desktopExportVerticalRatioPercent"), 135, 100,  260));
    reg(intKey(QStringLiteral("dock.desktopExportMaxHorizontalDriftPx"),  42,   8,  140));

    // ── print ─────────────────────────────────────────────────────────────────
    reg(strKey(QStringLiteral("print.pdfFallbackPrinterId"), QStringLiteral("")));

    // ── windowing ─────────────────────────────────────────────────────────────
    reg(boolKey(QStringLiteral("windowing.animationEnabled"), true));
    reg(enumKey(QStringLiteral("windowing.controlsSide"),
                QStringLiteral("right"), lrEnum));
    reg(strKey(QStringLiteral("windowing.bindClose"),       QStringLiteral("Alt+F4")));
    reg(strKey(QStringLiteral("windowing.bindMinimize"),    QStringLiteral("Alt+F9")));
    reg(strKey(QStringLiteral("windowing.bindMaximize"),    QStringLiteral("Alt+Up")));
    reg(strKey(QStringLiteral("windowing.bindTileLeft"),    QStringLiteral("Alt+Left")));
    reg(strKey(QStringLiteral("windowing.bindTileRight"),   QStringLiteral("Alt+Right")));
    reg(strKey(QStringLiteral("windowing.bindSwitchNext"),  QStringLiteral("Alt+F11")));
    reg(strKey(QStringLiteral("windowing.bindSwitchPrev"),  QStringLiteral("Alt+F12")));
    reg(strKey(QStringLiteral("windowing.bindWorkspace"),   QStringLiteral("Alt+F3")));

    // ── shortcuts ─────────────────────────────────────────────────────────────
    reg(strKey(QStringLiteral("shortcuts.workspaceOverview"), QStringLiteral("Meta+S")));
    reg(strKey(QStringLiteral("shortcuts.workspacePrev"),     QStringLiteral("Meta+Left")));
    reg(strKey(QStringLiteral("shortcuts.workspaceNext"),     QStringLiteral("Meta+Right")));
    reg(strKey(QStringLiteral("shortcuts.moveWindowPrev"),    QStringLiteral("Meta+Shift+Left")));
    reg(strKey(QStringLiteral("shortcuts.moveWindowNext"),    QStringLiteral("Meta+Shift+Right")));

    // ── fonts ─────────────────────────────────────────────────────────────────
    reg(strKey(QStringLiteral("fonts.defaultFont"),    QStringLiteral(""), true));
    reg(strKey(QStringLiteral("fonts.documentFont"),   QStringLiteral(""), true));
    reg(strKey(QStringLiteral("fonts.monospaceFont"),  QStringLiteral(""), true));
    reg(strKey(QStringLiteral("fonts.titlebarFont"),   QStringLiteral(""), true));

    // ── wallpaper ─────────────────────────────────────────────────────────────
    reg(strKey(QStringLiteral("wallpaper.uri"), QStringLiteral("")));

    // ── firewall ─────────────────────────────────────────────────────────────
    reg(boolKey(QStringLiteral("firewall.enabled"), true, false, true));
    reg(enumKey(QStringLiteral("firewall.mode"),
                QStringLiteral("home"),
                {QStringLiteral("home"), QStringLiteral("public"), QStringLiteral("custom")},
                false, true));
    reg(enumKey(QStringLiteral("firewall.defaultIncomingPolicy"),
                QStringLiteral("deny"),
                {QStringLiteral("allow"), QStringLiteral("deny"), QStringLiteral("prompt")},
                false, true));
    reg(enumKey(QStringLiteral("firewall.defaultOutgoingPolicy"),
                QStringLiteral("allow"),
                {QStringLiteral("allow"), QStringLiteral("deny"), QStringLiteral("prompt")},
                false, true));
    reg(intKey(QStringLiteral("firewall.promptCooldownSeconds"), 20, 1, 300, false, true));

    // ── appearance (new canonical namespace) ─────────────────────────────────
    reg(enumKey(QStringLiteral("appearance.theme_mode"),
                QStringLiteral("dark"),
                {QStringLiteral("dark"), QStringLiteral("light"), QStringLiteral("system")}));
    reg(strKey(QStringLiteral("appearance.accent_color"), QStringLiteral("#4aa3ff")));

    // ── dock (new canonical keys) ─────────────────────────────────────────────
    reg(intKey(QStringLiteral("dock.icon_size"), 48, 16, 256));
    reg(boolKey(QStringLiteral("dock.auto_hide"), false));
    reg(enumKey(QStringLiteral("dock.position"),
                QStringLiteral("bottom"),
                {QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("bottom")}));

    // ── desktop ────────────────────────────────────────────────────────────────
    reg(strKey(QStringLiteral("desktop.wallpaper_uri"), QStringLiteral("")));

    // ── topbar ────────────────────────────────────────────────────────────────
    reg(boolKey(QStringLiteral("topbar.clock_24h"), false));

    // ── notifications ─────────────────────────────────────────────────────────
    reg(boolKey(QStringLiteral("notifications.do_not_disturb"), false));

    // ── dynamic namespaces (arbitrary sub-keys allowed) ───────────────────────
    regDynamic(QStringLiteral("perAppOverride."));
    regDynamic(QStringLiteral("firewall.networkProfiles."));
    regDynamic(QStringLiteral("firewall.rules."));
}

// ── Public API ────────────────────────────────────────────────────────────────

const SettingsSchema &SettingsSchema::instance()
{
    static SettingsSchema s_instance;
    return s_instance;
}

bool SettingsSchema::hasKey(const QString &key) const
{
    if (m_specs.contains(key))
        return true;
    return isDynamicPrefix(key);
}

const KeySpec *SettingsSchema::spec(const QString &key) const
{
    auto it = m_specs.find(key);
    if (it != m_specs.end())
        return &it.value();
    return nullptr;
}

bool SettingsSchema::isDynamicPrefix(const QString &key) const
{
    for (const QString &prefix : m_dynamicPrefixes) {
        if (key.startsWith(prefix))
            return true;
    }
    return false;
}

QVariant SettingsSchema::builtinDefault(const QString &key) const
{
    const KeySpec *s = spec(key);
    return s ? s->defaultValue : QVariant{};
}

QList<KeySpec> SettingsSchema::allSpecs() const
{
    return m_specs.values();
}

bool SettingsSchema::validate(const QString &key, const QVariant &value,
                               QString *error) const
{
    const KeySpec *s = spec(key);
    if (!s) {
        // Allow dynamic-namespace keys through without type-checking.
        if (isDynamicPrefix(key))
            return true;
        if (error)
            *error = QStringLiteral("unknown settings key: %1").arg(key);
        return false;
    }

    if (!s->userWritable) {
        if (error)
            *error = QStringLiteral("key is read-only: %1").arg(key);
        return false;
    }

    switch (s->type) {
    case SettingsType::Bool:
        if (value.metaType().id() != QMetaType::Bool) {
            if (error)
                *error = QStringLiteral("%1 must be a boolean").arg(key);
            return false;
        }
        break;

    case SettingsType::Int: {
        bool ok = false;
        const int v = value.toInt(&ok);
        if (!ok) {
            if (error)
                *error = QStringLiteral("%1 must be an integer").arg(key);
            return false;
        }
        if (s->hasRange && (v < static_cast<int>(s->rangeMin) || v > static_cast<int>(s->rangeMax))) {
            if (error)
                *error = QStringLiteral("%1 out of range [%2, %3]")
                             .arg(key)
                             .arg(static_cast<int>(s->rangeMin))
                             .arg(static_cast<int>(s->rangeMax));
            return false;
        }
        break;
    }

    case SettingsType::Real: {
        bool ok = false;
        const double v = value.toDouble(&ok);
        if (!ok) {
            if (error)
                *error = QStringLiteral("%1 must be a number").arg(key);
            return false;
        }
        if (s->hasRange && (v < s->rangeMin || v > s->rangeMax)) {
            if (error)
                *error = QStringLiteral("%1 out of range [%2, %3]")
                             .arg(key).arg(s->rangeMin).arg(s->rangeMax);
            return false;
        }
        break;
    }

    case SettingsType::String: {
        if (!value.canConvert<QString>()) {
            if (error)
                *error = QStringLiteral("%1 must be a string").arg(key);
            return false;
        }
        if (!s->enumValues.isEmpty()) {
            const QString v = value.toString().trimmed().toLower();
            if (!s->enumValues.contains(v)) {
                if (error)
                    *error = QStringLiteral("%1 must be one of: %2")
                                 .arg(key, s->enumValues.join(QStringLiteral(", ")));
                return false;
            }
        }
        break;
    }
    }

    return true;
}

} // namespace Slm
