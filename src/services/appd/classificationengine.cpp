#include "classificationengine.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

namespace Slm::Appd {

namespace {
// Known terminal emulators — children of these are cli-app.
const QStringList kTerminals = {
    QStringLiteral("konsole"),
    QStringLiteral("gnome-terminal"),
    QStringLiteral("xterm"),
    QStringLiteral("alacritty"),
    QStringLiteral("kitty"),
    QStringLiteral("wezterm"),
    QStringLiteral("foot"),
    QStringLiteral("tilix"),
    QStringLiteral("yakuake"),
    QStringLiteral("xfce4-terminal"),
};

// Known shell processes — children of these are cli-app.
const QStringList kShells = {
    QStringLiteral("bash"),
    QStringLiteral("sh"),
    QStringLiteral("zsh"),
    QStringLiteral("fish"),
    QStringLiteral("dash"),
    QStringLiteral("ksh"),
};

// Shell/compositor apps that should never surface in dock.
const QStringList kShellAppIds = {
    QStringLiteral("appdesktop_shell"),
    QStringLiteral("org.slm.shell"),
    QStringLiteral("slm-desktop"),
    QStringLiteral("plasmashell"),
    QStringLiteral("kwin_wayland"),
    QStringLiteral("kwin_x11"),
};

QString exeNameFromPath(const QString &executable)
{
    return QFileInfo(executable).fileName().toLower();
}

QString readProcessExe(qint64 pid)
{
    const QString commPath = QStringLiteral("/proc/%1/comm").arg(pid);
    QFile f(commPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll()).trimmed().toLower();
}
} // namespace

ClassificationEngine::ClassificationEngine()
{
    loadOverrides();
}

void ClassificationEngine::loadOverrides()
{
    m_overrides.clear();

    const QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                         + QStringLiteral("/desktop-appd/overrides.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[appd-classify] failed to parse overrides:" << err.errorString();
        return;
    }

    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString categoryStr = it.value().toString();
        AppCategory cat = AppCategory::Unknown;
        if (categoryStr == QStringLiteral("shell"))                cat = AppCategory::Shell;
        else if (categoryStr == QStringLiteral("gui-app"))         cat = AppCategory::GuiApp;
        else if (categoryStr == QStringLiteral("gtk"))             cat = AppCategory::Gtk;
        else if (categoryStr == QStringLiteral("kde"))             cat = AppCategory::Kde;
        else if (categoryStr == QStringLiteral("qt-generic"))      cat = AppCategory::QtGeneric;
        else if (categoryStr == QStringLiteral("cli-app"))         cat = AppCategory::CliApp;
        else if (categoryStr == QStringLiteral("background-agent")) cat = AppCategory::BackgroundAgent;
        else if (categoryStr == QStringLiteral("system-ignore"))   cat = AppCategory::SystemIgnore;

        m_overrides.insert(it.key(), cat);
    }

    qDebug() << "[appd-classify] loaded" << m_overrides.size() << "overrides from" << path;
}

AppCategory ClassificationEngine::classify(const QString &appId,
                                           const QString &desktopFile,
                                           const QString &executable,
                                           const QStringList &desktopCategories,
                                           bool hasWindow,
                                           qint64 parentPid) const
{
    // 1. Explicit override.
    if (const auto cat = classifyFromOverride(appId); cat != AppCategory::Unknown) {
        return cat;
    }

    // 2. Known shell apps.
    for (const auto &id : kShellAppIds) {
        if (appId.contains(id, Qt::CaseInsensitive)) {
            return AppCategory::Shell;
        }
    }

    // 3. .desktop metadata.
    if (!desktopFile.isEmpty() || !desktopCategories.isEmpty()) {
        const auto cat = classifyFromDesktop(desktopCategories, desktopFile);
        if (cat != AppCategory::Unknown) {
            return cat;
        }
    }

    // 4. Toolkit / executable hint.
    if (!executable.isEmpty()) {
        const auto cat = classifyFromToolkit(executable);
        if (cat != AppCategory::Unknown) {
            return cat;
        }
    }

    // 5. Parent PID (terminal parent → CLI).
    if (parentPid > 0) {
        const auto cat = classifyFromParentPid(parentPid);
        if (cat != AppCategory::Unknown) {
            return cat;
        }
    }

    // 6. Window existence.
    if (hasWindow) {
        return AppCategory::GuiApp;
    }

    return AppCategory::Unknown;
}

AppCategory ClassificationEngine::classifyFromOverride(const QString &appId) const
{
    return m_overrides.value(appId, AppCategory::Unknown);
}

AppCategory ClassificationEngine::classifyFromDesktop(const QStringList &categories,
                                                      const QString &desktopFile) const
{
    // Parse .desktop file if categories list is empty.
    QStringList cats = categories;
    if (cats.isEmpty() && !desktopFile.isEmpty()) {
        cats = readDesktopCategories(desktopFile);
    }

    for (const auto &cat : std::as_const(cats)) {
        const QString c = cat.toLower();
        if (c == QStringLiteral("kde") || c.startsWith(QStringLiteral("x-kde"))) {
            return AppCategory::Kde;
        }
        if (c == QStringLiteral("gnome") || c == QStringLiteral("gtk")) {
            return AppCategory::Gtk;
        }
        if (c == QStringLiteral("qt")) {
            return AppCategory::QtGeneric;
        }
    }

    // Has a .desktop file at all → gui-app fallback.
    if (!desktopFile.isEmpty()) {
        return AppCategory::GuiApp;
    }

    return AppCategory::Unknown;
}

AppCategory ClassificationEngine::classifyFromToolkit(const QString &executable) const
{
    const QString exe = exeNameFromPath(executable);
    if (isTerminal(exe) || kShells.contains(exe)) {
        return AppCategory::CliApp;
    }
    return AppCategory::Unknown;
}

AppCategory ClassificationEngine::classifyFromParentPid(qint64 parentPid) const
{
    const QString parentExe = readProcessExe(parentPid);
    if (parentExe.isEmpty()) {
        return AppCategory::Unknown;
    }
    if (isTerminal(parentExe) || kShells.contains(parentExe)) {
        return AppCategory::CliApp;
    }
    return AppCategory::Unknown;
}

// static
bool ClassificationEngine::isTerminal(const QString &executable)
{
    return kTerminals.contains(executable.toLower());
}

// static
QStringList ClassificationEngine::readDesktopCategories(const QString &desktopFile)
{
    QSettings s(desktopFile, QSettings::IniFormat);
    s.beginGroup(QStringLiteral("Desktop Entry"));
    const QString raw = s.value(QStringLiteral("Categories")).toString();
    s.endGroup();
    return raw.split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

} // namespace Slm::Appd
