#include "helperdbusinterface.h"
#include "auditlog.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QProcess>

static constexpr char kServiceName[] = "org.slm.EnvironmentHelper1";
static constexpr char kObjectPath[]  = "/org/slm/EnvironmentHelper";
static constexpr char kActionId[]    = "org.slm.environment.write-system";
static constexpr int  kPkcheckTimeoutMs = 30000; // allow time for auth dialog

HelperDbusInterface::HelperDbusInterface(HelperService *service, QObject *parent)
    : QObject(parent)
    , m_service(service)
{
}

bool HelperDbusInterface::registerOn(QDBusConnection &bus)
{
    if (!bus.registerService(QLatin1String(kServiceName)))
        return false;
    return bus.registerObject(
        QLatin1String(kObjectPath), this,
        QDBusConnection::ExportAllSlots
            | QDBusConnection::ExportAllSignals);
}

// ── Authorization ─────────────────────────────────────────────────────────────

bool HelperDbusInterface::checkAuthorization(quint32 &outUid)
{
    outUid = 0;

    // Identify the caller on the system bus.
    const QString sender = QDBusContext::message().service();
    if (sender.isEmpty())
        return false;

    QDBusConnectionInterface *busIface = connection().interface();
    const QDBusReply<uint> pidReply = busIface->servicePid(sender);
    const QDBusReply<uint> uidReply = busIface->serviceUid(sender);
    if (!pidReply.isValid() || !uidReply.isValid())
        return false;

    const uint callerPid = pidReply.value();
    outUid = uidReply.value();

    // pkcheck verifies org.slm.environment.write-system for the calling process.
    // --allow-user-interaction lets polkit prompt the user's session agent.
    QProcess pkcheck;
    pkcheck.start(QStringLiteral("pkcheck"),
                  { QStringLiteral("--action-id"), QLatin1String(kActionId),
                    QStringLiteral("--process"),    QString::number(callerPid),
                    QStringLiteral("--allow-user-interaction") });

    if (!pkcheck.waitForStarted(2000))
        return false;

    pkcheck.waitForFinished(kPkcheckTimeoutMs);
    return pkcheck.exitStatus() == QProcess::NormalExit && pkcheck.exitCode() == 0;
}

// ── D-Bus slots ───────────────────────────────────────────────────────────────

QVariantMap HelperDbusInterface::WriteSystemEntry(const QString &key,
                                                   const QString &value,
                                                   const QString &comment,
                                                   const QString &mergeMode,
                                                   bool           enabled)
{
    quint32 uid = 0;
    if (!checkAuthorization(uid)) {
        AuditLog::record(QStringLiteral("write"), key, uid, false,
                         QStringLiteral("polkit-denied"));
        return err(QStringLiteral("Authorization denied"));
    }

    if (!m_service->writeEntry(key, value, comment, mergeMode, enabled)) {
        AuditLog::record(QStringLiteral("write"), key, uid, false,
                         m_service->lastError());
        return err(m_service->lastError());
    }

    AuditLog::record(QStringLiteral("write"), key, uid, true);
    emit SystemEntriesChanged();
    return ok();
}

QVariantMap HelperDbusInterface::DeleteSystemEntry(const QString &key)
{
    quint32 uid = 0;
    if (!checkAuthorization(uid)) {
        AuditLog::record(QStringLiteral("delete"), key, uid, false,
                         QStringLiteral("polkit-denied"));
        return err(QStringLiteral("Authorization denied"));
    }

    if (!m_service->deleteEntry(key)) {
        AuditLog::record(QStringLiteral("delete"), key, uid, false,
                         m_service->lastError());
        return err(m_service->lastError());
    }

    AuditLog::record(QStringLiteral("delete"), key, uid, true);
    emit SystemEntriesChanged();
    return ok();
}

QVariantList HelperDbusInterface::GetSystemEntries()
{
    QVariantList result;
    for (const EnvEntry &e : m_service->entries()) {
        QVariantMap m;
        m[QStringLiteral("key")]        = e.key;
        m[QStringLiteral("value")]      = e.value;
        m[QStringLiteral("enabled")]    = e.enabled;
        m[QStringLiteral("comment")]    = e.comment;
        m[QStringLiteral("mergeMode")]  = e.mergeMode;
        m[QStringLiteral("modifiedAt")] = e.modifiedAt.toString(Qt::ISODate);
        result.append(m);
    }
    return result;
}

// ── helpers ───────────────────────────────────────────────────────────────────

QVariantMap HelperDbusInterface::ok()
{
    return { { QStringLiteral("ok"), true }, { QStringLiteral("error"), QString{} } };
}

QVariantMap HelperDbusInterface::err(const QString &message)
{
    return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), message } };
}
