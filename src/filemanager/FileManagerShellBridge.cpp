#include "FileManagerShellBridge.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QDBusReply>
#include <QTimer>
#include <QUuid>
#include <QDir>
#include <QStandardPaths>
#include <QFuture>
#include <QtConcurrent>

#include "src/apps/filemanager/include/filemanagerapi.h"

Q_LOGGING_CATEGORY(logFmBridge, "slm.desktop.fmbridge")

// ── DBus constants ────────────────────────────────────────────────────────────

static constexpr const char kFmService[]    = "org.slm.Desktop.FileManager1";
static constexpr const char kFmPath[]       = "/org/slm/Desktop/FileManager1";
static constexpr const char kFmIface[]      = "org.slm.Desktop.FileManager1";

static constexpr const char kFileOpsService[] = "org.slm.Desktop.FileOperations";
static constexpr const char kFileOpsPath[]    = "/org/slm/Desktop/FileOperations";
static constexpr const char kFileOpsIface[]   = "org.slm.Desktop.FileOperations";

// ── Helpers ───────────────────────────────────────────────────────────────────

QString FileManagerShellBridge::newReqId()
{
    return QUuid::createUuid().toString(QUuid::Id128).left(8);
}

// Hitung item di trash secara lokal (GIO trash:// files dir)
static int countTrashItems()
{
    const QString trashFiles = QDir::homePath()
        + QStringLiteral("/.local/share/Trash/files");
    const QDir dir(trashFiles);
    if (!dir.exists()) return 0;
    return static_cast<int>(
        dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count());
}

// Bangun sidebar places dari bookmarks + storage locations
static QVariantList buildSidebarPlaces(FileManagerApi *api)
{
    QVariantList places;
    if (!api) return places;

    // Bookmarks
    const auto bookmarks = api->bookmarks(500);
    for (const auto &bm : bookmarks) {
        auto entry = bm.toMap();
        entry[QStringLiteral("section")] = QStringLiteral("bookmarks");
        entry[QStringLiteral("ejectable")] = false;
        places.append(entry);
    }

    // Storage locations (drives, network, etc.)
    const auto locations = api->storageLocations();
    for (const auto &loc : locations) {
        auto entry = loc.toMap();
        entry[QStringLiteral("section")] = QStringLiteral("devices");
        places.append(entry);
    }

    return places;
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

FileManagerShellBridge::FileManagerShellBridge(FileManagerApi *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
    Q_ASSERT(api);

    connectFmDbus();
    connectFileOpsDbus();
    startTrashWatch();

    // Sidebar: storage locations changed
    connect(m_api, &FileManagerApi::storageLocationsUpdated,
            this, [this](const QVariantList &) {
        qCDebug(logFmBridge) << "storageLocationsUpdated → sidebarPlacesChanged";
        emit sidebarPlacesChanged();
    });

    // Mount/unmount events → sidebar + signals
    connect(m_api, &FileManagerApi::storageMountFinished,
            this, [this](const QString &devicePath, bool ok,
                         const QString &mountedPath, const QString &) {
        if (ok) {
            emit mountAdded(mountedPath, {}, {}, false);
            emit sidebarPlacesChanged();
        }
        Q_UNUSED(devicePath)
    });
    connect(m_api, &FileManagerApi::storageUnmountFinished,
            this, [this](const QString &devicePath, bool ok, const QString &) {
        if (ok) {
            emit mountRemoved(devicePath);
            emit sidebarPlacesChanged();
        }
    });
}

FileManagerShellBridge::~FileManagerShellBridge()
{
    delete m_fileOpsIface;
    delete m_fmIface;
}

// ── DBus setup ────────────────────────────────────────────────────────────────

void FileManagerShellBridge::connectFmDbus()
{
    auto *watcher = new QDBusServiceWatcher(
        QString::fromLatin1(kFmService),
        QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForOwnerChange,
        this);

    connect(watcher, &QDBusServiceWatcher::serviceRegistered,
            this, &FileManagerShellBridge::onFmServiceAppeared);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &FileManagerShellBridge::onFmServiceVanished);

    if (QDBusConnection::sessionBus().interface()
            ->isServiceRegistered(QString::fromLatin1(kFmService)))
    {
        onFmServiceAppeared(QString::fromLatin1(kFmService));
    }
}

