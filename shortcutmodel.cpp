#include "shortcutmodel.h"
#include "urlutils.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <algorithm>
#include <climits>

#ifdef signals
#undef signals
#define DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif
extern "C" {
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
}
#ifdef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#define signals Q_SIGNALS
#undef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif

namespace {
QString fromUtf8(const char *value)
{
    return value ? QString::fromUtf8(value) : QString();
}

QString iconNameFromGIcon(GIcon *icon)
{
    if (!icon) {
        return QString();
    }
    if (G_IS_THEMED_ICON(icon)) {
        const gchar *const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names && names[0]) {
            return fromUtf8(names[0]);
        }
    }
    return QString();
}

QString iconPathFromGIcon(GIcon *icon)
{
    if (!icon) {
        return QString();
    }
    if (G_IS_FILE_ICON(icon)) {
        GFile *file = g_file_icon_get_file(G_FILE_ICON(icon));
        if (!file) {
            return QString();
        }
        gchar *path = g_file_get_path(file);
        const QString result = fromUtf8(path);
        g_free(path);
        if (!result.isEmpty() && QFileInfo::exists(result)) {
            return QUrl::fromLocalFile(result).toString();
        }
    }
    return QString();
}

QString sanitizeFileName(QString name)
{
    name = name.trimmed();
    if (name.isEmpty()) {
        return QStringLiteral("Shortcut");
    }
    static const QString forbidden = QStringLiteral("\\/:*?\"<>|");
    for (const QChar ch : forbidden) {
        name.replace(ch, QChar('_'));
    }
    return name;
}

QString uniqueDesktopFilePath(const QString &desktopDir, const QString &baseName)
{
    const QString safeBase = sanitizeFileName(baseName);
    QString candidate = QDir(desktopDir).filePath(safeBase + QStringLiteral(".desktop"));
    if (!QFileInfo::exists(candidate)) {
        return candidate;
    }
    for (int i = 2; i <= 9999; ++i) {
        candidate = QDir(desktopDir).filePath(safeBase + QStringLiteral(" (%1).desktop").arg(i));
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QDir(desktopDir).filePath(safeBase + QStringLiteral("-shortcut.desktop"));
}

QString resolveDesktopFilePath(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    auto resolveViaAppId = [](const QString &appId) -> QString {
        GDesktopAppInfo *appInfo = g_desktop_app_info_new(appId.toUtf8().constData());
        if (!appInfo) {
            return QString();
        }
        const QString file = fromUtf8(g_desktop_app_info_get_filename(appInfo));
        g_object_unref(appInfo);
        return file;
    };

    QString candidate = trimmed;
    const QString localPath = UrlUtils::localPathFromFileUrl(candidate);
    if (!localPath.isEmpty()) {
        candidate = localPath;
    }

    QFileInfo pathInfo(candidate);
    if (pathInfo.exists() && pathInfo.isFile()) {
        if (pathInfo.suffix().toLower() == QStringLiteral("desktop")) {
            return pathInfo.absoluteFilePath();
        }
        const QString viaPathId = resolveViaAppId(pathInfo.fileName());
        if (!viaPathId.isEmpty()) {
            return QFileInfo(viaPathId).absoluteFilePath();
        }
    }

    QString viaId = resolveViaAppId(trimmed);
    if (!viaId.isEmpty()) {
        return QFileInfo(viaId).absoluteFilePath();
    }
    if (!trimmed.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        viaId = resolveViaAppId(trimmed + QStringLiteral(".desktop"));
        if (!viaId.isEmpty()) {
            return QFileInfo(viaId).absoluteFilePath();
        }
    }

    const QString needle = trimmed.toLower();
    GList *infos = g_app_info_get_all();
    for (GList *it = infos; it != nullptr; it = it->next) {
        GAppInfo *info = G_APP_INFO(it->data);
        if (!info || !g_app_info_should_show(info)) {
            continue;
        }

        const QString id = fromUtf8(g_app_info_get_id(info)).toLower();
        const QString displayName = fromUtf8(g_app_info_get_display_name(info)).toLower();
        const QString exec = fromUtf8(g_app_info_get_executable(info)).toLower();
        const QString execBase = QFileInfo(exec.split(' ').value(0)).fileName().toLower();

        bool matched = (needle == id || needle == displayName || needle == exec || needle == execBase);
        if (!matched && id.endsWith(QStringLiteral(".desktop"))) {
            const QString idNoSuffix = id.left(id.size() - 8);
            matched = (needle == idNoSuffix);
        }
        if (!matched) {
            continue;
        }
        if (G_IS_DESKTOP_APP_INFO(info)) {
            const QString filename = fromUtf8(g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(info)));
            if (!filename.isEmpty()) {
                g_list_free_full(infos, g_object_unref);
                return QFileInfo(filename).absoluteFilePath();
            }
        }
    }
    g_list_free_full(infos, g_object_unref);
    return QString();
}

