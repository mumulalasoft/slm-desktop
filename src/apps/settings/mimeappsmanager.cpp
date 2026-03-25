#include "mimeappsmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QDebug>

// XDG data dirs for .desktop files, in precedence order (user before system).
static QStringList xdgApplicationDirs()
{
    QStringList dirs;
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    dirs << home + QLatin1String("/.local/share/applications");
    for (const QString &base : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        const QString path = base + QLatin1String("/applications");
        if (!dirs.contains(path))
            dirs << path;
    }
    return dirs;
}

MimeAppsManager::MimeAppsManager(QObject *parent) : QObject(parent) {}

QList<MimeAppsManager::DesktopEntry> MimeAppsManager::scanDesktopFiles() const
{
    QList<DesktopEntry> entries;
    QSet<QString> seen;

    for (const QString &dir : xdgApplicationDirs()) {
        const QFileInfoList files = QDir(dir).entryInfoList(
            {QStringLiteral("*.desktop")}, QDir::Files);

        for (const QFileInfo &fi : files) {
            const QString id = fi.fileName();
            if (seen.contains(id))
                continue; // user dir already handled this desktop-file id

            QFile f(fi.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            DesktopEntry e;
            e.id = id;
            bool inMain = false;
            bool skip = false;

            QTextStream ts(&f);
            while (!ts.atEnd()) {
                const QString line = ts.readLine().trimmed();
                if (line == QLatin1String("[Desktop Entry]")) {
                    inMain = true;
                    continue;
                }
                if (line.startsWith('[')) {
                    inMain = false;
                    continue;
                }
                if (!inMain || line.isEmpty() || line.startsWith('#'))
                    continue;

                const int eq = line.indexOf('=');
                if (eq < 0)
                    continue;

                const QString key = line.left(eq).trimmed();
                const QString val = line.mid(eq + 1).trimmed();

                if (key == QLatin1String("Name") && e.name.isEmpty())
                    e.name = val;
                else if (key == QLatin1String("Icon"))
                    e.icon = val;
                else if (key == QLatin1String("MimeType"))
                    e.mimeTypes = val.split(QLatin1Char(';'), Qt::SkipEmptyParts);
                else if (key == QLatin1String("Type") && val != QLatin1String("Application"))
                    skip = true;
                else if ((key == QLatin1String("Hidden") || key == QLatin1String("NoDisplay"))
                         && val.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
                    skip = true;
            }

            if (!skip && !e.name.isEmpty()) {
                seen.insert(id);
                entries << e;
            }
        }
    }
    return entries;
}

QVariantList MimeAppsManager::appsForMimeType(const QString &mimeType) const
{
    const QString mime = mimeType.trimmed().toLower();
    QVariantList result;

    for (const DesktopEntry &e : scanDesktopFiles()) {
        for (const QString &mt : e.mimeTypes) {
            if (mt.trimmed().toLower() == mime) {
                QVariantMap map;
                map[QStringLiteral("id")]   = e.id;
                map[QStringLiteral("name")] = e.name;
                map[QStringLiteral("icon")] = e.icon.isEmpty()
                    ? QStringLiteral("application-x-executable") : e.icon;
                result << map;
                break;
            }
        }
    }
    return result;
}

QString MimeAppsManager::defaultAppForMimeType(const QString &mimeType) const
{
    // Read from ~/.config/mimeapps.list (user-level, takes precedence).
    // Format: [Default Applications]\nmime/type=app.desktop
    QFile file(mimeAppsListPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    bool inSection = false;
    QTextStream ts(&file);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line == QLatin1String("[Default Applications]")) {
            inSection = true;
            continue;
        }
        if (line.startsWith('[')) {
            inSection = false;
            continue;
        }
        if (!inSection || line.isEmpty() || line.startsWith('#'))
            continue;

        const int eq = line.indexOf('=');
        if (eq < 0)
            continue;
        if (line.left(eq).trimmed() == mimeType) {
            // Value may be a semicolon-separated list; first entry wins.
            return line.mid(eq + 1).trimmed()
                .split(QLatin1Char(';'), Qt::SkipEmptyParts)
                .value(0)
                .trimmed();
        }
    }
    return {};
}

void MimeAppsManager::setDefaultApp(const QString &mimeType, const QString &desktopFileId)
{
    // Read the current file so we preserve all other content.
    QStringList lines;
    QFile file(mimeAppsListPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&file);
        while (!ts.atEnd())
            lines << ts.readLine();
        file.close();
    }

    bool inSection = false;
    bool found = false;
    int sectionIdx = -1;

    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line == QLatin1String("[Default Applications]")) {
            inSection = true;
            sectionIdx = i;
            continue;
        }
        if (line.startsWith('[')) {
            inSection = false;
            continue;
        }
        if (!inSection || line.isEmpty() || line.startsWith('#'))
            continue;

        const int eq = line.indexOf('=');
        if (eq < 0)
            continue;
        if (line.left(eq).trimmed() == mimeType) {
            lines[i] = mimeType + QLatin1Char('=') + desktopFileId;
            found = true;
        }
    }

    if (!found) {
        if (sectionIdx < 0) {
            // No [Default Applications] section yet — append one.
            if (!lines.isEmpty() && !lines.last().trimmed().isEmpty())
                lines << QString();
            lines << QStringLiteral("[Default Applications]");
            sectionIdx = lines.size() - 1;
        }
        // Insert the new key right after the section header (before next section).
        int insertAt = sectionIdx + 1;
        while (insertAt < lines.size() && !lines[insertAt].trimmed().startsWith('['))
            ++insertAt;
        lines.insert(insertAt, mimeType + QLatin1Char('=') + desktopFileId);
    }

    // Ensure the config directory exists.
    QDir().mkpath(QFileInfo(mimeAppsListPath()).absolutePath());

    QFile out(mimeAppsListPath());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "[MimeAppsManager] Cannot write" << mimeAppsListPath();
        return;
    }
    QTextStream ts(&out);
    for (const QString &l : lines)
        ts << l << '\n';

    emit defaultAppChanged(mimeType, desktopFileId);
}

QString MimeAppsManager::mimeAppsListPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QLatin1String("/mimeapps.list");
}