void FileManagerShellBridge::connectFileOpsDbus()
{
    auto *watcher = new QDBusServiceWatcher(
        QString::fromLatin1(kFileOpsService),
        QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForOwnerChange,
        this);

    connect(watcher, &QDBusServiceWatcher::serviceRegistered,
            this, &FileManagerShellBridge::onFileOpsServiceAppeared);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &FileManagerShellBridge::onFileOpsServiceVanished);

    if (QDBusConnection::sessionBus().interface()
            ->isServiceRegistered(QString::fromLatin1(kFileOpsService)))
    {
        onFileOpsServiceAppeared(QString::fromLatin1(kFileOpsService));
    }
}

void FileManagerShellBridge::onFmServiceAppeared(const QString &service)
{
    qCDebug(logFmBridge) << "FileManager service appeared:" << service;
    delete m_fmIface;
    m_fmIface = new QDBusInterface(
        service,
        QString::fromLatin1(kFmPath),
        QString::fromLatin1(kFmIface),
        QDBusConnection::sessionBus(),
        this);
    m_fmAvailable = m_fmIface->isValid();
    emit fileManagerAvailableChanged();
}

void FileManagerShellBridge::onFmServiceVanished(const QString &)
{
    qCDebug(logFmBridge) << "FileManager service vanished";
    delete m_fmIface;
    m_fmIface = nullptr;
    m_fmAvailable = false;
    emit fileManagerAvailableChanged();
}

void FileManagerShellBridge::onFileOpsServiceAppeared(const QString &service)
{
    delete m_fileOpsIface;
    m_fileOpsIface = new QDBusInterface(
        service,
        QString::fromLatin1(kFileOpsPath),
        QString::fromLatin1(kFileOpsIface),
        QDBusConnection::sessionBus(),
        this);

    if (m_fileOpsIface->isValid()) {
        QDBusConnection::sessionBus().connect(
            service, QString::fromLatin1(kFileOpsPath),
            QString::fromLatin1(kFileOpsIface), QStringLiteral("JobsChanged"),
            this, SLOT(onFileOpsJobsChanged()));
        QDBusConnection::sessionBus().connect(
            service, QString::fromLatin1(kFileOpsPath),
            QString::fromLatin1(kFileOpsIface), QStringLiteral("Progress"),
            this, SLOT(onFileOpsProgress(QString,int)));
        QDBusConnection::sessionBus().connect(
            service, QString::fromLatin1(kFileOpsPath),
            QString::fromLatin1(kFileOpsIface), QStringLiteral("ProgressDetail"),
            this, SLOT(onFileOpsProgressDetail(QString,qulonglong,qulonglong)));
        QDBusConnection::sessionBus().connect(
            service, QString::fromLatin1(kFileOpsPath),
            QString::fromLatin1(kFileOpsIface), QStringLiteral("Finished"),
            this, SLOT(onFileOpsFinished(QString)));
        QDBusConnection::sessionBus().connect(
            service, QString::fromLatin1(kFileOpsPath),
            QString::fromLatin1(kFileOpsIface), QStringLiteral("Error"),
            this, SLOT(onFileOpsError(QString)));
        m_fileOpsAvailable = true;
    } else {
        m_fileOpsAvailable = false;
    }
    emit fileOpsAvailableChanged();
}

void FileManagerShellBridge::onFileOpsServiceVanished(const QString &)
{
    qCDebug(logFmBridge) << "FileOps service vanished";
    delete m_fileOpsIface;
    m_fileOpsIface = nullptr;
    m_fileOpsAvailable = false;
    emit fileOpsAvailableChanged();
}