bool containsDesktopInList(const gchar *const *items)
{
    if (!items) {
        return false;
    }
    for (int i = 0; items[i] != nullptr; ++i) {
        const QString value = QString::fromUtf8(items[i]).trimmed().toLower();
        if (value == QStringLiteral("gnome") || value == QStringLiteral("unity")) {
            return true;
        }
    }
    return false;
}

bool shouldSkipDesktopEntry(const QString &desktopFilePath)
{
    GKeyFile *keyFile = g_key_file_new();
    if (!keyFile) {
        return false;
    }

    GError *error = nullptr;
    const gboolean loaded = g_key_file_load_from_file(keyFile, desktopFilePath.toUtf8().constData(),
                                                      G_KEY_FILE_NONE, &error);
    if (error) {
        g_error_free(error);
        error = nullptr;
    }
    if (!loaded) {
        g_key_file_unref(keyFile);
        return false;
    }

    gboolean noDisplay = FALSE;
    if (g_key_file_has_key(keyFile, "Desktop Entry", "NoDisplay", nullptr)) {
        noDisplay = g_key_file_get_boolean(keyFile, "Desktop Entry", "NoDisplay", nullptr);
    }

    gsize notShowInLen = 0;
    gchar **notShowIn =
        g_key_file_get_string_list(keyFile, "Desktop Entry", "NotShowIn", &notShowInLen, nullptr);
    bool hideForDesktop = containsDesktopInList(const_cast<const gchar *const *>(notShowIn));
    g_strfreev(notShowIn);

    // Non-standard typo key seen in some custom launchers.
    gsize noShowInLen = 0;
    gchar **noShowIn =
        g_key_file_get_string_list(keyFile, "Desktop Entry", "NoShowIn", &noShowInLen, nullptr);
    hideForDesktop = hideForDesktop || containsDesktopInList(const_cast<const gchar *const *>(noShowIn));
    g_strfreev(noShowIn);

    g_key_file_unref(keyFile);
    return noDisplay || hideForDesktop;
}
} // namespace

ShortcutModel::ShortcutModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_desktopWatchDebounce.setSingleShot(true);
    m_desktopWatchDebounce.setInterval(140);
    connect(&m_desktopWatchDebounce, &QTimer::timeout, this, &ShortcutModel::refresh);
    connect(&m_desktopWatcher, &QFileSystemWatcher::directoryChanged, this,
            &ShortcutModel::scheduleRefreshFromWatcher);
    load();
    setupDesktopWatcher();
}

int ShortcutModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant ShortcutModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const ShortcutEntry &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return entry.name;
    case IconSourceRole:
        return entry.iconSource;
    case IconNameRole:
        return entry.iconName;
    case TypeRole:
        return entry.type;
    case TargetRole:
        return entry.target;
    case DesktopFileRole:
        return entry.desktopFile;
    case ExecutableRole:
        return entry.executable;
    case SourcePathRole:
        return entry.sourcePath;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ShortcutModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[IconSourceRole] = "iconSource";
    roles[IconNameRole] = "iconName";
    roles[TypeRole] = "type";
    roles[TargetRole] = "target";
    roles[DesktopFileRole] = "desktopFile";
    roles[ExecutableRole] = "executable";
    roles[SourcePathRole] = "sourcePath";
    return roles;
}

