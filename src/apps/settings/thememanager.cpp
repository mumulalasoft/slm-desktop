#include "thememanager.h"
#include "../../core/prefs/uipreferences.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

// ── XDG helper ───────────────────────────────────────────────────────────────

QStringList ThemeManager::xdgDataDirs()
{
    QStringList dirs;

    // XDG_DATA_HOME (default: ~/.local/share)
    const QString dataHome = qEnvironmentVariable(
        "XDG_DATA_HOME",
        QDir::homePath() + QStringLiteral("/.local/share"));
    dirs << dataHome;

    // XDG_DATA_DIRS (default: /usr/local/share:/usr/share)
    const QString dataDirs = qEnvironmentVariable(
        "XDG_DATA_DIRS",
        QStringLiteral("/usr/local/share:/usr/share"));
    for (const QString &d : dataDirs.split(u':', Qt::SkipEmptyParts))
        dirs << d.trimmed();

    return dirs;
}

// ── Scanner: GTK themes ───────────────────────────────────────────────────────

QStringList ThemeManager::scanGtkThemes()
{
    QSet<QString> seen;
    QStringList result;

    // Standard locations: XDG data dirs + ~/.themes (legacy)
    QStringList searchRoots;
    for (const QString &base : xdgDataDirs())
        searchRoots << base + QStringLiteral("/themes");
    searchRoots << QDir::homePath() + QStringLiteral("/.themes");

    for (const QString &root : searchRoots) {
        QDir dir(root);
        if (!dir.exists())
            continue;
        const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &name : entries) {
            if (seen.contains(name))
                continue;
            // A valid GTK theme directory must contain a gtk-3.0 or gtk-4.0
            // subdirectory so we don't surface icon/cursor theme directories.
            const QDir themeDir(root + u'/' + name);
            if (themeDir.exists(QStringLiteral("gtk-3.0")) ||
                themeDir.exists(QStringLiteral("gtk-4.0"))) {
                seen.insert(name);
                result << name;
            }
        }
    }

    result.sort(Qt::CaseInsensitive);
    return result;
}

// ── Scanner: Icon themes ──────────────────────────────────────────────────────

QStringList ThemeManager::scanIconThemes()
{
    QSet<QString> seen;
    QStringList result;

    // Internal/fallback theme names that are not user-facing.
    static const QSet<QString> kSkip = {
        QStringLiteral("default"), QStringLiteral("locolor")
    };

    QStringList searchRoots;
    for (const QString &base : xdgDataDirs())
        searchRoots << base + QStringLiteral("/icons");
    searchRoots << QDir::homePath() + QStringLiteral("/.icons");

    for (const QString &root : searchRoots) {
        QDir dir(root);
        if (!dir.exists())
            continue;
        const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &name : entries) {
            if (kSkip.contains(name) || seen.contains(name))
                continue;
            // A valid icon theme must have an index.theme file.
            if (QFileInfo::exists(root + u'/' + name + QStringLiteral("/index.theme"))) {
                seen.insert(name);
                result << name;
            }
        }
    }

    result.sort(Qt::CaseInsensitive);
    return result;
}

// ── Scanner: KDE color schemes ────────────────────────────────────────────────

QStringList ThemeManager::scanKdeColorSchemes()
{
    QSet<QString> seen;
    QStringList result;

    for (const QString &base : xdgDataDirs()) {
        QDir dir(base + QStringLiteral("/color-schemes"));
        if (!dir.exists())
            continue;
        const auto entries = dir.entryInfoList({QStringLiteral("*.colors")}, QDir::Files, QDir::Name);
        for (const QFileInfo &fi : entries) {
            const QString name = fi.completeBaseName();
            if (!seen.contains(name)) {
                seen.insert(name);
                result << name;
            }
        }
    }

    result.sort(Qt::CaseInsensitive);
    return result;
}

// ── Apply helpers ─────────────────────────────────────────────────────────────

// Write gtk-theme-name to ~/.config/gtk-{3,4}.0/settings.ini.
// Creates the file (and directory) if absent; leaves other keys intact.
void ThemeManager::applyGtkTheme(const QString &themeName)
{
    if (themeName.isEmpty())
        return;

    const QString configBase = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QStringList gtkDirs = {
        configBase + QStringLiteral("/gtk-3.0"),
        configBase + QStringLiteral("/gtk-4.0"),
    };

    for (const QString &gtkDir : gtkDirs) {
        QDir().mkpath(gtkDir);
        QSettings ini(gtkDir + QStringLiteral("/settings.ini"), QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("Settings"));
        ini.setValue(QStringLiteral("gtk-theme-name"), themeName);
        ini.endGroup();
        ini.sync();
    }
}

