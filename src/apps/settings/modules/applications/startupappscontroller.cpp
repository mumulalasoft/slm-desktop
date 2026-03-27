#include "startupappscontroller.h"

#include <QDir>
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUuid>
#include <algorithm>

// ── StartupAppsController ─────────────────────────────────────────────────

StartupAppsController::StartupAppsController(QObject *parent)
    : QObject(parent)
{
    reload();
}

QVariantList StartupAppsController::apps() const
{
    return m_apps;
}

void StartupAppsController::reload()
{
    QDir dir(userDir());
    if (!dir.exists()) {
        m_apps = {};
        emit appsChanged();
        return;
    }

    QVariantList result;
    const QStringList files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files);
    for (const QString &fname : files) {
        const QString id    = QFileInfo(fname).completeBaseName();
        const QString fpath = dir.absoluteFilePath(fname);
        const auto    keys  = parseDesktopFile(fpath);

        if (keys.value(QStringLiteral("Type")) != QStringLiteral("Application"))
            continue;

        // Skip entries explicitly hidden
        if (keys.value(QStringLiteral("Hidden")).trimmed().compare(
                QStringLiteral("true"), Qt::CaseInsensitive) == 0)
            continue;

        result << buildEntry(id, keys);
    }

    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("name")).toString().toLower()
             < b.toMap().value(QStringLiteral("name")).toString().toLower();
    });

    m_apps = result;
    emit appsChanged();
}

void StartupAppsController::setEnabled(const QString &id, bool enabled)
{
    const QString uPath = userPath(id);
    if (!QFile::exists(uPath)) return;

    updateKey(uPath,
              QStringLiteral("X-GNOME-Autostart-enabled"),
              enabled ? QStringLiteral("true") : QStringLiteral("false"));
    reload();
}

void StartupAppsController::removeApp(const QString &id)
{
    const QString uPath = userPath(id);
    if (QFile::exists(uPath))
        QFile::remove(uPath);
    reload();
}

void StartupAppsController::addDesktopFile(const QString &sourcePath)
{
    if (!QFile::exists(sourcePath)) return;
    ensureUserDir();

    const QString fname = QFileInfo(sourcePath).fileName();
    QString dest        = userDir() + QLatin1Char('/') + fname;

    if (QFile::exists(dest)) {
        const QString stem = QFileInfo(fname).completeBaseName();
        dest = userDir() + QLatin1Char('/') + stem
             + QLatin1Char('-')
             + QUuid::createUuid().toString(QUuid::Id128).left(6)
             + QStringLiteral(".desktop");
    }

    QFile::copy(sourcePath, dest);
    updateKey(dest, QStringLiteral("Hidden"), QStringLiteral("false"));
    updateKey(dest, QStringLiteral("X-GNOME-Autostart-enabled"), QStringLiteral("true"));
    reload();
}

void StartupAppsController::addEntry(const QString &name,
                                     const QString &exec,
                                     const QString &icon,
                                     const QString &comment)
{
    if (name.trimmed().isEmpty() || exec.trimmed().isEmpty()) return;
    ensureUserDir();

    QString stem = name.toLower();
    stem.replace(QRegularExpression(QStringLiteral("[^a-z0-9_-]")), QStringLiteral("-"));
    stem.replace(QRegularExpression(QStringLiteral("-{2,}")), QStringLiteral("-"));
    stem = stem.mid(0, 40);

    QString dest = userDir() + QLatin1Char('/') + stem + QStringLiteral(".desktop");
    if (QFile::exists(dest)) {
        dest = userDir() + QLatin1Char('/')
             + stem + QLatin1Char('-')
             + QUuid::createUuid().toString(QUuid::Id128).left(6)
             + QStringLiteral(".desktop");
    }

    QMap<QString, QString> keys;
    keys[QStringLiteral("Type")]    = QStringLiteral("Application");
    keys[QStringLiteral("Name")]    = name;
    keys[QStringLiteral("Exec")]    = exec;
    keys[QStringLiteral("X-GNOME-Autostart-enabled")] = QStringLiteral("true");
    if (!icon.isEmpty())    keys[QStringLiteral("Icon")]    = icon;
    if (!comment.isEmpty()) keys[QStringLiteral("Comment")] = comment;

    writeDesktopFile(dest, keys);
    reload();
}

