#include "dockmodel.h"
#include "dockidentity.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <QDebug>
#include <QTextStream>
#include <QtCore/qobjectdefs.h>
#include <QRegularExpression>

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

QString normalizeExecutableName(const QString &execRaw)
{
    QString exec = execRaw.trimmed();
    if (exec.isEmpty()) {
        return QString();
    }
    exec = exec.section(' ', 0, 0).trimmed();
    exec = exec.section('/', -1).trimmed();
    return exec.toLower();
}

QString normalizeToken(const QString &value)
{
    return DockIdentity::normalizeToken(value);
}

QString prettifyToken(const QString &value)
{
    QString text = value.trimmed();
    if (text.isEmpty()) {
        return QStringLiteral("App");
    }
    text.replace('.', ' ');
    text.replace('-', ' ');
    text.replace('_', ' ');
    text = text.simplified();
    if (!text.isEmpty()) {
        text[0] = text[0].toUpper();
    }
    return text;
}

QSet<QString> executableCandidates(const QString &raw)
{
    QSet<QString> candidates;
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        return candidates;
    }

    const QStringList parts = QProcess::splitCommand(trimmed);
    if (!parts.isEmpty()) {
        const QString program = QFileInfo(parts.first()).fileName().trimmed().toLower();
        if (!program.isEmpty()) {
            candidates.insert(program);
            candidates.insert(normalizeToken(program));
        }
        for (const QString &part : parts) {
            const QString p = part.trimmed().toLower();
            if (p.isEmpty()) {
                continue;
            }
            candidates.insert(normalizeToken(p));
            const QString base = QFileInfo(p).fileName().trimmed().toLower();
            if (!base.isEmpty()) {
                candidates.insert(base);
                candidates.insert(normalizeToken(base));
            }
        }
    }

    const QString exec = normalizeExecutableName(trimmed);
    if (exec.isEmpty()) {
        return candidates;
    }

    candidates.insert(exec);
    candidates.insert(normalizeToken(exec));

    if (exec.endsWith(QStringLiteral("-bin")) && exec.size() > 4) {
        candidates.insert(exec.left(exec.size() - 4));
    }

    const QStringList dashParts = exec.split('-', Qt::SkipEmptyParts);
    if (dashParts.size() > 1) {
        QString prefix;
        for (int i = 0; i < dashParts.size(); ++i) {
            prefix += (i == 0 ? QString() : QStringLiteral("-")) + dashParts.at(i);
            candidates.insert(prefix);
        }
    }

    if (exec.contains(QStringLiteral("chrome"))) {
        candidates.insert(QStringLiteral("chrome"));
        candidates.insert(QStringLiteral("google-chrome"));
        candidates.insert(QStringLiteral("google-chrome-stable"));
        candidates.insert(QStringLiteral("chromium"));
        candidates.insert(QStringLiteral("chromium-browser"));
    }

    return candidates;
}

bool isGenericWrapperToken(const QString &tokenRaw)
{
    return DockIdentity::isGenericWrapperToken(tokenRaw);
}

bool isWrapperLikeToken(const QString &tokenRaw)
{
    return DockIdentity::isWrapperLikeToken(tokenRaw);
}

bool isWeakIdentityToken(const QString &tokenRaw)
{
    return DockIdentity::isWeakIdentityToken(tokenRaw);
}

bool isStrongIdentityToken(const QString &tokenRaw)
{
    return DockIdentity::isStrongIdentityToken(tokenRaw);
}

QSet<QString> runtimeTokensFromDesktopFile(const QString &desktopFilePath)
{
    QSet<QString> tokens;
    if (desktopFilePath.trimmed().isEmpty() || !QFileInfo::exists(desktopFilePath)) {
        return tokens;
    }

    GDesktopAppInfo *info =
        g_desktop_app_info_new_from_filename(desktopFilePath.toUtf8().constData());
    if (!info) {
        return tokens;
    }

    const QString appId = normalizeToken(fromUtf8(g_app_info_get_id(G_APP_INFO(info))));
    if (!appId.isEmpty()) {
        tokens.insert(appId);
        if (appId.endsWith(QStringLiteral("desktop"))) {
            tokens.insert(appId.left(appId.size() - 7));
        }
    }

    const QString startupClass = normalizeToken(fromUtf8(g_desktop_app_info_get_startup_wm_class(info)));
    if (!startupClass.isEmpty()) {
        tokens.insert(startupClass);
    }

    const QString xFlatpak = normalizeToken(fromUtf8(g_desktop_app_info_get_string(info, "X-Flatpak")));
    if (!xFlatpak.isEmpty()) {
        tokens.insert(xFlatpak);
    }

    g_object_unref(info);
    return tokens;
}

QSet<QString> entryRuntimeTokens(const DockEntry &entry)
{
    QSet<QString> tokens = executableCandidates(entry.executable);

    const QString nameToken = normalizeToken(entry.name);
    if (!nameToken.isEmpty()) {
        tokens.insert(nameToken);
    }

    const QString baseName = QFileInfo(entry.desktopFile).completeBaseName();
    QString normalizedBase = normalizeToken(baseName);
    if (!normalizedBase.isEmpty()) {
        tokens.insert(normalizedBase);
    }

    // Remove copy suffix like " (7)" from desktop filename and keep core token.
    QString strippedBase = baseName;
    strippedBase.remove(QRegularExpression(QStringLiteral("\\s*\\(\\d+\\)$")));
    strippedBase = normalizeToken(strippedBase);
    if (!strippedBase.isEmpty()) {
        tokens.insert(strippedBase);
    }

    tokens.unite(runtimeTokensFromDesktopFile(entry.desktopFile));
    return tokens;
}

QSet<QString> launchHintTokens(const QSet<QString> &tokens)
{
    return DockIdentity::filterLaunchHintTokens(tokens);
}

void addRunningCandidates(QSet<QString> &runningExecutables, const QString &raw)
{
    const auto candidates = executableCandidates(raw);
    for (const QString &candidate : candidates) {
        if (!candidate.isEmpty()) {
            runningExecutables.insert(candidate);
        }
    }
}

