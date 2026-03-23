#include "screencastprivacymodel.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QVariantMap>

namespace {
constexpr const char kService[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kIface[] = "org.freedesktop.impl.portal.desktop.slm";
constexpr const char kStateSignal[] = "ScreencastSessionStateChanged";
constexpr const char kRevokeSignal[] = "ScreencastSessionRevoked";
}

ScreencastPrivacyModel::ScreencastPrivacyModel(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kService),
                                          QString::fromLatin1(kPath),
                                          QString::fromLatin1(kIface),
                                          QString::fromLatin1(kStateSignal),
                                          this,
                                          SLOT(onSessionStateChanged(QString,QString,bool,int)));
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kService),
                                          QString::fromLatin1(kPath),
                                          QString::fromLatin1(kIface),
                                          QString::fromLatin1(kRevokeSignal),
                                          this,
                                          SLOT(onSessionRevoked(QString,QString,QString,int)));
    refresh();
}

bool ScreencastPrivacyModel::active() const
{
    return m_activeCount > 0;
}

int ScreencastPrivacyModel::activeCount() const
{
    return m_activeCount;
}

QStringList ScreencastPrivacyModel::activeApps() const
{
    return m_activeApps;
}

QVariantList ScreencastPrivacyModel::activeSessionItems() const
{
    return m_activeSessionItems;
}

QString ScreencastPrivacyModel::statusTitle() const
{
    if (m_activeCount <= 0) {
        return tr("No active screen sharing");
    }
    if (m_activeCount == 1) {
        return tr("Screen sharing is active");
    }
    return tr("Screen sharing is active (%1 sessions)").arg(m_activeCount);
}

QString ScreencastPrivacyModel::statusSubtitle() const
{
    if (m_activeApps.isEmpty()) {
        return m_activeCount > 0
            ? tr("Active session detected")
            : QString();
    }
    if (m_activeApps.size() == 1) {
        return tr("Shared by %1").arg(m_activeApps.constFirst());
    }
    return tr("Shared by %1 and %2 more")
        .arg(m_activeApps.constFirst())
        .arg(m_activeApps.size() - 1);
}

QString ScreencastPrivacyModel::tooltipText() const
{
    if (m_activeCount <= 0) {
        return tr("No active screen sharing");
    }
    if (m_activeCount == 1) {
        return tr("Screen sharing active");
    }
    return tr("Screen sharing active (%1)").arg(m_activeCount);
}

QString ScreencastPrivacyModel::lastActionText() const
{
    return m_lastActionText;
}

void ScreencastPrivacyModel::refresh()
{
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        setState(0, {}, {});
        return;
    }

    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetScreencastState"));
    if (!reply.isValid()) {
        setState(0, {}, {});
        return;
    }
    const QVariantMap out = reply.value();
    if (!out.value(QStringLiteral("ok"), false).toBool()) {
        setState(0, {}, {});
        return;
    }
    const QVariantMap results = out.value(QStringLiteral("results")).toMap();
    const int count = results.value(QStringLiteral("active_count"), 0).toInt();
    QStringList apps;
    const QVariantList appRows = results.value(QStringLiteral("active_apps")).toList();
    apps.reserve(appRows.size());
    for (const QVariant &v : appRows) {
        const QString appId = v.toString().trimmed();
        if (appId.isEmpty()) {
            continue;
        }
        const QString label = normalizeAppLabel(appId);
        if (!apps.contains(label)) {
            apps.push_back(label);
        }
    }
    const QVariantList sessionRows = results.value(QStringLiteral("active_session_items")).toList();
    QVariantList sessionItems;
    sessionItems.reserve(sessionRows.size());
    for (const QVariant &v : sessionRows) {
        const QVariantMap row = v.toMap();
        const QString session = row.value(QStringLiteral("session")).toString().trimmed();
        if (session.isEmpty()) {
            continue;
        }
        const QString appId = row.value(QStringLiteral("app_id")).toString().trimmed();
        const QString appLabel = row.value(QStringLiteral("app_label")).toString().trimmed();
        const QString iconName = row.value(QStringLiteral("icon_name")).toString().trimmed();
        sessionItems.push_back(QVariantMap{
            {QStringLiteral("session"), session},
            {QStringLiteral("app_id"), appId},
            {QStringLiteral("label"), appLabel.isEmpty() ? normalizeAppLabel(appId) : appLabel},
            {QStringLiteral("icon_name"), iconName},
        });
    }
    setState(count, apps, sessionItems);
}