void ShortcutModel::refresh()
{
    load();
}

int ShortcutModel::shortcutCount() const
{
    return m_entries.size();
}

QVariantMap ShortcutModel::loadSlotMap() const
{
    QVariantMap result;
    QFile file(slotMapFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return result;
    }

    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (!it.value().isDouble()) {
            continue;
        }
        const int slot = it.value().toInt(-1);
        if (slot >= 0) {
            result.insert(it.key(), slot);
        }
    }
    return result;
}

bool ShortcutModel::saveSlotMap(const QVariantMap &slotMap) const
{
    if (!ensureDesktopDir()) {
        return false;
    }

    QJsonObject obj;
    for (auto it = slotMap.constBegin(); it != slotMap.constEnd(); ++it) {
        bool ok = false;
        const int slot = it.value().toInt(&ok);
        if (ok && slot >= 0) {
            obj.insert(it.key(), slot);
        }
    }

    QFile file(slotMapFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.close();
    return true;
}

QString ShortcutModel::desktopDirectoryPath() const
{
    return QDir::home().filePath(QStringLiteral("Desktop"));
}

QString ShortcutModel::orderFilePath() const
{
    return QDir(desktopDirectoryPath()).filePath(QStringLiteral(".desktop_shell_shortcut_order"));
}

QString ShortcutModel::slotMapFilePath() const
{
    return QDir(desktopDirectoryPath()).filePath(QStringLiteral(".desktop_shell_slot_map.json"));
}

bool ShortcutModel::ensureDesktopDir() const
{
    QDir dir(desktopDirectoryPath());
    return dir.exists() || dir.mkpath(QStringLiteral("."));
}

QStringList ShortcutModel::loadOrder() const
{
    QStringList order;
    QFile file(orderFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return order;
    }
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            order.push_back(line);
        }
    }
    return order;
}

void ShortcutModel::saveOrder() const
{
    if (!ensureDesktopDir()) {
        return;
    }
    QFile file(orderFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }
    QTextStream out(&file);
    for (const ShortcutEntry &entry : m_entries) {
        if (entry.sourcePath.isEmpty()) {
            continue;
        }
        out << QFileInfo(entry.sourcePath).fileName() << '\n';
    }
}

void ShortcutModel::setupDesktopWatcher()
{
    const QStringList existing = m_desktopWatcher.directories();
    if (!existing.isEmpty()) {
        m_desktopWatcher.removePaths(existing);
    }

    const QString desktopDir = desktopDirectoryPath();
    if (QFileInfo::exists(desktopDir) && QFileInfo(desktopDir).isDir()) {
        m_desktopWatcher.addPath(desktopDir);
    }
}

void ShortcutModel::scheduleRefreshFromWatcher()
{
    m_desktopWatchDebounce.start();
}

