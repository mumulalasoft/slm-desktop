#include "themeiconcontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSet>

namespace {
QStringList effectiveThemeSearchPaths()
{
    QStringList paths = QIcon::themeSearchPaths();
    if (!paths.contains(QStringLiteral("/usr/share/icons"))) {
        paths << QStringLiteral("/usr/share/icons");
    }
    if (!paths.contains(QStringLiteral("/usr/local/share/icons"))) {
        paths << QStringLiteral("/usr/local/share/icons");
    }
    return paths;
}

QString stripDarkSuffix(const QString &theme)
{
    const QString lower = theme.toLower();
    if (lower.endsWith(QStringLiteral("-dark"))) {
        return theme.left(theme.size() - 5);
    }
    if (lower.endsWith(QStringLiteral(" dark"))) {
        return theme.left(theme.size() - 5);
    }
    if (lower.endsWith(QStringLiteral("_dark"))) {
        return theme.left(theme.size() - 5);
    }
    if (lower.endsWith(QStringLiteral("dark"))) {
        return theme.left(theme.size() - 4);
    }
    return theme;
}
} // namespace

ThemeIconController::ThemeIconController(QObject *parent)
    : QObject(parent)
{
    refreshAvailableThemes();
    detectDefaultThemeMapping();
}

QString ThemeIconController::lightThemeName() const
{
    return m_lightThemeName;
}

QString ThemeIconController::darkThemeName() const
{
    return m_darkThemeName;
}

QStringList ThemeIconController::availableThemes() const
{
    return m_availableThemes;
}

int ThemeIconController::revision() const
{
    return m_revision;
}

void ThemeIconController::applyForDarkMode(bool darkMode)
{
    const QString target = darkMode ? m_darkThemeName : m_lightThemeName;
    if (target.isEmpty()) {
        return;
    }
    if (QIcon::themeName() == target) {
        return;
    }

    QIcon::setThemeName(target);
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
    ++m_revision;
    emit revisionChanged();
}

void ThemeIconController::setThemeMapping(const QString &lightThemeName, const QString &darkThemeName)
{
    const QString light = normalizedThemeName(lightThemeName);
    const QString dark = normalizedThemeName(darkThemeName);
    if (light.isEmpty() || dark.isEmpty()) {
        return;
    }
    if (light == m_lightThemeName && dark == m_darkThemeName) {
        return;
    }

    m_lightThemeName = light;
    m_darkThemeName = dark;
    emit themeMappingChanged();
}

void ThemeIconController::useAutoDetectedMapping()
{
    detectDefaultThemeMapping();
}

void ThemeIconController::refreshAvailableThemes()
{
    QSet<QString> names;
    const QStringList paths = effectiveThemeSearchPaths();
    for (const QString &basePath : paths) {
        if (basePath.isEmpty()) {
            continue;
        }
        QDir base(basePath);
        const QStringList dirs = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &name : dirs) {
            const QString indexPath = QDir(base.filePath(name)).filePath(QStringLiteral("index.theme"));
            if (QFileInfo::exists(indexPath)) {
                names.insert(name);
            }
        }
    }
    QStringList list = names.values();
    list.sort(Qt::CaseInsensitive);
    if (list != m_availableThemes) {
        m_availableThemes = list;
        emit availableThemesChanged();
    }
}

bool ThemeIconController::themeExists(const QString &themeName) const
{
    if (themeName.isEmpty()) {
        return false;
    }
    const QStringList paths = effectiveThemeSearchPaths();
    for (const QString &base : paths) {
        if (base.isEmpty()) {
            continue;
        }
        const QString themePath = QDir(base).filePath(themeName);
        if (QFileInfo::exists(QDir(themePath).filePath(QStringLiteral("index.theme")))) {
            return true;
        }
    }
    return false;
}

void ThemeIconController::detectDefaultThemeMapping()
{
    QString current = normalizedThemeName(QIcon::themeName());
    if (current.isEmpty()) {
        current = QStringLiteral("Adwaita");
    }

    QString light;
    QString dark;
    const QString lower = current.toLower();
    if (lower.contains(QStringLiteral("dark"))) {
        dark = current;
        light = stripDarkSuffix(current);
        if (light.isEmpty()) {
            light = QStringLiteral("Adwaita");
        }
    } else {
        light = current;
        const QString dashDark = current + QStringLiteral("-dark");
        const QString spaceDark = current + QStringLiteral(" Dark");
        if (themeExists(dashDark)) {
            dark = dashDark;
        } else if (themeExists(spaceDark)) {
            dark = spaceDark;
        } else {
            dark = QStringLiteral("Adwaita-dark");
        }
    }

    if (!themeExists(light)) {
        light = themeExists(QStringLiteral("Adwaita")) ? QStringLiteral("Adwaita") : QStringLiteral("hicolor");
    }
    if (!themeExists(dark)) {
        if (themeExists(QStringLiteral("Adwaita-dark"))) {
            dark = QStringLiteral("Adwaita-dark");
        } else if (themeExists(light + QStringLiteral("-dark"))) {
            dark = light + QStringLiteral("-dark");
        } else {
            dark = light;
        }
    }

    m_lightThemeName = light;
    m_darkThemeName = dark;
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
    emit themeMappingChanged();
}

QString ThemeIconController::normalizedThemeName(QString name) const
{
    name = name.trimmed();
    return name;
}