bool containsAnyCandidate(const QSet<QString> &runningExecutables, const QString &raw)
{
    const auto candidates = executableCandidates(raw);
    for (const QString &candidate : candidates) {
        if (runningExecutables.contains(candidate)) {
            return true;
        }
    }
    return false;
}

QSet<QString> windowMatchTokens(const QString &desktopFilePath, const QString &executable,
                                const QString &displayName)
{
    QSet<QString> tokens = executableCandidates(executable);

    const QString desktopBase = normalizeToken(QFileInfo(desktopFilePath).completeBaseName());
    if (!desktopBase.isEmpty()) {
        tokens.insert(desktopBase);
    }

    const QString nameToken = normalizeToken(displayName);
    if (!nameToken.isEmpty()) {
        tokens.insert(nameToken);
    }

    if (!desktopFilePath.trimmed().isEmpty() && QFileInfo::exists(desktopFilePath)) {
        GDesktopAppInfo *info =
            g_desktop_app_info_new_from_filename(desktopFilePath.toUtf8().constData());
        if (info) {
            const QString startupClass = normalizeToken(fromUtf8(g_desktop_app_info_get_startup_wm_class(info)));
            if (!startupClass.isEmpty()) {
                tokens.insert(startupClass);
            }
            const QString appId = normalizeToken(fromUtf8(g_app_info_get_id(G_APP_INFO(info))));
            if (!appId.isEmpty()) {
                tokens.insert(appId);
                if (appId.endsWith(QStringLiteral(".desktop"))) {
                    tokens.insert(appId.left(appId.size() - 8));
                }
            }
            g_object_unref(info);
        }
    }

    return tokens;
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

    const gchar *serialized = g_icon_to_string(icon);
    if (!serialized) {
        return QString();
    }
    const QString maybePath = fromUtf8(serialized);
    if (!maybePath.isEmpty() && QFileInfo::exists(maybePath)) {
        return QUrl::fromLocalFile(maybePath).toString();
    }
    return QString();
}

QSet<QString> wmClassTokensFromLine(const QString &wmctrlLine)
{
    QSet<QString> out;
    const QString trimmed = wmctrlLine.trimmed();
    if (trimmed.isEmpty()) {
        return out;
    }

    const QStringList fields =
        trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (fields.size() < 4) {
        return out;
    }

    const QString wmClassRaw = fields.at(3).trimmed().toLower();
    if (wmClassRaw.isEmpty()) {
        return out;
    }

    out.insert(normalizeToken(wmClassRaw));
    const QStringList parts = wmClassRaw.split('.', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString token = normalizeToken(part);
        if (!token.isEmpty()) {
            out.insert(token);
        }
    }
    return out;
}
} // namespace

DockModel::DockModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_localSettings()
{
    m_verboseLogging = m_localSettings.value(QStringLiteral("debug/verboseLogging"), false).toBool();
    ensureDockDirectory();
    QTimer::singleShot(0, this, &DockModel::refresh);

    m_runningWatchDebounce.setSingleShot(true);
    m_runningWatchDebounce.setInterval(120);
    connect(&m_runningWatchDebounce, &QTimer::timeout, this, &DockModel::refreshRunningStates);
    connect(&m_procWatcher, &QFileSystemWatcher::directoryChanged, this, &DockModel::scheduleRunningRefresh);
    setupProcessWatcher();

    m_runningPollTimer.setInterval(700);
    m_runningPollTimer.setSingleShot(false);
    connect(&m_runningPollTimer, &QTimer::timeout, this, &DockModel::refreshRunningStates);
    m_runningPollTimer.start();
}

int DockModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant DockModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const DockEntry &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return entry.name;
    case IconSourceRole:
        return entry.iconSource;
    case IconNameRole:
        return entry.iconName;
    case DesktopFileRole:
        return entry.desktopFile;
    case ExecutableRole:
        return entry.executable;
    case IsRunningRole:
        return entry.isRunning;
    case IsPinnedRole:
        return entry.isPinned;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> DockModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[IconSourceRole] = "iconSource";
    roles[IconNameRole] = "iconName";
    roles[DesktopFileRole] = "desktopFile";
    roles[ExecutableRole] = "executable";
    roles[IsRunningRole] = "isRunning";
    roles[IsPinnedRole] = "isPinned";
    return roles;
}

QString DockModel::dockDirectoryPath() const
{
    return QDir::home().filePath(QStringLiteral(".Dock"));
}

QString DockModel::orderFilePath() const
{
    return QDir(dockDirectoryPath()).filePath(QStringLiteral(".order"));
}

bool DockModel::ensureDockDirectory()
{
    QDir dir(dockDirectoryPath());
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(QStringLiteral("."));
}

QStringList DockModel::loadDockOrder() const
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

void DockModel::saveDockOrder(const QVector<DockEntry> &entries) const
{
    QFile file(orderFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }

    QTextStream out(&file);
    for (const DockEntry &entry : entries) {
        if (!entry.isPinned) {
            continue;
        }
        const QString fileName = QFileInfo(entry.desktopFile).fileName();
        if (!fileName.isEmpty()) {
            out << fileName << '\n';
        }
    }
}