void ShortcutModel::load()
{
    QVector<ShortcutEntry> nextEntries;
    if (!ensureDesktopDir()) {
        beginResetModel();
        m_entries.clear();
        endResetModel();
        setupDesktopWatcher();
        return;
    }

    QDir desktopDir(desktopDirectoryPath());
    const QFileInfoList files = desktopDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    const QStringList order = loadOrder();
    nextEntries.reserve(files.size());

    for (const QFileInfo &info : files) {
        if (info.fileName().startsWith('.')) {
            continue;
        }

        ShortcutEntry entry;
        entry.sourcePath = info.absoluteFilePath();

        const QString suffix = info.suffix().toLower();
        if (suffix == QStringLiteral("desktop")) {
            if (shouldSkipDesktopEntry(info.absoluteFilePath())) {
                continue;
            }
            GDesktopAppInfo *appInfo =
                g_desktop_app_info_new_from_filename(info.absoluteFilePath().toUtf8().constData());
            if (!appInfo) {
                QFile fallbackFile(info.absoluteFilePath());
                if (!fallbackFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    continue;
                }
                QTextStream in(&fallbackFile);
                QString fallbackName = info.completeBaseName();
                QString fallbackIcon;
                QString fallbackExec;
                QString fallbackUrl;
                while (!in.atEnd()) {
                    const QString line = in.readLine().trimmed();
                    if (line.startsWith(QLatin1Char('#')) || line.isEmpty()) {
                        continue;
                    }
                    if (line.startsWith(QStringLiteral("Name="), Qt::CaseInsensitive) &&
                        fallbackName == info.completeBaseName()) {
                        fallbackName = line.mid(5).trimmed();
                    } else if (line.startsWith(QStringLiteral("Icon="), Qt::CaseInsensitive) &&
                               fallbackIcon.isEmpty()) {
                        fallbackIcon = line.mid(5).trimmed();
                    } else if (line.startsWith(QStringLiteral("Exec="), Qt::CaseInsensitive) &&
                               fallbackExec.isEmpty()) {
                        fallbackExec = line.mid(5).trimmed();
                    } else if (line.startsWith(QStringLiteral("URL="), Qt::CaseInsensitive) &&
                               fallbackUrl.isEmpty()) {
                        fallbackUrl = line.mid(4).trimmed();
                    }
                }
                fallbackFile.close();

                if (!fallbackExec.isEmpty()) {
                    fallbackExec.replace(QRegularExpression(QStringLiteral("%[fFuUdDnNickvm]")), QString());
                    fallbackExec = fallbackExec.trimmed();
                }

                entry.name = fallbackName.isEmpty() ? info.completeBaseName() : fallbackName;
                entry.desktopFile = info.absoluteFilePath();
                entry.executable = fallbackExec;
                entry.iconName = fallbackIcon;
                entry.removable = true;
                if (!fallbackUrl.isEmpty()) {
                    const QUrl url = QUrl::fromUserInput(fallbackUrl);
                    if (url.isValid() && (url.scheme().toLower() == QStringLiteral("http") ||
                                          url.scheme().toLower() == QStringLiteral("https"))) {
                        entry.type = QStringLiteral("web");
                        entry.target = url.toString();
                    } else {
                        entry.type = QStringLiteral("file");
                        entry.target = url.isValid() ? url.toString() : fallbackUrl;
                    }
                } else {
                    entry.type = QStringLiteral("desktop");
                    entry.target = QUrl::fromLocalFile(entry.desktopFile).toString();
                }
                nextEntries.push_back(entry);
                continue;
            }

            entry.name = fromUtf8(g_app_info_get_display_name(G_APP_INFO(appInfo)));
            if (entry.name.isEmpty()) {
                entry.name = info.completeBaseName();
            }
            entry.desktopFile = info.absoluteFilePath();
            entry.executable = fromUtf8(g_app_info_get_executable(G_APP_INFO(appInfo)));
            entry.removable = true;

            const gchar *iconKey = g_desktop_app_info_get_string(appInfo, "Icon");
            if (iconKey) {
                entry.iconName = fromUtf8(iconKey);
            }

            const QString urlValue = fromUtf8(g_desktop_app_info_get_string(appInfo, "URL"));
            if (!urlValue.trimmed().isEmpty()) {
                const QUrl url = QUrl::fromUserInput(urlValue.trimmed());
                if (url.isValid() && (url.scheme().toLower() == QStringLiteral("http") ||
                                      url.scheme().toLower() == QStringLiteral("https"))) {
                    entry.type = QStringLiteral("web");
                    entry.target = url.toString();
                } else {
                    entry.type = QStringLiteral("file");
                    entry.target = url.isValid() ? url.toString() : urlValue.trimmed();
                }
            } else {
                entry.type = QStringLiteral("desktop");
                entry.target = QUrl::fromLocalFile(entry.desktopFile).toString();
            }

            GIcon *icon = g_app_info_get_icon(G_APP_INFO(appInfo));
            if (entry.iconName.isEmpty()) {
                entry.iconName = iconNameFromGIcon(icon);
            }
            entry.iconSource = iconPathFromGIcon(icon);
            g_object_unref(appInfo);
            nextEntries.push_back(entry);
            continue;
        }

        if (suffix == QStringLiteral("url") || suffix == QStringLiteral("webloc")) {
            QFile file(info.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            QTextStream in(&file);
            QString urlText;
            while (!in.atEnd()) {
                const QString line = in.readLine().trimmed();
                if (line.startsWith(QStringLiteral("URL="), Qt::CaseInsensitive)) {
                    urlText = line.mid(4).trimmed();
                    break;
                }
            }
            if (urlText.isEmpty()) {
                continue;
            }

            const QUrl url = QUrl::fromUserInput(urlText);
            entry.type = (url.scheme().toLower() == QStringLiteral("http") ||
                          url.scheme().toLower() == QStringLiteral("https"))
                             ? QStringLiteral("web")
                             : QStringLiteral("file");
            entry.target = url.isValid() ? url.toString() : urlText;
            entry.name = info.completeBaseName();
            entry.iconName = (entry.type == QStringLiteral("web"))
                                 ? QStringLiteral("internet-web-browser")
                                 : QStringLiteral("text-x-generic");
            entry.removable = true;
            nextEntries.push_back(entry);
            continue;
        }

        entry.type = QStringLiteral("file");
        entry.target = QUrl::fromLocalFile(info.absoluteFilePath()).toString();
        entry.name = info.fileName();
        entry.iconName = info.isDir() ? QStringLiteral("folder") : QStringLiteral("text-x-generic");
        entry.removable = info.isSymLink();
        nextEntries.push_back(entry);
    }

    if (!order.isEmpty()) {
        QHash<QString, int> orderIndex;
        orderIndex.reserve(order.size());
        for (int i = 0; i < order.size(); ++i) {
            orderIndex.insert(order.at(i), i);
        }

        std::sort(nextEntries.begin(), nextEntries.end(),
                  [&orderIndex](const ShortcutEntry &a, const ShortcutEntry &b) {
                      const QString keyA = QFileInfo(a.sourcePath).fileName();
                      const QString keyB = QFileInfo(b.sourcePath).fileName();
                      const int idxA = orderIndex.contains(keyA) ? orderIndex.value(keyA) : INT_MAX;
                      const int idxB = orderIndex.contains(keyB) ? orderIndex.value(keyB) : INT_MAX;
                      if (idxA != idxB) {
                          return idxA < idxB;
                      }
                      return QString::localeAwareCompare(a.name, b.name) < 0;
                  });
    }

    beginResetModel();
    m_entries = std::move(nextEntries);
    endResetModel();
    setupDesktopWatcher();
}

bool ShortcutModel::containsDesktopFile(const QString &desktopFilePath) const
{
    const QString normalized = QFileInfo(desktopFilePath).absoluteFilePath();
    for (const ShortcutEntry &entry : m_entries) {
        if (entry.type == QStringLiteral("desktop") &&
            QFileInfo(entry.desktopFile).absoluteFilePath() == normalized) {
            return true;
        }
    }
    return false;
}

bool ShortcutModel::containsTarget(const QString &type, const QString &target) const
{
    for (const ShortcutEntry &entry : m_entries) {
        if (entry.type == type && entry.target == target) {
            return true;
        }
    }
    return false;
}

bool ShortcutModel::addDesktopShortcut(const QString &desktopFilePath)
{
    if (!ensureDesktopDir()) {
        return false;
    }

    const QString resolvedPath = resolveDesktopFilePath(desktopFilePath);
    const QFileInfo srcInfo(resolvedPath);
    if (!srcInfo.exists() || !srcInfo.isFile() || srcInfo.suffix().toLower() != QStringLiteral("desktop")) {
        return false;
    }
    if (containsDesktopFile(srcInfo.absoluteFilePath())) {
        return false;
    }

    const QString destination =
        uniqueDesktopFilePath(desktopDirectoryPath(), srcInfo.completeBaseName().isEmpty()
                                                       ? srcInfo.fileName()
                                                       : srcInfo.completeBaseName());
    if (!QFile::copy(srcInfo.absoluteFilePath(), destination)) {
        return false;
    }

    refresh();
    saveOrder();
    return true;
}

bool ShortcutModel::addDesktopShortcutById(const QString &desktopId)
{
    return addDesktopShortcut(desktopId);
}

bool ShortcutModel::addAppShortcut(const QString &name, const QString &executable, const QString &iconName)
{
    if (!ensureDesktopDir()) {
        return false;
    }

    const QString appName = sanitizeFileName(name.trimmed().isEmpty() ? QStringLiteral("Application") : name);
    const QString exec = executable.trimmed();
    if (exec.isEmpty()) {
        return false;
    }

    for (const ShortcutEntry &entry : m_entries) {
        if (entry.type == QStringLiteral("desktop")) {
            const QString existingExec = entry.executable.trimmed();
            if (!existingExec.isEmpty() && existingExec.compare(exec, Qt::CaseInsensitive) == 0) {
                return false;
            }
        }
    }

    const QString filePath = uniqueDesktopFilePath(desktopDirectoryPath(), appName);
    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream stream(&out);
    stream << "[Desktop Entry]\n";
    stream << "Type=Application\n";
    stream << "Name=" << appName << "\n";
    stream << "Exec=" << exec << "\n";
    if (!iconName.trimmed().isEmpty()) {
        stream << "Icon=" << iconName.trimmed() << "\n";
    }
    stream << "Terminal=false\n";
    stream << "NoDisplay=false\n";
    out.close();

    refresh();
    saveOrder();
    return true;
}

bool ShortcutModel::addFileShortcut(const QString &filePathOrUrl)
{
    if (!ensureDesktopDir()) {
        return false;
    }

    QUrl url = QUrl::fromUserInput(filePathOrUrl);
    if (!url.isValid()) {
        return false;
    }
    if (url.scheme().isEmpty() || url.isLocalFile()) {
        const QString path = url.isLocalFile() ? url.toLocalFile() : filePathOrUrl;
        const QFileInfo info(path);
        if (!info.exists()) {
            return false;
        }
        url = QUrl::fromLocalFile(info.absoluteFilePath());
    }

    if (containsTarget(QStringLiteral("file"), url.toString())) {
        return false;
    }

    const QString title = sanitizeFileName(url.isLocalFile() ? QFileInfo(url.toLocalFile()).fileName()
                                                              : url.fileName());
    const QString filePath = uniqueDesktopFilePath(desktopDirectoryPath(), title);

    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream stream(&out);
    stream << "[Desktop Entry]\n";
    stream << "Type=Link\n";
    stream << "Name=" << title << "\n";
    stream << "URL=" << url.toString() << "\n";
    stream << "Icon=" << (url.isLocalFile() ? "folder" : "text-x-generic") << "\n";
    stream << "Terminal=false\n";
    out.close();

    refresh();
    saveOrder();
    return true;
}

bool ShortcutModel::addWebShortcut(const QString &webUrl, const QString &title)
{
    if (!ensureDesktopDir()) {
        return false;
    }

    const QUrl url = QUrl::fromUserInput(webUrl);
    if (!url.isValid()) {
        return false;
    }
    const QString scheme = url.scheme().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        return false;
    }
    if (containsTarget(QStringLiteral("web"), url.toString())) {
        return false;
    }

    QString name = title.trimmed();
    if (name.isEmpty()) {
        name = url.host();
    }
    if (name.isEmpty()) {
        name = QStringLiteral("Web Shortcut");
    }
    name = sanitizeFileName(name);

    const QString filePath = uniqueDesktopFilePath(desktopDirectoryPath(), name);
    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream stream(&out);
    stream << "[Desktop Entry]\n";
    stream << "Type=Link\n";
    stream << "Name=" << name << "\n";
    stream << "URL=" << url.toString() << "\n";
    stream << "Icon=internet-web-browser\n";
    stream << "Terminal=false\n";
    out.close();

    refresh();
    saveOrder();
    return true;
}