// ── Trash watch ───────────────────────────────────────────────────────────────

void FileManagerShellBridge::startTrashWatch()
{
    auto *timer = new QTimer(this);
    timer->setInterval(5000);
    connect(timer, &QTimer::timeout, this, &FileManagerShellBridge::onTrashCountRefresh);
    timer->start();
    onTrashCountRefresh();
}

void FileManagerShellBridge::onTrashCountRefresh()
{
    const int count = countTrashItems();
    if (count != m_trashItemCount) {
        m_trashItemCount = count;
        emit trashCountChanged(count);
    }
}

// ── Properties ────────────────────────────────────────────────────────────────

int FileManagerShellBridge::trashItemCount() const    { return m_trashItemCount; }
bool FileManagerShellBridge::fileManagerAvailable() const { return m_fmAvailable; }
bool FileManagerShellBridge::fileOpsAvailable() const  { return m_fileOpsAvailable; }

// ── In-process queries ────────────────────────────────────────────────────────

QVariantList FileManagerShellBridge::sidebarPlaces() const
{
    return buildSidebarPlaces(m_api);
}

// ── Navigation (cross-process via DBus) ──────────────────────────────────────

void FileManagerShellBridge::openPath(const QString &path)
{
    const QString reqId = newReqId();
    qCDebug(logFmBridge) << "[req:" << reqId << "] openPath →" << path;

    // Jika filemanager ada di sesi yang sama (in-process app), pakai API langsung
    // Jika filemanager app terpisah, gunakan DBus activation
    QDBusInterface iface(QString::fromLatin1(kFmService),
                         QString::fromLatin1(kFmPath),
                         QString::fromLatin1(kFmIface),
                         QDBusConnection::sessionBus());
    const auto reply = iface.call(QStringLiteral("OpenPath"), path, reqId);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(logFmBridge) << "[req:" << reqId << "] openPath error (fallback to API):"
                               << reply.errorMessage();
        // Fallback: apabila filemanager berjalan in-process dengan shell
        if (m_api)
            m_api->startOpenPathInFileManager(path, reqId);
    }
}

void FileManagerShellBridge::revealItem(const QString &path)
{
    const QString reqId = newReqId();
    qCDebug(logFmBridge) << "[req:" << reqId << "] revealItem →" << path;

    QDBusInterface iface(QString::fromLatin1(kFmService),
                         QString::fromLatin1(kFmPath),
                         QString::fromLatin1(kFmIface),
                         QDBusConnection::sessionBus());
    const auto reply = iface.call(QStringLiteral("RevealItem"), path, reqId);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(logFmBridge) << "[req:" << reqId << "] revealItem error:" << reply.errorMessage();
        // Fallback in-process
        if (m_api) m_api->startOpenPathInFileManager(path, reqId);
    }
}

void FileManagerShellBridge::newWindow(const QString &path)
{
    const QString reqId = newReqId();
    qCDebug(logFmBridge) << "[req:" << reqId << "] newWindow path=" << path;
    QDBusInterface iface(QString::fromLatin1(kFmService),
                         QString::fromLatin1(kFmPath),
                         QString::fromLatin1(kFmIface),
                         QDBusConnection::sessionBus());
    iface.call(QStringLiteral("NewWindow"), path);
}

// ── Volume management (via FileManagerApi GIO bindings) ───────────────────────

void FileManagerShellBridge::mountVolume(const QString &deviceUri)
{
    if (!m_api) return;
    qCDebug(logFmBridge) << "mountVolume:" << deviceUri;
    m_api->startMountStorageDevice(deviceUri);
}

void FileManagerShellBridge::unmountVolume(const QString &mountPath)
{
    if (!m_api) return;
    qCDebug(logFmBridge) << "unmountVolume:" << mountPath;
    m_api->startUnmountStorageDevice(mountPath);
}