void ScreencastPrivacyModel::onSessionStateChanged(const QString &,
                                                   const QString &,
                                                   bool isActive,
                                                   int)
{
    if (!isActive) {
        setLastActionText(tr("Session stopped"));
    }
    refresh();
}

void ScreencastPrivacyModel::onSessionRevoked(const QString &,
                                              const QString &,
                                              const QString &reason,
                                              int)
{
    const QString normalizedReason = reason.trimmed();
    if (normalizedReason.isEmpty()) {
        setLastActionText(tr("Session revoked"));
    } else {
        setLastActionText(tr("Session revoked: %1").arg(normalizedReason));
    }
    refresh();
}

bool ScreencastPrivacyModel::stopSession(const QString &sessionHandle)
{
    const QString normalized = sessionHandle.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("CloseScreencastSession"), normalized);
    const bool ok = reply.isValid() && reply.value().value(QStringLiteral("ok")).toBool();
    if (ok) {
        setLastActionText(tr("Stopped sharing for one session"));
    }
    refresh();
    return ok;
}

bool ScreencastPrivacyModel::revokeSession(const QString &sessionHandle, const QString &reason)
{
    const QString normalized = sessionHandle.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    const QString normalizedReason = reason.trimmed().isEmpty()
        ? QStringLiteral("revoked-by-user")
        : reason.trimmed();
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return false;
    }
    QDBusReply<QVariantMap> reply =
        iface.call(QStringLiteral("RevokeScreencastSession"), normalized, normalizedReason);
    const bool ok = reply.isValid() && reply.value().value(QStringLiteral("ok")).toBool();
    if (ok) {
        setLastActionText(tr("Revoked sharing for one session"));
    }
    refresh();
    return ok;
}

int ScreencastPrivacyModel::stopAllSessions()
{
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return 0;
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("CloseAllScreencastSessions"));
    if (!reply.isValid()) {
        return 0;
    }
    const QVariantMap results = reply.value().value(QStringLiteral("results")).toMap();
    const int closedCount = results.value(QStringLiteral("closed_count"), 0).toInt();
    if (closedCount > 0) {
        setLastActionText(tr("Stopped sharing for %1 sessions").arg(closedCount));
    }
    refresh();
    return closedCount;
}

void ScreencastPrivacyModel::setState(int activeCount,
                                      const QStringList &activeApps,
                                      const QVariantList &sessionItems)
{
    const bool oldActive = active();
    const int nextCount = qMax(0, activeCount);

    const bool countChanged = m_activeCount != nextCount;
    const bool appsChanged = m_activeApps != activeApps;
    const bool itemsChanged = m_activeSessionItems != sessionItems;
    if (!countChanged && !appsChanged && !itemsChanged) {
        return;
    }

    m_activeCount = nextCount;
    m_activeApps = activeApps;
    m_activeSessionItems = sessionItems;

    if (countChanged) {
        emit activeCountChanged();
    }
    const bool newActive = active();
    if (oldActive != newActive) {
        emit activeChanged();
    }
    if (appsChanged) {
        emit activeAppsChanged();
    }
    if (itemsChanged) {
        emit activeSessionItemsChanged();
    }
    emit presentationChanged();
}

void ScreencastPrivacyModel::setLastActionText(const QString &text)
{
    const QString normalized = text.trimmed();
    if (m_lastActionText == normalized) {
        return;
    }
    m_lastActionText = normalized;
    emit presentationChanged();
}

QString ScreencastPrivacyModel::normalizeAppLabel(const QString &appId)
{
    QString label = appId.trimmed();
    if (label.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        label.chop(8);
    }
    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    return label;
}
