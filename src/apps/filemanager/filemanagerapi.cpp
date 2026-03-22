#include "filemanagerapi.h"
#include "src/apps/filemanager/ops/fileoperationserrors.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QMimeDatabase>

namespace {

constexpr const char kFileOpsService[] = "org.slm.Desktop.FileOperations";
constexpr const char kFileOpsPath[] = "/org/slm/Desktop/FileOperations";
constexpr const char kFileOpsIface[] = "org.slm.Desktop.FileOperations";

} // namespace

FileManagerApi::FileManagerApi(QObject *parent)
    : QObject(parent)
{
    m_slmContextMenuController = new SlmContextMenuController(this, this);
    m_batchTimer.setSingleShot(false);
    m_batchTimer.setInterval(0);
    connect(&m_batchTimer, &QTimer::timeout, this, &FileManagerApi::processBatchStep);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        emit directoryChanged(path);
        emit pathChanged(path, QStringLiteral("directory"));
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        emit fileChanged(path);
        emit pathChanged(path, QStringLiteral("file"));
    });

    QDBusConnection::sessionBus().connect(QString::fromLatin1(kFileOpsService),
                                          QString::fromLatin1(kFileOpsPath),
                                          QString::fromLatin1(kFileOpsIface),
                                          QStringLiteral("JobsChanged"),
                                          this,
                                          SLOT(onDaemonFileOpsJobsChanged()));
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kFileOpsService),
                                          QString::fromLatin1(kFileOpsPath),
                                          QString::fromLatin1(kFileOpsIface),
                                          QStringLiteral("Progress"),
                                          this,
                                          SLOT(onDaemonFileOpProgress(QString,int)));
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kFileOpsService),
                                          QString::fromLatin1(kFileOpsPath),
                                          QString::fromLatin1(kFileOpsIface),
                                          QStringLiteral("ProgressDetail"),
                                          this,
                                          SLOT(onDaemonFileOpProgressDetail(QString,int,int)));
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kFileOpsService),
                                          QString::fromLatin1(kFileOpsPath),
                                          QString::fromLatin1(kFileOpsIface),
                                          QStringLiteral("Finished"),
                                          this,
                                          SLOT(onDaemonFileOpFinished(QString)));
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kFileOpsService),
                                          QString::fromLatin1(kFileOpsPath),
                                          QString::fromLatin1(kFileOpsIface),
                                          QStringLiteral("Error"),
                                          this,
                                          SLOT(onDaemonFileOpError(QString)));
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kFileOpsService),
                                          QString::fromLatin1(kFileOpsPath),
                                          QString::fromLatin1(kFileOpsIface),
                                          QStringLiteral("ErrorDetail"),
                                          this,
                                          SLOT(onDaemonFileOpErrorDetail(QString,QString,QString)));

    if (QDBusConnection::sessionBus().isConnected()) {
        auto *fileOpsWatcher = new QDBusServiceWatcher(QString::fromLatin1(kFileOpsService),
                                                       QDBusConnection::sessionBus(),
                                                       QDBusServiceWatcher::WatchForOwnerChange,
                                                       this);
        connect(fileOpsWatcher,
                &QDBusServiceWatcher::serviceOwnerChanged,
                this,
                [this](const QString &, const QString &, const QString &) {
                    QTimer::singleShot(0, this, &FileManagerApi::recoverDaemonFileOpState);
                });
    }
    QTimer::singleShot(0, this, &FileManagerApi::recoverDaemonFileOpState);
}

FileManagerApi::~FileManagerApi() = default;

QVariantMap FileManagerApi::makeResult(bool ok, const QString &error, const QVariantMap &extra)
{
    QVariantMap out = extra;
    out.insert(QStringLiteral("ok"), ok);
    if (!ok) {
        out.insert(QStringLiteral("error"), error.isEmpty() ? QStringLiteral("operation-failed") : error);
    }
    return out;
}

QString FileManagerApi::expandPath(const QString &path)
{
    QString out = path.trimmed();
    if (out == QStringLiteral("~")) {
        return QDir::homePath();
    }
    if (out.startsWith(QStringLiteral("~/"))) {
        return QDir::homePath() + out.mid(1);
    }
    return out;
}

QVariantMap FileManagerApi::fileInfoMap(const QFileInfo &info)
{
    QVariantMap row;
    row.insert(QStringLiteral("name"), info.fileName());
    row.insert(QStringLiteral("path"), info.absoluteFilePath());
    row.insert(QStringLiteral("isDir"), info.isDir());
    row.insert(QStringLiteral("isFile"), info.isFile());
    row.insert(QStringLiteral("isHidden"), info.isHidden());
    row.insert(QStringLiteral("size"), static_cast<qlonglong>(info.size()));
    row.insert(QStringLiteral("lastModified"), info.lastModified().toString(Qt::ISODateWithMs));
    if (info.isDir()) {
        row.insert(QStringLiteral("mimeType"), QStringLiteral("inode/directory"));
        row.insert(QStringLiteral("iconName"), QStringLiteral("folder"));
    } else {
        const QMimeType mime = QMimeDatabase().mimeTypeForFile(info.absoluteFilePath(), QMimeDatabase::MatchDefault);
        const QString mimeName = mime.name();
        const QString iconName = !mime.iconName().isEmpty()
                ? mime.iconName()
                : (!mime.genericIconName().isEmpty() ? mime.genericIconName()
                                                     : QStringLiteral("text-x-generic"));
        row.insert(QStringLiteral("mimeType"), mimeName);
        row.insert(QStringLiteral("iconName"), iconName);
    }
    row.insert(QStringLiteral("suffix"), info.suffix());
    row.insert(QStringLiteral("thumbnailPath"), QString());
    row.insert(QStringLiteral("ok"), true);
    return row;
}