// Write gtk-icon-theme-name to ~/.config/gtk-{3,4}.0/settings.ini.
void ThemeManager::applyGtkIconTheme(const QString &themeName)
{
    if (themeName.isEmpty())
        return;

    const QString configBase = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QStringList gtkDirs = {
        configBase + QStringLiteral("/gtk-3.0"),
        configBase + QStringLiteral("/gtk-4.0"),
    };

    for (const QString &gtkDir : gtkDirs) {
        QDir().mkpath(gtkDir);
        QSettings ini(gtkDir + QStringLiteral("/settings.ini"), QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("Settings"));
        ini.setValue(QStringLiteral("gtk-icon-theme-name"), themeName);
        ini.endGroup();
        ini.sync();
    }
}

// Write icon theme name to ~/.config/kdeglobals [Icons] section.
void ThemeManager::applyKdeIconTheme(const QString &themeName)
{
    if (themeName.isEmpty())
        return;

    const QString configBase = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QSettings kdeglobals(configBase + QStringLiteral("/kdeglobals"), QSettings::IniFormat);
    kdeglobals.beginGroup(QStringLiteral("Icons"));
    kdeglobals.setValue(QStringLiteral("Theme"), themeName);
    kdeglobals.endGroup();
    kdeglobals.sync();
}

// Write ColorScheme to ~/.config/kdeglobals [General] section.
void ThemeManager::applyKdeColorScheme(const QString &schemeName)
{
    if (schemeName.isEmpty())
        return;

    const QString configBase = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QSettings kdeglobals(configBase + QStringLiteral("/kdeglobals"), QSettings::IniFormat);
    kdeglobals.beginGroup(QStringLiteral("General"));
    kdeglobals.setValue(QStringLiteral("ColorScheme"), schemeName);
    kdeglobals.endGroup();
    kdeglobals.sync();
}

// ── Constructor ───────────────────────────────────────────────────────────────

ThemeManager::ThemeManager(UIPreferences *prefs, QObject *parent)
    : QObject(parent)
    , m_prefs(prefs)
    , m_gtkThemes(scanGtkThemes())
    , m_kdeColorSchemes(scanKdeColorSchemes())
    , m_iconThemes(scanIconThemes())
{
    connectPrefs();
}

void ThemeManager::connectPrefs()
{
    // Forward UIPreferences signals so QML bindings on ThemeManager stay live.
    connect(m_prefs, &UIPreferences::gtkThemeLightChanged,
            this, &ThemeManager::gtkThemeLightChanged);
    connect(m_prefs, &UIPreferences::gtkThemeDarkChanged,
            this, &ThemeManager::gtkThemeDarkChanged);
    connect(m_prefs, &UIPreferences::kdeColorSchemeLightChanged,
            this, &ThemeManager::kdeColorSchemeLightChanged);
    connect(m_prefs, &UIPreferences::kdeColorSchemeDarkChanged,
            this, &ThemeManager::kdeColorSchemeDarkChanged);
    connect(m_prefs, &UIPreferences::gtkIconThemeLightChanged,
            this, &ThemeManager::gtkIconThemeLightChanged);
    connect(m_prefs, &UIPreferences::gtkIconThemeDarkChanged,
            this, &ThemeManager::gtkIconThemeDarkChanged);
    connect(m_prefs, &UIPreferences::kdeIconThemeLightChanged,
            this, &ThemeManager::kdeIconThemeLightChanged);
    connect(m_prefs, &UIPreferences::kdeIconThemeDarkChanged,
            this, &ThemeManager::kdeIconThemeDarkChanged);

    // Apply the active theme/icon-theme whenever the selection or the mode changes.
    auto applyActive = [this]() {
        const bool dark = (m_prefs->themeMode() == QLatin1String("dark"));
        applyGtkTheme(dark ? m_prefs->gtkThemeDark() : m_prefs->gtkThemeLight());
        applyKdeColorScheme(dark ? m_prefs->kdeColorSchemeDark() : m_prefs->kdeColorSchemeLight());
        applyGtkIconTheme(dark ? m_prefs->gtkIconThemeDark() : m_prefs->gtkIconThemeLight());
        applyKdeIconTheme(dark ? m_prefs->kdeIconThemeDark() : m_prefs->kdeIconThemeLight());
    };

    connect(m_prefs, &UIPreferences::themeModeChanged, this, applyActive);
    connect(m_prefs, &UIPreferences::gtkThemeLightChanged, this, applyActive);
    connect(m_prefs, &UIPreferences::gtkThemeDarkChanged, this, applyActive);
    connect(m_prefs, &UIPreferences::kdeColorSchemeLightChanged, this, applyActive);
    connect(m_prefs, &UIPreferences::kdeColorSchemeDarkChanged, this, applyActive);
    connect(m_prefs, &UIPreferences::gtkIconThemeLightChanged, this, applyActive);
    connect(m_prefs, &UIPreferences::gtkIconThemeDarkChanged, this, applyActive);
    connect(m_prefs, &UIPreferences::kdeIconThemeLightChanged, this, applyActive);
    connect(m_prefs, &UIPreferences::kdeIconThemeDarkChanged, this, applyActive);
}