bool ShortcutModel::removeShortcut(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }

    const ShortcutEntry &entry = m_entries.at(index);
    if (!entry.removable || entry.sourcePath.isEmpty()) {
        return false;
    }

    const QFileInfo sourceInfo(entry.sourcePath);
    const QString desktopDir = QDir(desktopDirectoryPath()).absolutePath();
    if (sourceInfo.absoluteDir().absolutePath() != desktopDir) {
        return false;
    }

    if (!QFile::remove(sourceInfo.absoluteFilePath())) {
        return false;
    }

    refresh();
    saveOrder();
    return true;
}

bool ShortcutModel::moveShortcut(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || toIndex < 0 || fromIndex >= m_entries.size() || toIndex >= m_entries.size() ||
        fromIndex == toIndex) {
        return false;
    }

    if (!beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(),
                       (fromIndex < toIndex) ? (toIndex + 1) : toIndex)) {
        return false;
    }
    m_entries.move(fromIndex, toIndex);
    endMoveRows();
    saveOrder();
    return true;
}

bool ShortcutModel::arrangeShortcuts()
{
    if (m_entries.size() < 2) {
        return true;
    }

    auto priorityOf = [](const ShortcutEntry &entry) -> int {
        if (entry.type == QStringLiteral("desktop")) {
            return 0;
        }
        if (entry.type == QStringLiteral("web")) {
            return 1;
        }
        if (entry.type == QStringLiteral("file")) {
            const QFileInfo info(entry.sourcePath);
            if (info.exists() && info.isDir()) {
                return 2;
            }
            return 3;
        }
        return 3;
    };

    std::stable_sort(m_entries.begin(), m_entries.end(),
                     [&priorityOf](const ShortcutEntry &a, const ShortcutEntry &b) {
                         const int pa = priorityOf(a);
                         const int pb = priorityOf(b);
                         if (pa != pb) {
                             return pa < pb;
                         }
                         return QString::localeAwareCompare(a.name, b.name) < 0;
                     });

    beginResetModel();
    endResetModel();
    saveOrder();
    return true;
}