void DockModel::refresh()
{
    if (!ensureDockDirectory()) {
        beginResetModel();
        m_entries.clear();
        endResetModel();
        return;
    }

    const QDir dockDir(dockDirectoryPath());
    const QStringList desktopFiles = dockDir.entryList(QStringList() << "*.desktop", QDir::Files, QDir::Name);
    const QStringList order = loadDockOrder();

    QVector<DockEntry> nextEntries;
    nextEntries.reserve(desktopFiles.size());

    for (const QString &fileName : desktopFiles) {
        const QString absPath = dockDir.filePath(fileName);
        GDesktopAppInfo *info = g_desktop_app_info_new_from_filename(absPath.toUtf8().constData());
        if (!info) {
            continue;
        }

        DockEntry entry;
        entry.name = fromUtf8(g_app_info_get_display_name(G_APP_INFO(info)));
        if (entry.name.trimmed().isEmpty()) {
            entry.name = QFileInfo(fileName).completeBaseName();
        }
        entry.desktopFile = absPath;
        entry.executable = fromUtf8(g_app_info_get_executable(G_APP_INFO(info)));
        entry.isPinned = true;

        const gchar *iconKey = g_desktop_app_info_get_string(info, "Icon");
        if (iconKey) {
            entry.iconName = fromUtf8(iconKey);
        }

        GIcon *icon = g_app_info_get_icon(G_APP_INFO(info));
        if (entry.iconName.isEmpty()) {
            entry.iconName = iconNameFromGIcon(icon);
        }
        entry.iconSource = iconPathFromGIcon(icon);

        nextEntries.push_back(entry);
        g_object_unref(info);
    }

    std::sort(nextEntries.begin(), nextEntries.end(), [&order](const DockEntry &a, const DockEntry &b) {
        const QString fileA = QFileInfo(a.desktopFile).fileName();
        const QString fileB = QFileInfo(b.desktopFile).fileName();
        const int idxA = order.indexOf(fileA);
        const int idxB = order.indexOf(fileB);

        if (idxA >= 0 && idxB >= 0 && idxA != idxB) {
            return idxA < idxB;
        }
        if (idxA >= 0 && idxB < 0) {
            return true;
        }
        if (idxA < 0 && idxB >= 0) {
            return false;
        }
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    beginResetModel();
    m_entries = std::move(nextEntries);
    endResetModel();

    refreshRunningStates();
}

bool DockModel::addDesktopEntry(const QString &sourceDesktopFilePath)
{
    const QFileInfo srcInfo(sourceDesktopFilePath);
    if (!srcInfo.exists() || !srcInfo.isFile() || srcInfo.suffix().toLower() != QStringLiteral("desktop")) {
        return false;
    }
    if (!ensureDockDirectory()) {
        return false;
    }

    const QString destination = QDir(dockDirectoryPath()).filePath(srcInfo.fileName());
    if (QFileInfo(destination).absoluteFilePath() != srcInfo.absoluteFilePath() && QFile::exists(destination)) {
        QFile::remove(destination);
    }

    bool copied = true;
    if (QFileInfo(destination).absoluteFilePath() != srcInfo.absoluteFilePath()) {
        copied = QFile::copy(srcInfo.absoluteFilePath(), destination);
    }

    if (!copied) {
        return false;
    }

    // Keep insertion order deterministic for pinned entries.
    QVector<DockEntry> temp;
    temp.reserve(m_entries.size() + 1);
    temp += m_entries;
    DockEntry newEntry;
    newEntry.desktopFile = destination;
    newEntry.isPinned = true;
    temp.push_back(newEntry);
    saveDockOrder(temp);

    refresh();
    return true;
}

bool DockModel::addDesktopEntryAt(const QString &sourceDesktopFilePath, int pinnedPosition)
{
    const QFileInfo srcInfo(sourceDesktopFilePath);
    if (!srcInfo.exists() || !srcInfo.isFile() || srcInfo.suffix().toLower() != QStringLiteral("desktop")) {
        return false;
    }
    if (!ensureDockDirectory()) {
        return false;
    }

    const QString destination = QDir(dockDirectoryPath()).filePath(srcInfo.fileName());
    if (QFileInfo(destination).absoluteFilePath() != srcInfo.absoluteFilePath() && QFile::exists(destination)) {
        QFile::remove(destination);
    }

    bool copied = true;
    if (QFileInfo(destination).absoluteFilePath() != srcInfo.absoluteFilePath()) {
        copied = QFile::copy(srcInfo.absoluteFilePath(), destination);
    }
    if (!copied) {
        return false;
    }

    refresh();

    QVector<DockEntry> pinnedEntries;
    QVector<DockEntry> transientEntries;
    pinnedEntries.reserve(m_entries.size());
    transientEntries.reserve(m_entries.size());
    for (const DockEntry &entry : m_entries) {
        if (entry.isPinned) {
            pinnedEntries.push_back(entry);
        } else {
            transientEntries.push_back(entry);
        }
    }

    int currentPinnedPos = -1;
    for (int i = 0; i < pinnedEntries.size(); ++i) {
        if (QFileInfo(pinnedEntries.at(i).desktopFile).absoluteFilePath() ==
            QFileInfo(destination).absoluteFilePath()) {
            currentPinnedPos = i;
            break;
        }
    }
    if (currentPinnedPos < 0) {
        return false;
    }

    DockEntry moved = pinnedEntries.takeAt(currentPinnedPos);
    int targetPos = pinnedPosition;
    if (targetPos < 0) {
        targetPos = 0;
    }
    if (targetPos > pinnedEntries.size()) {
        targetPos = pinnedEntries.size();
    }
    pinnedEntries.insert(targetPos, moved);

    QVector<DockEntry> ordered;
    ordered.reserve(pinnedEntries.size() + transientEntries.size());
    ordered += pinnedEntries;
    ordered += transientEntries;
    saveDockOrder(ordered);
    refresh();
    return true;
}

bool DockModel::removeDesktopEntry(const QString &desktopFilePath)
{
    const QFileInfo info(desktopFilePath);
    if (!info.exists() || !info.isFile() || info.suffix().toLower() != QStringLiteral("desktop")) {
        return false;
    }

    const QString dockDirPath = QDir(dockDirectoryPath()).absolutePath();
    const QString entryDirPath = info.absoluteDir().absolutePath();
    if (entryDirPath != dockDirPath) {
        return false;
    }

    if (!QFile::remove(info.absoluteFilePath())) {
        return false;
    }

    refresh();
    return true;
}

bool DockModel::launchDesktopEntry(const QString &desktopFilePath, const QString &executable)
{
    if (!desktopFilePath.trimmed().isEmpty()) {
        const QFileInfo desktopInfo(desktopFilePath);
        if (desktopInfo.exists() && desktopInfo.isFile()) {
            GDesktopAppInfo *info =
                g_desktop_app_info_new_from_filename(desktopInfo.absoluteFilePath().toUtf8().constData());
            if (info) {
                GError *error = nullptr;
                const gboolean launched = g_app_info_launch(G_APP_INFO(info), nullptr, nullptr, &error);
                if (error) {
                    g_error_free(error);
                }
                g_object_unref(info);
                if (launched) {
                    if (m_verboseLogging) {
                        qInfo().noquote() << "DockLaunch: via desktop file success name="
                                          << QFileInfo(desktopFilePath).fileName()
                                          << "exec=" << executable;
                    }
                    return true;
                }
            }
        }
    }

    const QString exec = executable.trimmed();
    if (exec.isEmpty()) {
        return false;
    }

    const QStringList parts = QProcess::splitCommand(exec);
    if (parts.isEmpty()) {
        return false;
    }
    const QString program = parts.first();
    const QStringList arguments = parts.mid(1);
    qint64 pid = -1;
    const bool started = QProcess::startDetached(program, arguments, QString(), &pid);
    if (started) {
        QSet<QString> launchTokens = executableCandidates(executable);
        launchTokens.unite(executableCandidates(program));
        m_runtimeRegistry.noteUserLaunch(launchHintTokens(launchTokens), pid);
        if (m_verboseLogging) {
            qInfo().noquote() << "DockLaunch: startDetached success program=" << program
                              << "pid=" << pid << "exec=" << executable;
        }
    } else {
        if (m_verboseLogging) {
            qInfo().noquote() << "DockLaunch: startDetached failed program=" << program
                              << "exec=" << executable;
        }
    }
    return started;
}

bool DockModel::activateExistingWindow(const QString &desktopFilePath, const QString &executable,
                                       const QString &displayName)
{
    const bool ok = focusExistingWindow(desktopFilePath, executable, displayName);
    if (m_verboseLogging) {
        qInfo().noquote() << "DockFocus: name=" << displayName
                          << "desktop=" << desktopFilePath
                          << "exec=" << executable
                          << "ok=" << ok;
    }
    return ok;
}

bool DockModel::activateOrLaunch(const QString &desktopFilePath, const QString &executable,
                                 const QString &displayName)
{
    if (focusExistingWindow(desktopFilePath, executable, displayName)) {
        if (m_verboseLogging) {
            qInfo().noquote() << "DockActivateOrLaunch: focused existing window name=" << displayName
                              << "desktop=" << desktopFilePath;
        }
        return true;
    }
    const bool launched = launchDesktopEntry(desktopFilePath, executable);
    if (m_verboseLogging) {
        qInfo().noquote() << "DockActivateOrLaunch: launched name=" << displayName
                          << "desktop=" << desktopFilePath
                          << "exec=" << executable
                          << "ok=" << launched;
    }
    return launched;
}

void DockModel::noteLaunchedEntry(const QString &desktopFilePath, const QString &executable,
                                  const QString &displayName, const QString &iconName,
                                  const QString &iconSource)
{
    const QString normDesktop = QFileInfo(desktopFilePath).absoluteFilePath();
    const QString normExec = normalizeExecutableName(executable);
    const bool execIsGenericWrapper = isGenericWrapperToken(normExec) || !isStrongIdentityToken(normExec);
    const QString normName = displayName.trimmed();
    if (!normDesktop.isEmpty()) {
        m_desktopPathLaunchHints.insert(normDesktop, QDateTime::currentMSecsSinceEpoch() + 6000);
    }

    for (int i = 0; i < m_entries.size(); ++i) {
        DockEntry &entry = m_entries[i];
        const bool sameDesktop = !normDesktop.isEmpty() &&
                                 QFileInfo(entry.desktopFile).absoluteFilePath() == normDesktop;
        const bool sameExec = !execIsGenericWrapper &&
                              !normExec.isEmpty() &&
                              normalizeExecutableName(entry.executable) == normExec;
        const bool sameName = !normName.isEmpty() &&
                              isStrongIdentityToken(normName) &&
                              QString::compare(entry.name.trimmed(), normName, Qt::CaseInsensitive) == 0;
        if (sameDesktop || sameExec || sameName) {
            m_runtimeRegistry.noteUserLaunch(launchHintTokens(entryRuntimeTokens(entry)));
            if (m_verboseLogging) {
                qInfo().noquote() << "DockNoteLaunch: matched existing entry name=" << entry.name
                                  << "desktop=" << entry.desktopFile
                                  << "exec=" << entry.executable;
            }
            if (!entry.isRunning) {
                entry.isRunning = true;
                const QModelIndex idx = index(i, 0);
                emit dataChanged(idx, idx, QVector<int>{IsRunningRole});
            }
            QTimer::singleShot(300, this, &DockModel::refreshRunningStates);
            QTimer::singleShot(1500, this, &DockModel::refreshRunningStates);
            return;
        }
    }

    DockEntry transient;
    transient.name = normName.isEmpty() ? QStringLiteral("App") : normName;
    transient.desktopFile = desktopFilePath;
    transient.executable = executable;
    transient.iconName = iconName;
    transient.iconSource = iconSource;
    transient.isPinned = false;
    transient.isRunning = true;
    m_runtimeRegistry.noteUserLaunch(launchHintTokens(entryRuntimeTokens(transient)));
    if (!normDesktop.isEmpty()) {
        m_pendingTransientByDesktop.insert(normDesktop, transient);
        m_pendingTransientExpiryByDesktop.insert(normDesktop, QDateTime::currentMSecsSinceEpoch() + 6000);
    }
    if (m_verboseLogging) {
        qInfo().noquote() << "DockNoteLaunch: created transient entry name=" << transient.name
                          << "desktop=" << transient.desktopFile
                          << "exec=" << transient.executable;
    }

    int insertPos = 0;
    for (const DockEntry &entry : m_entries) {
        if (entry.isPinned) {
            ++insertPos;
        } else {
            break;
        }
    }

    beginInsertRows(QModelIndex(), insertPos, insertPos);
    m_entries.insert(insertPos, transient);
    endInsertRows();

    QTimer::singleShot(300, this, &DockModel::refreshRunningStates);
    QTimer::singleShot(1500, this, &DockModel::refreshRunningStates);
}

bool DockModel::focusExistingWindow(const QString &desktopFilePath, const QString &executable,
                                    const QString &displayName) const
{
    const QString wmctrlPath = QStandardPaths::findExecutable(QStringLiteral("wmctrl"));
    if (wmctrlPath.isEmpty()) {
        return false;
    }

    QProcess listProc;
    listProc.start(wmctrlPath, QStringList{QStringLiteral("-lx")});
    if (!listProc.waitForFinished(350)) {
        listProc.kill();
        return false;
    }
    if (listProc.exitStatus() != QProcess::NormalExit || listProc.exitCode() != 0) {
        return false;
    }

    const QString output = QString::fromUtf8(listProc.readAllStandardOutput());
    if (output.trimmed().isEmpty()) {
        return false;
    }

    const QSet<QString> tokenSet = windowMatchTokens(desktopFilePath, executable, displayName);
    QStringList tokens = tokenSet.values();
    if (tokens.isEmpty()) {
        return false;
    }

    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    QString selectedWindowId;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const QStringList fields = trimmed.split(QRegularExpression(QStringLiteral("\\s+")),
                                                 Qt::SkipEmptyParts);
        if (fields.size() < 4) {
            continue;
        }
        const QString wmClass = normalizeToken(fields.at(3));
        const QString lower = normalizeToken(trimmed);
        bool matched = false;
        for (const QString &token : tokens) {
            if (token.isEmpty()) {
                continue;
            }
            if (lower.contains(token) || (!wmClass.isEmpty() && wmClass.contains(token))) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            continue;
        }

        const QString windowId = trimmed.section(' ', 0, 0).trimmed();
        if (windowId.startsWith(QStringLiteral("0x"))) {
            selectedWindowId = windowId;
            break;
        }
    }

    if (selectedWindowId.isEmpty()) {
        return false;
    }

    QProcess activateProc;
    activateProc.start(wmctrlPath, QStringList{QStringLiteral("-ia"), selectedWindowId});
    if (!activateProc.waitForFinished(350)) {
        activateProc.kill();
        return false;
    }
    return activateProc.exitStatus() == QProcess::NormalExit && activateProc.exitCode() == 0;
}

bool DockModel::movePinnedEntry(int fromModelIndex, int toModelIndex)
{
    if (fromModelIndex < 0 || fromModelIndex >= m_entries.size() ||
        toModelIndex < 0 || toModelIndex >= m_entries.size() ||
        fromModelIndex == toModelIndex) {
        return false;
    }

    QVector<int> pinnedModelIndices;
    pinnedModelIndices.reserve(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].isPinned) {
            pinnedModelIndices.push_back(i);
        }
    }

    const int fromPinnedPos = pinnedModelIndices.indexOf(fromModelIndex);
    const int toPinnedPos = pinnedModelIndices.indexOf(toModelIndex);
    if (fromPinnedPos < 0 || toPinnedPos < 0) {
        return false;
    }

    QVector<DockEntry> pinnedEntries;
    QVector<DockEntry> transientEntries;
    for (const DockEntry &entry : m_entries) {
        if (entry.isPinned) {
            pinnedEntries.push_back(entry);
        } else {
            transientEntries.push_back(entry);
        }
    }

    DockEntry moved = pinnedEntries.takeAt(fromPinnedPos);
    pinnedEntries.insert(toPinnedPos, moved);

    QVector<DockEntry> nextEntries;
    nextEntries.reserve(pinnedEntries.size() + transientEntries.size());
    nextEntries += pinnedEntries;
    nextEntries += transientEntries;

    beginResetModel();
    m_entries = std::move(nextEntries);
    endResetModel();

    saveDockOrder(m_entries);
    return true;
}