void FileManagerShellBridge::ejectVolume(const QString &deviceUri)
{
    // Eject = unmount + hardware eject; GIO startUnmountStorageDevice handles both
    if (!m_api) return;
    qCDebug(logFmBridge) << "ejectVolume:" << deviceUri;
    m_api->startUnmountStorageDevice(deviceUri);
}

// ── Trash ─────────────────────────────────────────────────────────────────────

void FileManagerShellBridge::emptyTrash()
{
    const QString reqId = newReqId();
    qCDebug(logFmBridge) << "[req:" << reqId << "] emptyTrash";

    // Prefer fileopsd via DBus (crash-isolated)
    if (m_fileOpsIface && m_fileOpsIface->isValid()) {
        const auto reply = m_fileOpsIface->call(QStringLiteral("EmptyTrash"));
        if (reply.type() != QDBusMessage::ErrorMessage) return;
        qCWarning(logFmBridge) << "[req:" << reqId << "] emptyTrash via fileopsd failed:"
                               << reply.errorMessage();
    }

    // Fallback: FileManagerApi in-process
    if (m_api) {
        qCDebug(logFmBridge) << "[req:" << reqId << "] emptyTrash fallback to in-process";
        m_api->startEmptyTrash();
    }
}

// ── File operations (via fileopsd DBus) ───────────────────────────────────────

QString FileManagerShellBridge::startCopy(const QStringList &sources,
                                           const QString &dest)
{
    const QString reqId = newReqId();
    qCDebug(logFmBridge) << "[req:" << reqId << "] startCopy" << sources.count() << "→" << dest;

    const auto startLocalFallback = [this, reqId, sources, dest]() -> QString {
        if (!m_api) {
            return {};
        }
        const QString localOpId = QStringLiteral("local-copy-%1").arg(reqId);
        QTimer::singleShot(0, this, [this, localOpId, sources, dest]() {
            QVariantList src;
            src.reserve(sources.size());
            for (const QString &path : sources) {
                src.push_back(path);
            }
            const QVariantMap out = m_api->copyPaths(src, dest, false);
            const bool ok = out.value(QStringLiteral("ok")).toBool();
            const QString err = out.value(QStringLiteral("error")).toString();
            emit operationFinished(localOpId, ok, err);
        });
        return localOpId;
    };

    if (!m_fileOpsIface || !m_fileOpsIface->isValid()) {
        qCWarning(logFmBridge) << "[req:" << reqId << "] startCopy: fileopsd tidak tersedia";
        return startLocalFallback();
    }
    QDBusReply<QString> reply = m_fileOpsIface->call(
        QStringLiteral("Copy"), sources, dest);
    if (!reply.isValid()) {
        qCWarning(logFmBridge) << "[req:" << reqId << "] startCopy error:" << reply.error().message();
        return startLocalFallback();
    }
    const QString opId = reply.value().trimmed();
    if (opId.isEmpty()) {
        return startLocalFallback();
    }
    return opId;
}

QString FileManagerShellBridge::startMove(const QStringList &sources,
                                           const QString &dest)
{
    const QString reqId = newReqId();
    qCDebug(logFmBridge) << "[req:" << reqId << "] startMove" << sources.count() << "→" << dest;
    if (!m_fileOpsIface || !m_fileOpsIface->isValid()) return {};
    QDBusReply<QString> reply = m_fileOpsIface->call(QStringLiteral("Move"), sources, dest);
    return reply.isValid() ? reply.value() : QString();
}

QString FileManagerShellBridge::startDelete(const QStringList &paths)
{
    const QString reqId = newReqId();
    qCDebug(logFmBridge) << "[req:" << reqId << "] startDelete" << paths.count();
    if (!m_fileOpsIface || !m_fileOpsIface->isValid()) return {};
    QDBusReply<QString> reply = m_fileOpsIface->call(QStringLiteral("Delete"), paths);
    return reply.isValid() ? reply.value() : QString();
}

