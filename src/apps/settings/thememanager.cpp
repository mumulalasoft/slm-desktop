#include "thememanager.h"
#include "desktopsettingsclient.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStringView>

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

void ThemeManager::applyWindowControlsLayout(const QString &side)
{
    const QString normalized = side.trimmed().toLower() == QLatin1String("left")
            ? QStringLiteral("left") : QStringLiteral("right");

    // GTK client-side decorations (GTK3/GTK4).
    const QString gtkLayout = (normalized == QLatin1String("left"))
            ? QStringLiteral("close,minimize,maximize:")
            : QStringLiteral(":minimize,maximize,close");
    const QString configBase = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QStringList gtkDirs = {
        configBase + QStringLiteral("/gtk-3.0"),
        configBase + QStringLiteral("/gtk-4.0"),
    };
    for (const QString &gtkDir : gtkDirs) {
        QDir().mkpath(gtkDir);
        QSettings ini(gtkDir + QStringLiteral("/settings.ini"), QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("Settings"));
        ini.setValue(QStringLiteral("gtk-decoration-layout"), gtkLayout);
        ini.endGroup();
        ini.sync();
    }

    // KDE / KWin decoration layout.
    QSettings kwinrc(configBase + QStringLiteral("/kwinrc"), QSettings::IniFormat);
    kwinrc.beginGroup(QStringLiteral("org.kde.kdecoration2"));
    if (normalized == QLatin1String("left")) {
        kwinrc.setValue(QStringLiteral("ButtonsOnLeft"), QStringLiteral("XIA"));
        kwinrc.setValue(QStringLiteral("ButtonsOnRight"), QStringLiteral(""));
    } else {
        kwinrc.setValue(QStringLiteral("ButtonsOnLeft"), QStringLiteral(""));
        kwinrc.setValue(QStringLiteral("ButtonsOnRight"), QStringLiteral("XIA"));
    }
    kwinrc.endGroup();
    kwinrc.sync();
}

// ── Constructor ───────────────────────────────────────────────────────────────

ThemeManager::ThemeManager(DesktopSettingsClient *desktopSettings,
                           QObject *parent)
    : QObject(parent)
    , m_desktopSettings(desktopSettings)
    , m_gtkThemes(scanGtkThemes())
    , m_kdeColorSchemes(scanKdeColorSchemes())
    , m_iconThemes(scanIconThemes())
{
    connectSources();
}

void ThemeManager::connectSources()
{
    if (!m_desktopSettings) {
        return;
    }
    connect(m_desktopSettings, &DesktopSettingsClient::gtkThemeLightChanged,
            this, &ThemeManager::gtkThemeLightChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::gtkThemeDarkChanged,
            this, &ThemeManager::gtkThemeDarkChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::kdeColorSchemeLightChanged,
            this, &ThemeManager::kdeColorSchemeLightChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::kdeColorSchemeDarkChanged,
            this, &ThemeManager::kdeColorSchemeDarkChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::gtkIconThemeLightChanged,
            this, &ThemeManager::gtkIconThemeLightChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::gtkIconThemeDarkChanged,
            this, &ThemeManager::gtkIconThemeDarkChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::kdeIconThemeLightChanged,
            this, &ThemeManager::kdeIconThemeLightChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::kdeIconThemeDarkChanged,
            this, &ThemeManager::kdeIconThemeDarkChanged);
    connect(m_desktopSettings, &DesktopSettingsClient::windowControlsSideChanged, this, [this]() {
        applyWindowControlsLayout(resolveWindowControlsSide());
        emit windowControlsSideChanged();
        emit appearanceChanged();
    });

    // Apply the active theme/icon-theme whenever the selection or the mode changes.
    auto applyActive = [this]() {
        const bool dark = (activeThemeMode() == QLatin1String("dark"));
        applyGtkTheme(dark ? currentGtkThemeDark() : currentGtkThemeLight());
        applyKdeColorScheme(dark ? currentKdeColorSchemeDark() : currentKdeColorSchemeLight());
        applyGtkIconTheme(dark ? currentGtkIconThemeDark() : currentGtkIconThemeLight());
        applyKdeIconTheme(dark ? currentKdeIconThemeDark() : currentKdeIconThemeLight());
    };

    connect(m_desktopSettings, &DesktopSettingsClient::themeModeChanged, this, applyActive);
    connect(m_desktopSettings, &DesktopSettingsClient::gtkThemeLightChanged, this, applyActive);
    connect(m_desktopSettings, &DesktopSettingsClient::gtkThemeDarkChanged, this, applyActive);
    connect(m_desktopSettings, &DesktopSettingsClient::kdeColorSchemeLightChanged, this, applyActive);
    connect(m_desktopSettings, &DesktopSettingsClient::kdeColorSchemeDarkChanged, this, applyActive);
    connect(m_desktopSettings, &DesktopSettingsClient::gtkIconThemeLightChanged, this, applyActive);
    connect(m_desktopSettings, &DesktopSettingsClient::gtkIconThemeDarkChanged, this, applyActive);
    connect(m_desktopSettings, &DesktopSettingsClient::kdeIconThemeLightChanged, this, applyActive);
    connect(m_desktopSettings, &DesktopSettingsClient::kdeIconThemeDarkChanged, this, applyActive);

    applyWindowControlsLayout(resolveWindowControlsSide());
}

// ── Public API ────────────────────────────────────────────────────────────────

QStringList ThemeManager::gtkThemes() const        { return m_gtkThemes; }
QStringList ThemeManager::kdeColorSchemes() const  { return m_kdeColorSchemes; }
QStringList ThemeManager::iconThemes() const       { return m_iconThemes; }