QVariantList StartupAppsController::installedApps() const
{
    // Scan XDG_DATA_HOME and XDG_DATA_DIRS for applications/*.desktop
    QStringList dataDirs;
    const QByteArray dataHome = qgetenv("XDG_DATA_HOME");
    dataDirs << (dataHome.isEmpty()
                     ? QDir::homePath() + QStringLiteral("/.local/share")
                     : QString::fromLocal8Bit(dataHome));

    const QByteArray dataDirsEnv = qgetenv("XDG_DATA_DIRS");
    if (!dataDirsEnv.isEmpty()) {
        for (const QByteArray &d : dataDirsEnv.split(':'))
            dataDirs << QString::fromLocal8Bit(d);
    } else {
        dataDirs << QStringLiteral("/usr/local/share") << QStringLiteral("/usr/share");
    }

    QMap<QString, QVariantMap> seen; // id → entry (first dir wins)
    for (const QString &base : std::as_const(dataDirs)) {
        QDir dir(base + QStringLiteral("/applications"));
        if (!dir.exists()) continue;
        const QStringList files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &fname : files) {
            const QString id   = QFileInfo(fname).completeBaseName();
            if (seen.contains(id)) continue;
            const auto keys = parseDesktopFile(dir.absoluteFilePath(fname));
            if (keys.value(QStringLiteral("Type")) != QStringLiteral("Application")) continue;
            if (keys.value(QStringLiteral("NoDisplay")).trimmed().compare(
                    QStringLiteral("true"), Qt::CaseInsensitive) == 0) continue;
            if (keys.value(QStringLiteral("Hidden")).trimmed().compare(
                    QStringLiteral("true"), Qt::CaseInsensitive) == 0) continue;
            const QString name = keys.value(QStringLiteral("Name"));
            if (name.isEmpty()) continue;
            QVariantMap m;
            m[QStringLiteral("id")]      = id;
            m[QStringLiteral("name")]    = name;
            m[QStringLiteral("icon")]    = keys.value(QStringLiteral("Icon"));
            m[QStringLiteral("comment")] = keys.value(QStringLiteral("Comment"));
            m[QStringLiteral("exec")]    = keys.value(QStringLiteral("Exec"));
            seen.insert(id, m);
        }
    }

    QVariantList result;
    for (const auto &m : std::as_const(seen))
        result << m;
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("name")).toString().toLower()
             < b.toMap().value(QStringLiteral("name")).toString().toLower();
    });
    return result;
}

// ── private ───────────────────────────────────────────────────────────────

QString StartupAppsController::userDir() const
{
    return QDir::homePath() + QStringLiteral("/.config/autostart");
}

QString StartupAppsController::userPath(const QString &id) const
{
    return userDir() + QLatin1Char('/') + id + QStringLiteral(".desktop");
}

QMap<QString, QString> StartupAppsController::parseDesktopFile(
    const QString &filePath) const
{
    QMap<QString, QString> result;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);

    bool inDesktopEntry = false;

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();

        if (line.startsWith(QLatin1Char('['))) {
            inDesktopEntry = (line == QStringLiteral("[Desktop Entry]"));
            continue;
        }

        if (!inDesktopEntry) continue;
        if (line.startsWith(QLatin1Char('#')) || line.isEmpty()) continue;

        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 1) continue;

        const QString key   = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();

        if (key.contains(QLatin1Char('['))) continue; // skip localized keys
        if (!result.contains(key))
            result.insert(key, value);
    }

    return result;
}

void StartupAppsController::writeDesktopFile(
    const QString &filePath,
    const QMap<QString, QString> &keys) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    out << "[Desktop Entry]\n";
    for (auto it = keys.constBegin(); it != keys.constEnd(); ++it)
        out << it.key() << '=' << it.value() << '\n';
}

void StartupAppsController::updateKey(const QString &filePath,
                                      const QString &key,
                                      const QString &value) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    QStringList lines;
    bool inDE  = false;
    bool found = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().startsWith(QLatin1Char('[')))
            inDE = (line.trimmed() == QStringLiteral("[Desktop Entry]"));
        if (inDE && !found) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq > 0 && line.left(eq).trimmed() == key) {
                line  = key + QLatin1Char('=') + value;
                found = true;
            }
        }
        lines << line;
    }
    f.close();

    if (!found) {
        bool inserted = false;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].trimmed().startsWith(QLatin1Char('[')) && i > 0) {
                lines.insert(i, key + QLatin1Char('=') + value);
                inserted = true;
                break;
            }
        }
        if (!inserted)
            lines << key + QLatin1Char('=') + value;
    }

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (const QString &l : std::as_const(lines))
        out << l << '\n';
}

bool StartupAppsController::ensureUserDir() const
{
    return QDir().mkpath(userDir());
}

QVariantMap StartupAppsController::buildEntry(const QString &id,
                                              const QMap<QString, QString> &keys) const
{
    const QString enabledStr = keys.value(
        QStringLiteral("X-GNOME-Autostart-enabled"),
        QStringLiteral("true")).toLower();
    const bool enabled = (enabledStr != QStringLiteral("false"));

    QVariantMap m;
    m[QStringLiteral("id")]      = id;
    m[QStringLiteral("name")]    = keys.value(QStringLiteral("Name"));
    m[QStringLiteral("icon")]    = keys.value(QStringLiteral("Icon"));
    m[QStringLiteral("comment")] = keys.value(QStringLiteral("Comment"));
    m[QStringLiteral("exec")]    = keys.value(QStringLiteral("Exec"));
    m[QStringLiteral("enabled")] = enabled;
    return m;
}