QString FileManagerShellBridge::trashFiles(const QStringList &paths)
{
    const QString reqId = newReqId();
    qCDebug(logFmBridge) << "[req:" << reqId << "] trashFiles" << paths.count();
    if (!m_fileOpsIface || !m_fileOpsIface->isValid()) return {};
    QDBusReply<QString> reply = m_fileOpsIface->call(QStringLiteral("Trash"), paths);
    return reply.isValid() ? reply.value() : QString();
}

void FileManagerShellBridge::cancelOperation(const QString &operationId)
{
    if (operationId.isEmpty()) return;
    qCDebug(logFmBridge) << "cancelOperation:" << operationId;
    if (!m_fileOpsIface || !m_fileOpsIface->isValid()) return;
    m_fileOpsIface->call(QStringLiteral("Cancel"), operationId);
}

// ── Search (in-process, incremental, non-blocking) ────────────────────────────

void FileManagerShellBridge::searchFiles(const QString &rootPath,
                                          const QString &query,
                                          const QString &sessionId)
{
    if (!m_api) return;
    if (m_activeSearchSessions.contains(sessionId)) {
        qCDebug(logFmBridge) << "searchFiles: session" << sessionId << "sudah berjalan";
        return;
    }
    qCDebug(logFmBridge) << "searchFiles [" << sessionId << "]" << query << "in" << rootPath;
    // Alokasikan search session token dari FileManagerApi
    const qulonglong token = m_api->beginSearchSession();

    // Jalankan di thread pool agar tidak block UI; simpan future agar return value tidak di-ignore
    m_activeSearchSessions[sessionId] = QtConcurrent::run([this, rootPath, query, sessionId, token]() {
        const auto result = m_api->searchDirectory(
            rootPath, query, /*includeHidden=*/false,
            /*dirFirst=*/true, /*maxResults=*/200, token);
        const QVariantList entries = result.value(QStringLiteral("entries")).toList();

        QMetaObject::invokeMethod(this, [this, sessionId, entries]() {
            if (!m_activeSearchSessions.contains(sessionId)) return; // cancelled
            emit searchResultsAvailable(sessionId, entries);
            m_activeSearchSessions.remove(sessionId);
            emit searchFinished(sessionId);
        }, Qt::QueuedConnection);
    });
}

void FileManagerShellBridge::cancelSearch(const QString &sessionId)
{
    qCDebug(logFmBridge) << "cancelSearch [" << sessionId << "]";
    // Hapus dari active sessions — lambda di searchFiles akan cek ini
    m_activeSearchSessions.remove(sessionId);
}

// ── FileOps signal handlers ───────────────────────────────────────────────────

void FileManagerShellBridge::onFileOpsJobsChanged()
{
    onTrashCountRefresh();
}

void FileManagerShellBridge::onFileOpsProgress(const QString &id, int percent)
{
    emit operationProgress(id, percent / 100.0,
                           QStringLiteral("%1%").arg(percent));
}

void FileManagerShellBridge::onFileOpsProgressDetail(const QString &id,
                                                       qulonglong current,
                                                       qulonglong total)
{
    const double fraction = (total > 0) ? static_cast<double>(current) / total : 0.0;
    emit operationProgress(id, fraction,
                           QStringLiteral("%1 / %2").arg(current).arg(total));
}

void FileManagerShellBridge::onFileOpsFinished(const QString &id)
{
    qCDebug(logFmBridge) << "operation finished:" << id;
    emit operationFinished(id, true, {});
    QTimer::singleShot(500, this, &FileManagerShellBridge::onTrashCountRefresh);
}

void FileManagerShellBridge::onFileOpsError(const QString &id)
{
    qCDebug(logFmBridge) << "operation error:" << id;
    QString errMsg;
    if (m_fileOpsIface && m_fileOpsIface->isValid()) {
        const auto reply = m_fileOpsIface->call(QStringLiteral("GetJob"), id);
        if (reply.type() == QDBusMessage::ReplyMessage)
            errMsg = reply.arguments().first().toMap()
                .value(QStringLiteral("error")).toString();
    }
    emit operationFinished(id, false, errMsg);
}
