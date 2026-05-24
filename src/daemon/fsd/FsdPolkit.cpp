#include "FsdPolkit.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>

namespace {

constexpr const char kPolkitService[]   = "org.freedesktop.PolicyKit1";
constexpr const char kPolkitPath[]      = "/org/freedesktop/PolicyKit1/Authority";
constexpr const char kPolkitInterface[] = "org.freedesktop.PolicyKit1.Authority";

// Maps verb names to polkit action IDs.
const struct { const char *verb; const char *actionId; } kVerbActionMap[] = {
    { "rename",   "org.slm.desktop.fsd.rename"   },
    { "delete",   "org.slm.desktop.fsd.delete"   },
    { "purge",    "org.slm.desktop.fsd.purge"     },
    { "move",     "org.slm.desktop.fsd.move"      },
    { "copy",     "org.slm.desktop.fsd.copy"      },
    { "write",    "org.slm.desktop.fsd.write"     },
    { "create",   "org.slm.desktop.fsd.create"    },
    { "chmod",    "org.slm.desktop.fsd.chmod"     },
    { "chown",    "org.slm.desktop.fsd.chown"     },
    { "mount",    "org.slm.desktop.fsd.mount"     },
    { "unmount",  "org.slm.desktop.fsd.unmount"   },
};

} // namespace

FsdPolkit::FsdPolkit(QObject *parent)
    : QObject(parent)
{
}

QString FsdPolkit::actionIdForVerb(const QString &verb)
{
    for (const auto &entry : kVerbActionMap) {
        if (verb == QLatin1String(entry.verb))
            return QString::fromLatin1(entry.actionId);
    }
    return {};
}

QString FsdPolkit::checkAuthorization(const QString &callerUniqueName, const QString &verb)
{
    const QString actionId = actionIdForVerb(verb);
    if (actionId.isEmpty()) {
        qWarning().noquote() << "[slm-fsd][polkit] unknown verb:" << verb;
        return QStringLiteral("UnknownVerb");
    }

    // Phase 0 stub: log the check but do not block.
    // TODO(Phase1): replace with callPolicyKit(callerUniqueName, actionId)
    qInfo().noquote() << "[slm-fsd][polkit] STUB check action=" << actionId
                      << "caller=" << callerUniqueName
                      << "(stub: returning NotImplemented)";
    return QStringLiteral("NotImplemented");
}

// Phase 1 implementation — kept here for reference; not called yet.
QString FsdPolkit::callPolicyKit(const QString &callerUniqueName, const QString &actionId)
{
    // Subject: system-bus-name with the caller's unique name
    // Format: (sa{sv}) where s = "system-bus-name", a{sv} = {"name": callerUniqueName}
    QVariantMap subjectDetails;
    subjectDetails[QStringLiteral("name")] = QDBusVariant(callerUniqueName);
    const QVariant subject = QVariant::fromValue(
        QDBusArgument() << QString::fromLatin1("system-bus-name") << subjectDetails);

    // AllowUserInteraction flag = 1 → allow polkit agent to show a dialog
    constexpr quint32 kAllowUserInteraction = 1;

    QDBusInterface authority(QString::fromLatin1(kPolkitService),
                             QString::fromLatin1(kPolkitPath),
                             QString::fromLatin1(kPolkitInterface),
                             QDBusConnection::systemBus());

    if (!authority.isValid()) {
        qWarning().noquote() << "[slm-fsd][polkit] PolicyKit1 not available";
        return QStringLiteral("PolkitUnavailable");
    }

    // CheckAuthorization(subject, action_id, details, flags, cancellation_id)
    QDBusReply<QVariantMap> reply = authority.call(
        QStringLiteral("CheckAuthorization"),
        subject,
        actionId,
        QVariantMap{},                  // no extra details
        kAllowUserInteraction,
        QString{}                       // no cancellation id
    );

    if (!reply.isValid()) {
        qWarning().noquote() << "[slm-fsd][polkit] D-Bus error:"
                             << reply.error().message();
        return QStringLiteral("PolkitUnavailable");
    }

    const QVariantMap result = reply.value();
    const bool isAuthorized = result.value(QStringLiteral("is_authorized"), false).toBool();

    if (!isAuthorized) {
        qInfo().noquote() << "[slm-fsd][polkit] denied action=" << actionId
                          << "caller=" << callerUniqueName;
        return QStringLiteral("PermissionDenied");
    }

    return {}; // authorized
}