QString ThemeManager::gtkThemeLight() const        { return currentGtkThemeLight(); }
QString ThemeManager::gtkThemeDark() const         { return currentGtkThemeDark(); }
QString ThemeManager::kdeColorSchemeLight() const  { return currentKdeColorSchemeLight(); }
QString ThemeManager::kdeColorSchemeDark() const   { return currentKdeColorSchemeDark(); }
QString ThemeManager::gtkIconThemeLight() const    { return currentGtkIconThemeLight(); }
QString ThemeManager::gtkIconThemeDark() const     { return currentGtkIconThemeDark(); }
QString ThemeManager::kdeIconThemeLight() const    { return currentKdeIconThemeLight(); }
QString ThemeManager::kdeIconThemeDark() const     { return currentKdeIconThemeDark(); }
QString ThemeManager::windowControlsSide() const
{
    return resolveWindowControlsSide();
}

void ThemeManager::setGtkThemeLight(const QString &theme)
{
    if (m_desktopSettings) {
        m_desktopSettings->setGtkThemeLight(theme);
    }
    if (activeThemeMode() != QLatin1String("dark"))
        applyGtkTheme(theme);
}

void ThemeManager::setGtkThemeDark(const QString &theme)
{
    if (m_desktopSettings) {
        m_desktopSettings->setGtkThemeDark(theme);
    }
    if (activeThemeMode() == QLatin1String("dark"))
        applyGtkTheme(theme);
}

void ThemeManager::setKdeColorSchemeLight(const QString &scheme)
{
    if (m_desktopSettings) {
        m_desktopSettings->setKdeColorSchemeLight(scheme);
    }
    if (activeThemeMode() != QLatin1String("dark"))
        applyKdeColorScheme(scheme);
}

void ThemeManager::setKdeColorSchemeDark(const QString &scheme)
{
    if (m_desktopSettings) {
        m_desktopSettings->setKdeColorSchemeDark(scheme);
    }
    if (activeThemeMode() == QLatin1String("dark"))
        applyKdeColorScheme(scheme);
}

void ThemeManager::setGtkIconThemeLight(const QString &theme)
{
    if (m_desktopSettings) {
        m_desktopSettings->setGtkIconThemeLight(theme);
    }
    if (activeThemeMode() != QLatin1String("dark"))
        applyGtkIconTheme(theme);
}

void ThemeManager::setGtkIconThemeDark(const QString &theme)
{
    if (m_desktopSettings) {
        m_desktopSettings->setGtkIconThemeDark(theme);
    }
    if (activeThemeMode() == QLatin1String("dark"))
        applyGtkIconTheme(theme);
}

void ThemeManager::setKdeIconThemeLight(const QString &theme)
{
    if (m_desktopSettings) {
        m_desktopSettings->setKdeIconThemeLight(theme);
    }
    if (activeThemeMode() != QLatin1String("dark"))
        applyKdeIconTheme(theme);
}

void ThemeManager::setKdeIconThemeDark(const QString &theme)
{
    if (m_desktopSettings) {
        m_desktopSettings->setKdeIconThemeDark(theme);
    }
    if (activeThemeMode() == QLatin1String("dark"))
        applyKdeIconTheme(theme);
}

void ThemeManager::setWindowControlsSide(const QString &side)
{
    const QString normalized = side.trimmed().toLower() == QLatin1String("left")
            ? QStringLiteral("left") : QStringLiteral("right");
    if (normalized == resolveWindowControlsSide()) {
        return;
    }
    if (m_desktopSettings) {
        m_desktopSettings->setWindowControlsSide(normalized);
    }
    applyWindowControlsLayout(normalized);
    emit windowControlsSideChanged();
    emit appearanceChanged();
}

QString ThemeManager::activeThemeMode() const
{
    return m_desktopSettings ? m_desktopSettings->themeMode() : QStringLiteral("dark");
}

QString ThemeManager::resolveWindowControlsSide() const
{
    if (m_desktopSettings) {
        return m_desktopSettings->windowControlsSide().trimmed().toLower() == QLatin1String("left")
                ? QStringLiteral("left")
                : QStringLiteral("right");
    }
    return QStringLiteral("right");
}

QString ThemeManager::currentGtkThemeLight() const
{
    return m_desktopSettings ? m_desktopSettings->gtkThemeLight() : QString();
}

QString ThemeManager::currentGtkThemeDark() const
{
    return m_desktopSettings ? m_desktopSettings->gtkThemeDark() : QString();
}

QString ThemeManager::currentKdeColorSchemeLight() const
{
    return m_desktopSettings ? m_desktopSettings->kdeColorSchemeLight() : QString();
}

QString ThemeManager::currentKdeColorSchemeDark() const
{
    return m_desktopSettings ? m_desktopSettings->kdeColorSchemeDark() : QString();
}

QString ThemeManager::currentGtkIconThemeLight() const
{
    return m_desktopSettings ? m_desktopSettings->gtkIconThemeLight() : QString();
}

QString ThemeManager::currentGtkIconThemeDark() const
{
    return m_desktopSettings ? m_desktopSettings->gtkIconThemeDark() : QString();
}

QString ThemeManager::currentKdeIconThemeLight() const
{
    return m_desktopSettings ? m_desktopSettings->kdeIconThemeLight() : QString();
}

QString ThemeManager::currentKdeIconThemeDark() const
{
    return m_desktopSettings ? m_desktopSettings->kdeIconThemeDark() : QString();
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