QVariantMap ShortcutModel::get(int index) const
{
    QVariantMap map;
    if (index < 0 || index >= m_entries.size()) {
        return map;
    }

    const ShortcutEntry &entry = m_entries.at(index);
    map.insert(QStringLiteral("name"), entry.name);
    map.insert(QStringLiteral("iconSource"), entry.iconSource);
    map.insert(QStringLiteral("iconName"), entry.iconName);
    map.insert(QStringLiteral("type"), entry.type);
    map.insert(QStringLiteral("target"), entry.target);
    map.insert(QStringLiteral("desktopFile"), entry.desktopFile);
    map.insert(QStringLiteral("executable"), entry.executable);
    map.insert(QStringLiteral("sourcePath"), entry.sourcePath);
    return map;
}

bool ShortcutModel::launchDesktop(const ShortcutEntry &entry) const
{
    if (!entry.desktopFile.trimmed().isEmpty()) {
        const QFileInfo info(entry.desktopFile);
        if (info.exists() && info.isFile()) {
            GDesktopAppInfo *appInfo =
                g_desktop_app_info_new_from_filename(info.absoluteFilePath().toUtf8().constData());
            if (appInfo) {
                GError *error = nullptr;
                const gboolean launched = g_app_info_launch(G_APP_INFO(appInfo), nullptr, nullptr, &error);
                if (error) {
                    g_error_free(error);
                }
                g_object_unref(appInfo);
                if (launched) {
                    return true;
                }
            }
        }
    }

    const QString exec = entry.executable.trimmed();
    if (exec.isEmpty()) {
        return false;
    }
    const QStringList parts = QProcess::splitCommand(exec);
    if (parts.isEmpty()) {
        return false;
    }
    return QProcess::startDetached(parts.first(), parts.mid(1));
}

bool ShortcutModel::launchShortcut(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }

    const ShortcutEntry &entry = m_entries.at(index);
    if (entry.type == QStringLiteral("desktop")) {
        return launchDesktop(entry);
    }

    const QUrl url = QUrl::fromUserInput(entry.target);
    if (!url.isValid()) {
        return false;
    }
    return QDesktopServices::openUrl(url);
}