// ── Public API ────────────────────────────────────────────────────────────────

QStringList ThemeManager::gtkThemes() const        { return m_gtkThemes; }
QStringList ThemeManager::kdeColorSchemes() const  { return m_kdeColorSchemes; }
QStringList ThemeManager::iconThemes() const       { return m_iconThemes; }

QString ThemeManager::gtkThemeLight() const        { return m_prefs->gtkThemeLight(); }
QString ThemeManager::gtkThemeDark() const         { return m_prefs->gtkThemeDark(); }
QString ThemeManager::kdeColorSchemeLight() const  { return m_prefs->kdeColorSchemeLight(); }
QString ThemeManager::kdeColorSchemeDark() const   { return m_prefs->kdeColorSchemeDark(); }
QString ThemeManager::gtkIconThemeLight() const    { return m_prefs->gtkIconThemeLight(); }
QString ThemeManager::gtkIconThemeDark() const     { return m_prefs->gtkIconThemeDark(); }
QString ThemeManager::kdeIconThemeLight() const    { return m_prefs->kdeIconThemeLight(); }
QString ThemeManager::kdeIconThemeDark() const     { return m_prefs->kdeIconThemeDark(); }

void ThemeManager::setGtkThemeLight(const QString &theme)
{
    m_prefs->setGtkThemeLight(theme);
    if (m_prefs->themeMode() != QLatin1String("dark"))
        applyGtkTheme(theme);
}

void ThemeManager::setGtkThemeDark(const QString &theme)
{
    m_prefs->setGtkThemeDark(theme);
    if (m_prefs->themeMode() == QLatin1String("dark"))
        applyGtkTheme(theme);
}

void ThemeManager::setKdeColorSchemeLight(const QString &scheme)
{
    m_prefs->setKdeColorSchemeLight(scheme);
    if (m_prefs->themeMode() != QLatin1String("dark"))
        applyKdeColorScheme(scheme);
}

void ThemeManager::setKdeColorSchemeDark(const QString &scheme)
{
    m_prefs->setKdeColorSchemeDark(scheme);
    if (m_prefs->themeMode() == QLatin1String("dark"))
        applyKdeColorScheme(scheme);
}

void ThemeManager::setGtkIconThemeLight(const QString &theme)
{
    m_prefs->setGtkIconThemeLight(theme);
    // System icon theme follows the GTK icon theme selection.
    m_prefs->setIconThemeLight(theme);
    if (m_prefs->themeMode() != QLatin1String("dark"))
        applyGtkIconTheme(theme);
}

void ThemeManager::setGtkIconThemeDark(const QString &theme)
{
    m_prefs->setGtkIconThemeDark(theme);
    // System icon theme follows the GTK icon theme selection.
    m_prefs->setIconThemeDark(theme);
    if (m_prefs->themeMode() == QLatin1String("dark"))
        applyGtkIconTheme(theme);
}

void ThemeManager::setKdeIconThemeLight(const QString &theme)
{
    m_prefs->setKdeIconThemeLight(theme);
    if (m_prefs->themeMode() != QLatin1String("dark"))
        applyKdeIconTheme(theme);
}

void ThemeManager::setKdeIconThemeDark(const QString &theme)
{
    m_prefs->setKdeIconThemeDark(theme);
    if (m_prefs->themeMode() == QLatin1String("dark"))
        applyKdeIconTheme(theme);
}

void ThemeManager::refresh()
{
    const QStringList newGtk = scanGtkThemes();
    if (newGtk != m_gtkThemes) {
        m_gtkThemes = newGtk;
        emit gtkThemesChanged();
    }

    const QStringList newKde = scanKdeColorSchemes();
    if (newKde != m_kdeColorSchemes) {
        m_kdeColorSchemes = newKde;
        emit kdeColorSchemesChanged();
    }

    const QStringList newIcons = scanIconThemes();
    if (newIcons != m_iconThemes) {
        m_iconThemes = newIcons;
        emit iconThemesChanged();
    }
}
