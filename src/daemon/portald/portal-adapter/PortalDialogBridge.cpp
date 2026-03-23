#include "PortalDialogBridge.h"

#include "../../../core/permissions/Capability.h"

#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusConnection>
#include <QMetaMethod>
#include <QMetaObject>

namespace Slm::PortalAdapter {

namespace {
constexpr const char kUiService[] = "org.slm.Desktop.PortalUI";
constexpr const char kUiPath[] = "/org/slm/Desktop/PortalUI";
constexpr const char kUiIface[] = "org.slm.Desktop.PortalUI";
constexpr int kUiTimeoutMs = 190000;

UserDecision parseUserDecision(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("allow_once")) {
        return UserDecision::AllowOnce;
    }
    if (v == QStringLiteral("allow_always")) {
        return UserDecision::AllowAlways;
    }
    if (v == QStringLiteral("deny")) {
        return UserDecision::Deny;
    }
    if (v == QStringLiteral("deny_always")) {
        return UserDecision::DenyAlways;
    }
    return UserDecision::Cancelled;
}

} // namespace

PortalDialogBridge::PortalDialogBridge(QObject *parent)
    : QObject(parent)
{
}

void PortalDialogBridge::requestConsent(const QString &requestPath,
                                        const Slm::Permissions::CallerIdentity &caller,
                                        Slm::Permissions::Capability capability,
                                        const Slm::Permissions::AccessContext &context,
                                        const ConsentCallback &callback)
{
    m_pending.insert(requestPath, callback);
    const QVariantMap payload{
        {QStringLiteral("requestPath"), requestPath},
        {QStringLiteral("appId"), caller.appId},
        {QStringLiteral("desktopFileId"), caller.desktopFileId},
        {QStringLiteral("busName"), caller.busName},
        {QStringLiteral("capability"), Slm::Permissions::capabilityToString(capability)},
        {QStringLiteral("resourceType"), context.resourceType},
        {QStringLiteral("resourceId"), context.resourceId},
        {QStringLiteral("sensitivityLevel"), Slm::Permissions::sensitivityToString(context.sensitivityLevel)},
        {QStringLiteral("initiatedByUserGesture"), context.initiatedByUserGesture},
    };
    const bool hasInProcessUiListener =
        isSignalConnected(QMetaMethod::fromSignal(&PortalDialogBridge::consentRequested));
    if (hasInProcessUiListener) {
        emit consentRequested(requestPath, payload);
        return;
    }

    // Trusted UI D-Bus bridge path (production).
    QDBusInterface iface(QString::fromLatin1(kUiService),
                         QString::fromLatin1(kUiPath),
                         QString::fromLatin1(kUiIface),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        iface.setTimeout(kUiTimeoutMs);
        // Expected reply payload shape:
        // { decision: "allow_once|allow_always|deny|deny_always|cancelled",
        //   persist: bool, scope: string, reason: string }
        QDBusPendingCall pending = iface.asyncCall(QStringLiteral("ConsentRequest"), payload);
        auto *watcher = new QDBusPendingCallWatcher(pending, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, requestPath](QDBusPendingCallWatcher *w) {
                    QDBusPendingReply<QVariantMap> reply = *w;
                    if (reply.isError()) {
                        submitConsent(requestPath,
                                      QStringLiteral("deny"),
                                      false,
                                      QString(),
                                      QStringLiteral("ui-consent-call-failed"));
                        w->deleteLater();
                        return;
                    }
                    const QVariantMap result = reply.value();
                    submitConsent(requestPath,
                                  result.value(QStringLiteral("decision"),
                                               QStringLiteral("cancelled")).toString(),
                                  result.value(QStringLiteral("persist")).toBool(),
                                  result.value(QStringLiteral("scope")).toString(),
                                  result.value(QStringLiteral("reason")).toString());
                    w->deleteLater();
                });
        return;
    }

    // No trusted UI bridge available: fail closed.
    QMetaObject::invokeMethod(
        this,
        [this, requestPath]() {
            submitConsent(requestPath,
                          QStringLiteral("deny"),
                          false,
                          QString(),
                          QStringLiteral("no-consent-ui-bridge"));
        },
        Qt::QueuedConnection);
}

void PortalDialogBridge::submitConsent(const QString &requestPath,
                                       const QString &decision,
                                       bool persist,
                                       const QString &scope,
                                       const QString &reason)
{
    const ConsentCallback callback = m_pending.take(requestPath);
    if (!callback) {
        return;
    }
    ConsentResult result;
    result.decision = parseUserDecision(decision);
    result.persist = persist;
    result.scope = scope.trimmed();
    result.reason = reason.trimmed();
    callback(result);
}

} // namespace Slm::PortalAdapter
