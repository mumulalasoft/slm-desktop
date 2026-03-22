#include "fileoperationsservice.h"

#include "../../../../dbuslogutils.h"
#include "fileoperationsmanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

namespace {
constexpr const char kService[] = "org.slm.Desktop.FileOperations";
constexpr const char kPath[] = "/org/slm/Desktop/FileOperations";
constexpr const char kApiVersion[] = "1.0";
}

FileOperationsService::FileOperationsService(FileOperationsManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    registerDbusService();
    if (m_manager) {
        connect(m_manager, &FileOperationsManager::JobsChanged,
                this, &FileOperationsService::JobsChanged);
        connect(m_manager, &FileOperationsManager::Progress,
                this, &FileOperationsService::Progress);
        connect(m_manager, &FileOperationsManager::ProgressDetail,
                this, &FileOperationsService::ProgressDetail);
        connect(m_manager, &FileOperationsManager::Finished,
                this, &FileOperationsService::Finished);
        connect(m_manager, &FileOperationsManager::Error,
                this, &FileOperationsService::Error);
        connect(m_manager, &FileOperationsManager::ErrorDetail,
                this, &FileOperationsService::ErrorDetail);
    }
}

FileOperationsService::~FileOperationsService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool FileOperationsService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString FileOperationsService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap FileOperationsService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_serviceRegistered},
        {QStringLiteral("api_version"), apiVersion()},
    };
}

QVariantMap FileOperationsService::GetCapabilities() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("capabilities"), QStringList{
             QStringLiteral("copy"),
             QStringLiteral("move"),
             QStringLiteral("delete"),
             QStringLiteral("trash"),
             QStringLiteral("empty_trash"),
             QStringLiteral("pause"),
             QStringLiteral("resume"),
             QStringLiteral("cancel"),
             QStringLiteral("get_job"),
             QStringLiteral("list_jobs"),
        }},
    };
}

void FileOperationsService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[desktopd] failed to open permission store for FileOperationsService";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);

    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.FileOperations"), QStringLiteral("Copy"), Slm::Permissions::Capability::FileActionInvoke);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.FileOperations"), QStringLiteral("Move"), Slm::Permissions::Capability::FileActionInvoke);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.FileOperations"), QStringLiteral("Delete"), Slm::Permissions::Capability::FileActionInvoke);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.FileOperations"), QStringLiteral("Trash"), Slm::Permissions::Capability::FileActionInvoke);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Desktop.FileOperations"), QStringLiteral("EmptyTrash"), Slm::Permissions::Capability::FileActionInvoke);
}

bool FileOperationsService::checkPermission(Slm::Permissions::Capability capability, const QString &methodName)
{
    if (qEnvironmentVariableIntValue("SLM_FILEOPS_BYPASS_PERMISSION") == 1) {
        return true;
    }

    if (!calledFromDBus()) {
        return true;
    }

    QVariantMap ctx;
    ctx.insert(QStringLiteral("method"), methodName);
    ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("High"));

    const Slm::Permissions::PolicyDecision d = m_securityGuard.check(message(), capability, ctx);
    if (!d.isAllowed()) {
        sendErrorReply(QStringLiteral("org.slm.PermissionDenied"), d.reason);
        return false;
    }
    return true;
}

QString FileOperationsService::Copy(const QStringList &uris, const QString &destination)
{
    if (!checkPermission(Slm::Permissions::Capability::FileActionInvoke, QStringLiteral("Copy"))) {
        return {};
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Copy"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("start"),
                     {{QStringLiteral("uri_count"), uris.size()},
                      {QStringLiteral("destination"), destination}});
    const QString jobId = m_manager ? m_manager->Copy(uris, destination) : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Copy"),
                     requestId,
                     caller,
                     jobId,
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), !jobId.trimmed().isEmpty()}});
    return jobId;
}

QString FileOperationsService::Move(const QStringList &uris, const QString &destination)
{
    if (!checkPermission(Slm::Permissions::Capability::FileActionInvoke, QStringLiteral("Move"))) {
        return {};
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Move"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("start"),
                     {{QStringLiteral("uri_count"), uris.size()},
                      {QStringLiteral("destination"), destination}});
    const QString jobId = m_manager ? m_manager->Move(uris, destination) : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Move"),
                     requestId,
                     caller,
                     jobId,
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), !jobId.trimmed().isEmpty()}});
    return jobId;
}

QString FileOperationsService::Delete(const QStringList &uris)
{
    if (!checkPermission(Slm::Permissions::Capability::FileActionInvoke, QStringLiteral("Delete"))) {
        return {};
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Delete"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("start"),
                     {{QStringLiteral("uri_count"), uris.size()}});
    const QString jobId = m_manager ? m_manager->Delete(uris) : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Delete"),
                     requestId,
                     caller,
                     jobId,
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), !jobId.trimmed().isEmpty()}});
    return jobId;
}

QString FileOperationsService::Trash(const QStringList &uris)
{
    if (!checkPermission(Slm::Permissions::Capability::FileActionInvoke, QStringLiteral("Trash"))) {
        return {};
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Trash"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("start"),
                     {{QStringLiteral("uri_count"), uris.size()}});
    const QString jobId = m_manager ? m_manager->Trash(uris) : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Trash"),
                     requestId,
                     caller,
                     jobId,
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), !jobId.trimmed().isEmpty()}});
    return jobId;
}

QString FileOperationsService::EmptyTrash()
{
    if (!checkPermission(Slm::Permissions::Capability::FileActionInvoke, QStringLiteral("EmptyTrash"))) {
        return {};
    }
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("EmptyTrash"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("start"));
    const QString jobId = m_manager ? m_manager->EmptyTrash() : QString();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("EmptyTrash"),
                     requestId,
                     caller,
                     jobId,
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), !jobId.trimmed().isEmpty()}});
    return jobId;
}

bool FileOperationsService::Pause(const QString &id)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_manager && m_manager->Pause(id);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Pause"),
                     requestId,
                     caller,
                     id,
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), ok}});
    return ok;
}

bool FileOperationsService::Resume(const QString &id)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_manager && m_manager->Resume(id);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Resume"),
                     requestId,
                     caller,
                     id,
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), ok}});
    return ok;
}

bool FileOperationsService::Cancel(const QString &id)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const bool ok = m_manager && m_manager->Cancel(id);
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("Cancel"),
                     requestId,
                     caller,
                     id,
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), ok}});
    return ok;
}

QVariantMap FileOperationsService::GetJob(const QString &id)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const QVariantMap result = m_manager ? m_manager->GetJob(id) : QVariantMap();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("GetJob"),
                     requestId,
                     caller,
                     id,
                     QStringLiteral("finish"),
                     {{QStringLiteral("ok"), !result.isEmpty()}});
    return result;
}

QVariantList FileOperationsService::ListJobs()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const QVariantList result = m_manager ? m_manager->ListJobs() : QVariantList();
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                     QStringLiteral("ListJobs"),
                     requestId,
                     caller,
                     QString(),
                     QStringLiteral("finish"),
                     {{QStringLiteral("count"), result.size()}});
    return result;
}

void FileOperationsService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllSignals |
                                           QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}