bool DockModel::isEntryRunning(const DockEntry &entry, const QSet<QString> &runningExecutables) const
{
    const QString normExec = normalizeExecutableName(entry.executable);
    if (isStrongIdentityToken(normExec) &&
        containsAnyCandidate(runningExecutables, entry.executable)) {
        return true;
    }

    const QSet<QString> tokens = entryRuntimeTokens(entry);
    for (const QString &token : tokens) {
        if (isStrongIdentityToken(token) && runningExecutables.contains(token)) {
            return true;
        }
    }
    return false;
}

bool DockModel::hasWindowForEntry(const DockEntry &entry, const QStringList &wmctrlLines) const
{
    const QSet<QString> tokenSet = windowMatchTokens(entry.desktopFile, entry.executable, entry.name);
    if (tokenSet.isEmpty()) {
        return false;
    }

    QSet<QString> tokens;
    QSet<QString> strongTokens;
    QSet<QString> weakTokens;
    for (const QString &token : tokenSet) {
        const QString normalized = normalizeToken(token);
        if (!normalized.isEmpty()) {
            tokens.insert(normalized);
            const bool strong = normalized.contains('.') ||
                                normalized.contains('-') ||
                                normalized.contains('_') ||
                                normalized.size() >= 8;
            if (strong) {
                strongTokens.insert(normalized);
            } else {
                weakTokens.insert(normalized);
            }
        }
    }
    if (tokens.isEmpty()) {
        return false;
    }

    for (const QString &line : wmctrlLines) {
        const QSet<QString> classTokens = wmClassTokensFromLine(line);
        if (classTokens.isEmpty()) {
            continue;
        }
        bool strongMatched = strongTokens.isEmpty();
        for (const QString &token : strongTokens) {
            if (token.isEmpty()) {
                continue;
            }
            if (classTokens.contains(token)) {
                strongMatched = true;
                break;
            }
            if (token.size() >= 5) {
                for (const QString &classToken : classTokens) {
                    if (classToken.size() >= 5 &&
                        (classToken.startsWith(token) || token.startsWith(classToken))) {
                        strongMatched = true;
                        break;
                    }
                }
                if (strongMatched) {
                    break;
                }
            }
        }
        if (!strongMatched) {
            continue;
        }

        // If the entry has strong identity tokens, treat that as sufficient.
        if (!strongTokens.isEmpty()) {
            return true;
        }

        for (const QString &token : weakTokens) {
            if (token.isEmpty()) {
                continue;
            }
            if (token.size() < 5 || isWeakIdentityToken(token)) {
                continue;
            }
            if (classTokens.contains(token)) {
                return true;
            }
            if (token.size() >= 5) {
                for (const QString &classToken : classTokens) {
                    if (classToken.size() >= 5 &&
                        (classToken.startsWith(token) || token.startsWith(classToken))) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void DockModel::setupProcessWatcher()
{
    const QStringList watched = m_procWatcher.directories();
    if (!watched.isEmpty()) {
        m_procWatcher.removePaths(watched);
    }

    const QString procPath = QStringLiteral("/proc");
    const QFileInfo procInfo(procPath);
    if (procInfo.exists() && procInfo.isDir()) {
        m_procWatcher.addPath(procPath);
    }
}

void DockModel::scheduleRunningRefresh()
{
    m_runningWatchDebounce.start();
}

void DockModel::refreshRunningStates()
{
    m_verboseLogging = m_localSettings.value(QStringLiteral("debug/verboseLogging"), false).toBool();
    setupProcessWatcher();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_desktopPathLaunchHints.begin(); it != m_desktopPathLaunchHints.end();) {
        if (it.value() < nowMs) {
            it = m_desktopPathLaunchHints.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_pendingTransientExpiryByDesktop.begin(); it != m_pendingTransientExpiryByDesktop.end();) {
        if (it.value() < nowMs) {
            m_pendingTransientByDesktop.remove(it.key());
            it = m_pendingTransientExpiryByDesktop.erase(it);
        } else {
            ++it;
        }
    }

    QSet<QString> runningExecutables;
    QSet<qint64> runningPids;
    const QDir procDir(QStringLiteral("/proc"));
    const QStringList pids = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    const QRegularExpression digits(QStringLiteral("^\\d+$"));

    for (const QString &pid : pids) {
        if (!digits.match(pid).hasMatch()) {
            continue;
        }
        runningPids.insert(pid.toLongLong());

        const QString commPath = QStringLiteral("/proc/%1/comm").arg(pid);
        QFile commFile(commPath);
        if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString comm = QString::fromUtf8(commFile.readAll()).trimmed().toLower();
            if (!comm.isEmpty()) {
                addRunningCandidates(runningExecutables, comm);
                runningExecutables.insert(normalizeToken(comm));
            }
        }

        const QString cmdlinePath = QStringLiteral("/proc/%1/cmdline").arg(pid);
        QFile cmdFile(cmdlinePath);
        if (cmdFile.open(QIODevice::ReadOnly)) {
            const QByteArray raw = cmdFile.readAll();
            if (!raw.isEmpty()) {
                const QList<QByteArray> args = raw.split('\0');
                for (const QByteArray &argRaw : args) {
                    if (argRaw.isEmpty()) {
                        continue;
                    }
                    const QString arg = QString::fromUtf8(argRaw).trimmed().toLower();
                    if (arg.isEmpty()) {
                        continue;
                    }
                    const QString exe = QFileInfo(arg).fileName().trimmed().toLower();
                    if (!exe.isEmpty()) {
                        addRunningCandidates(runningExecutables, exe);
                        runningExecutables.insert(normalizeToken(exe));
                    }
                    runningExecutables.insert(normalizeToken(arg));
                }
            }
        }
    }

    QStringList wmctrlLines;
    const QString wmctrlPath = QStandardPaths::findExecutable(QStringLiteral("wmctrl"));
    if (!wmctrlPath.isEmpty()) {
        QProcess listProc;
        listProc.start(wmctrlPath, QStringList{QStringLiteral("-lx")});
        if (listProc.waitForFinished(220) &&
            listProc.exitStatus() == QProcess::NormalExit &&
            listProc.exitCode() == 0) {
            wmctrlLines = QString::fromUtf8(listProc.readAllStandardOutput())
                              .split('\n', Qt::SkipEmptyParts);
        } else {
            listProc.kill();
        }
    }
    const bool hasWindowSnapshot = !wmctrlLines.isEmpty();
    m_runtimeRegistry.refresh(runningPids);

    QVector<DockEntry> pinnedEntries;
    pinnedEntries.reserve(m_entries.size());
    for (const DockEntry &entry : m_entries) {
        if (entry.isPinned) {
            pinnedEntries.push_back(entry);
        }
    }

    QSet<QString> pinnedExecutableNames;
    for (DockEntry &entry : pinnedEntries) {
        const bool previousRunning = entry.isRunning;
        const bool processRunning = isEntryRunning(entry, runningExecutables);
        const bool windowRunning = hasWindowForEntry(entry, wmctrlLines);
        const QSet<QString> runtimeTokens = entryRuntimeTokens(entry);
        const QString entryDesktopPath = QFileInfo(entry.desktopFile).absoluteFilePath();
        if (windowRunning) {
            m_runtimeRegistry.noteObservedRunning(runtimeTokens);
        }
        const bool userLaunched = m_runtimeRegistry.hasActiveUserLaunch(launchHintTokens(runtimeTokens));
        const bool desktopHint = !entryDesktopPath.isEmpty() &&
                                 m_desktopPathLaunchHints.contains(entryDesktopPath);
        entry.isRunning = windowRunning || userLaunched || desktopHint || (processRunning && userLaunched);
        if (!entryDesktopPath.isEmpty() && (windowRunning || processRunning)) {
            // Startup bridge no longer needed once app is actually observed running.
            m_desktopPathLaunchHints.remove(entryDesktopPath);
            m_pendingTransientByDesktop.remove(entryDesktopPath);
            m_pendingTransientExpiryByDesktop.remove(entryDesktopPath);
        }
        if (previousRunning != entry.isRunning) {
            if (m_verboseLogging) {
                qInfo().noquote() << "DockRunningChange:"
                                  << entry.name
                                  << "running=" << entry.isRunning
                                  << "window=" << windowRunning
                                  << "process=" << processRunning
                                  << "userHint=" << userLaunched
                                  << "wmctrlSnapshot=" << hasWindowSnapshot
                                  << "desktop=" << entry.desktopFile
                                  << "exec=" << entry.executable;
            }
        }
        auto candidates = executableCandidates(entry.executable);
        candidates.unite(entryRuntimeTokens(entry));
        for (const QString &candidate : candidates) {
            if (!candidate.isEmpty() && !isGenericWrapperToken(candidate)) {
                pinnedExecutableNames.insert(candidate);
            }
        }
    }

    QVector<DockEntry> transientEntries;
    QSet<QString> transientExecutableNames;
    QVector<DockEntry> existingTransientEntries;
    existingTransientEntries.reserve(m_entries.size());
    for (const DockEntry &entry : m_entries) {
        if (!entry.isPinned) {
            existingTransientEntries.push_back(entry);
        }
    }

    GList *infos = g_app_info_get_all();
    for (GList *it = infos; it != nullptr; it = it->next) {
        GAppInfo *info = G_APP_INFO(it->data);
        if (!info || !g_app_info_should_show(info)) {
            continue;
        }

        const QString executable = fromUtf8(g_app_info_get_executable(info));
        const QString normalizedExec = normalizeExecutableName(executable);
        if (normalizedExec.isEmpty()) {
            continue;
        }
        DockEntry probeEntry;
        probeEntry.name = fromUtf8(g_app_info_get_display_name(info));
        probeEntry.executable = executable;
        if (G_IS_DESKTOP_APP_INFO(info)) {
            probeEntry.desktopFile = fromUtf8(g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(info)));
        }

        const bool appWindowRunning = hasWindowForEntry(probeEntry, wmctrlLines);
        bool appProcessRunning = containsAnyCandidate(runningExecutables, executable);
        if (!appProcessRunning) {
            const QSet<QString> rt = entryRuntimeTokens(probeEntry);
            for (const QString &token : rt) {
                if (!token.isEmpty() && runningExecutables.contains(token)) {
                    appProcessRunning = true;
                    break;
                }
            }
        }
        const QSet<QString> probeTokens = entryRuntimeTokens(probeEntry);
        const QString probeDesktopPath = QFileInfo(probeEntry.desktopFile).absoluteFilePath();
        if (appWindowRunning) {
            m_runtimeRegistry.noteObservedRunning(probeTokens);
        }
        const bool userLaunched = m_runtimeRegistry.hasActiveUserLaunch(launchHintTokens(probeTokens));
        const bool desktopHint = !probeDesktopPath.isEmpty() &&
                                 m_desktopPathLaunchHints.contains(probeDesktopPath);
        const bool runningNow = appWindowRunning || userLaunched || desktopHint ||
                                (appProcessRunning && userLaunched);
        if (!probeDesktopPath.isEmpty() && (appWindowRunning || appProcessRunning)) {
            m_desktopPathLaunchHints.remove(probeDesktopPath);
            m_pendingTransientByDesktop.remove(probeDesktopPath);
            m_pendingTransientExpiryByDesktop.remove(probeDesktopPath);
        }
        if (!runningNow) {
            continue;
        }

        bool collidesWithPinned = false;
        bool collidesWithTransient = false;
        auto execCandidates = executableCandidates(executable);
        execCandidates.unite(entryRuntimeTokens(probeEntry));
        for (const QString &candidate : execCandidates) {
            if (!candidate.isEmpty() && !isGenericWrapperToken(candidate) &&
                pinnedExecutableNames.contains(candidate)) {
                collidesWithPinned = true;
                break;
            }
        }
        for (const QString &candidate : execCandidates) {
            if (!candidate.isEmpty() && !isGenericWrapperToken(candidate) &&
                transientExecutableNames.contains(candidate)) {
                collidesWithTransient = true;
                break;
            }
        }

        if (collidesWithPinned || collidesWithTransient) {
            continue;
        }

        DockEntry entry;
        entry.name = probeEntry.name;
        if (entry.name.trimmed().isEmpty()) {
            entry.name = normalizedExec;
        }
        entry.executable = executable;
        entry.isRunning = runningNow;
        entry.isPinned = false;

        if (G_IS_DESKTOP_APP_INFO(info) && entry.desktopFile.isEmpty()) {
            entry.desktopFile = probeEntry.desktopFile;
            const gchar *iconKey = g_desktop_app_info_get_string(G_DESKTOP_APP_INFO(info), "Icon");
            if (iconKey) {
                entry.iconName = fromUtf8(iconKey);
            }
        }

        GIcon *icon = g_app_info_get_icon(info);
        if (entry.iconName.isEmpty()) {
            entry.iconName = iconNameFromGIcon(icon);
        }
        entry.iconSource = iconPathFromGIcon(icon);

        transientEntries.push_back(entry);
        for (const QString &candidate : execCandidates) {
            if (!candidate.isEmpty() && !isGenericWrapperToken(candidate)) {
                transientExecutableNames.insert(candidate);
            }
        }
    }
    g_list_free_full(infos, g_object_unref);

    // Fallback: some apps launched from local shortcuts are not returned by g_app_info_get_all().
    // Use wmctrl window classes directly so they still appear as running transient apps.
    for (const QString &line : wmctrlLines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const QStringList fields =
            trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() < 4) {
            continue;
        }
        const QString wmClassRaw = fields.at(3).trimmed().toLower();
        if (wmClassRaw.isEmpty()) {
            continue;
        }
        if (wmClassRaw.contains(QStringLiteral("desktop_shell")) ||
            wmClassRaw.contains(QStringLiteral("plasmashell")) ||
            wmClassRaw.contains(QStringLiteral("gnome-shell"))) {
            continue;
        }

        const QStringList classParts = wmClassRaw.split('.', Qt::SkipEmptyParts);
        const QString wmToken = normalizeToken(classParts.isEmpty() ? wmClassRaw : classParts.last());
        if (wmToken.isEmpty()) {
            continue;
        }
        if (pinnedExecutableNames.contains(wmToken) || transientExecutableNames.contains(wmToken)) {
            continue;
        }

        DockEntry entry;
        entry.name = prettifyToken(classParts.isEmpty() ? wmClassRaw : classParts.last());
        entry.executable = wmToken;
        entry.isRunning = true;
        entry.isPinned = false;
        entry.iconName = wmToken;
        transientEntries.push_back(entry);
        transientExecutableNames.insert(wmToken);
    }

    // Preserve transient entries created from explicit user launch hints (e.g. shell shortcuts)
    // even when desktop database/window snapshots lag behind.
    for (const DockEntry &existing : existingTransientEntries) {
        const QSet<QString> tokens = entryRuntimeTokens(existing);
        const bool keepByHint = m_runtimeRegistry.hasActiveUserLaunch(launchHintTokens(tokens));
        const QString existingDesktopPath = QFileInfo(existing.desktopFile).absoluteFilePath();
        const bool keepByDesktopHint = !existingDesktopPath.isEmpty() &&
                                       m_desktopPathLaunchHints.contains(existingDesktopPath);
        const bool keepByWindow = hasWindowForEntry(existing, wmctrlLines);
        const bool keepByProcess = isEntryRunning(existing, runningExecutables);
        if (!keepByHint && !keepByDesktopHint && !keepByWindow && !keepByProcess) {
            continue;
        }

        auto candidates = executableCandidates(existing.executable);
        candidates.unite(tokens);
        bool collides = false;
        for (const QString &candidate : candidates) {
            if (candidate.isEmpty() || isGenericWrapperToken(candidate)) {
                continue;
            }
            if (pinnedExecutableNames.contains(candidate) || transientExecutableNames.contains(candidate)) {
                collides = true;
                break;
            }
        }
        if (collides) {
            continue;
        }

        DockEntry entry = existing;
        entry.isPinned = false;
        entry.isRunning = true;
        transientEntries.push_back(entry);
        for (const QString &candidate : candidates) {
            if (!candidate.isEmpty() && !isGenericWrapperToken(candidate)) {
                transientExecutableNames.insert(candidate);
            }
        }
    }

    // Force-merge pending user-launched transient entries (stable path-based hint),
    // so they survive model rebuilds during app startup.
    for (auto it = m_pendingTransientByDesktop.constBegin(); it != m_pendingTransientByDesktop.constEnd(); ++it) {
        const QString desktopPath = it.key();
        const DockEntry pending = it.value();
        if (desktopPath.isEmpty()) {
            continue;
        }
        const bool hasPathHint = m_desktopPathLaunchHints.contains(desktopPath);
        const bool hasPendingTtl = m_pendingTransientExpiryByDesktop.contains(desktopPath);
        if (!hasPathHint && !hasPendingTtl) {
            continue;
        }

        bool existsAlready = false;
        for (const DockEntry &e : transientEntries) {
            if (QFileInfo(e.desktopFile).absoluteFilePath() == desktopPath) {
                existsAlready = true;
                break;
            }
        }
        if (existsAlready) {
            continue;
        }

        DockEntry add = pending;
        add.isPinned = false;
        add.isRunning = true;
        transientEntries.push_back(add);
    }

    std::sort(transientEntries.begin(), transientEntries.end(),
              [](const DockEntry &a, const DockEntry &b) {
                  return QString::localeAwareCompare(a.name, b.name) < 0;
              });

    QVector<DockEntry> nextEntries;
    nextEntries.reserve(pinnedEntries.size() + transientEntries.size());
    nextEntries += pinnedEntries;
    nextEntries += transientEntries;

    auto transientKey = [](const DockEntry &entry) {
        return QFileInfo(entry.desktopFile).absoluteFilePath() + QStringLiteral("|") +
               normalizeToken(entry.executable) + QStringLiteral("|") + normalizeToken(entry.name);
    };
    QSet<QString> prevTransientSet;
    QSet<QString> nextTransientSet;
    for (const DockEntry &entry : m_entries) {
        if (!entry.isPinned) {
            prevTransientSet.insert(transientKey(entry));
        }
    }
    for (const DockEntry &entry : nextEntries) {
        if (!entry.isPinned) {
            nextTransientSet.insert(transientKey(entry));
        }
    }
    for (const QString &k : nextTransientSet) {
        if (!prevTransientSet.contains(k)) {
            if (m_verboseLogging) {
                qInfo().noquote() << "DockTransientChange: added" << k;
            }
        }
    }
    for (const QString &k : prevTransientSet) {
        if (!nextTransientSet.contains(k)) {
            if (m_verboseLogging) {
                qInfo().noquote() << "DockTransientChange: removed" << k;
            }
        }
    }

    auto entriesEqual = [](const QVector<DockEntry> &a, const QVector<DockEntry> &b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (int i = 0; i < a.size(); ++i) {
            const DockEntry &lhs = a.at(i);
            const DockEntry &rhs = b.at(i);
            if (lhs.name != rhs.name ||
                lhs.iconSource != rhs.iconSource ||
                lhs.iconName != rhs.iconName ||
                lhs.desktopFile != rhs.desktopFile ||
                lhs.executable != rhs.executable ||
                lhs.isRunning != rhs.isRunning ||
                lhs.isPinned != rhs.isPinned) {
                return false;
            }
        }
        return true;
    };

    if (entriesEqual(m_entries, nextEntries)) {
        return;
    }

    beginResetModel();
    m_entries = std::move(nextEntries);
    endResetModel();
}
