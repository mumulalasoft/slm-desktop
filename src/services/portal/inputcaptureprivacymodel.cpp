#include "inputcaptureprivacymodel.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QVariantMap>

namespace {
constexpr const char kService[] = "org.slm.Compositor.InputCaptureBackend";
constexpr const char kPath[] = "/org/slm/Compositor/InputCaptureBackend";
constexpr const char kIface[] = "org.slm.Compositor.InputCaptureBackend";
constexpr const char kActiveSignal[] = "CaptureActiveChanged";
constexpr const char kSessionSignal[] = "SessionStateChanged";
}

InputCapturePrivacyModel::InputCapturePrivacyModel(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kService),
                                          QString::fromLatin1(kPath),
                                          QString::fromLatin1(kIface),
                                          QString::fromLatin1(kActiveSignal),
                                          this,
                                          SLOT(onCaptureActiveChanged(bool,QString)));
    QDBusConnection::sessionBus().connect(QString::fromLatin1(kService),
                                          QString::fromLatin1(kPath),
                                          QString::fromLatin1(kIface),
                                          QString::fromLatin1(kSessionSignal),
                                          this,
                                          SLOT(onSessionStateChanged(QString,bool,int)));
    refresh();
}

bool InputCapturePrivacyModel::active() const
{
    return m_active;
}

QString InputCapturePrivacyModel::activeSession() const
{
    return m_activeSession;
}

int InputCapturePrivacyModel::enabledCount() const
{
    return m_enabledCount;
}

QVariantList InputCapturePrivacyModel::sessionItems() const
{
    return m_sessionItems;
}

QString InputCapturePrivacyModel::statusTitle() const
{
    if (!m_active) {
        return tr("No active input capture");
    }
    if (m_enabledCount <= 1) {
        return tr("Input capture is active");
    }
    return tr("Input capture is active (%1 sessions)").arg(m_enabledCount);
}

QString InputCapturePrivacyModel::statusSubtitle() const
{
    if (!m_activeSession.isEmpty()) {
        return tr("Active session: %1").arg(m_activeSession);
    }
    if (m_active) {
        return tr("Session active");
    }
    return QString();
}

QString InputCapturePrivacyModel::tooltipText() const
{
    if (!m_active) {
        return tr("No active input capture");
    }
    if (m_enabledCount <= 1) {
        return tr("Input capture active");
    }
    return tr("Input capture active (%1)").arg(m_enabledCount);
}

QString InputCapturePrivacyModel::lastActionText() const
{
    return m_lastActionText;
}

void InputCapturePrivacyModel::refresh()
{
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        setState(false, {}, 0, {});
        return;
    }

    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetState"));
    if (!reply.isValid()) {
        setState(false, {}, 0, {});
        return;
    }
    const QVariantMap out = reply.value();
    if (!out.value(QStringLiteral("ok"), false).toBool()) {
        setState(false, {}, 0, {});
        return;
    }
    const QVariantMap results = out.value(QStringLiteral("results")).toMap();
    const bool isActive = results.value(QStringLiteral("active"), false).toBool();
    const QString activeSessionHandle = results.value(QStringLiteral("active_session")).toString().trimmed();
    const int enabled = qMax(0, results.value(QStringLiteral("enabled_count"), 0).toInt());
    const QVariantList sessions = results.value(QStringLiteral("sessions")).toList();
    QVariantList rows;
    rows.reserve(sessions.size());
    for (const QVariant &rowValue : sessions) {
        const QVariantMap row = rowValue.toMap();
        const QString handle = row.value(QStringLiteral("session_handle")).toString().trimmed();
        if (handle.isEmpty()) {
            continue;
        }
        rows.push_back(QVariantMap{
            {QStringLiteral("session_handle"), handle},
            {QStringLiteral("app_id"), row.value(QStringLiteral("app_id")).toString().trimmed()},
            {QStringLiteral("enabled"), row.value(QStringLiteral("enabled")).toBool()},
            {QStringLiteral("barrier_count"), row.value(QStringLiteral("barrier_count")).toInt()},
        });
    }
    setState(isActive, activeSessionHandle, enabled, rows);
}

bool InputCapturePrivacyModel::disableSession(const QString &sessionHandle)
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
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("DisableSession"), normalized, QVariantMap{});
    const bool ok = reply.isValid() && reply.value().value(QStringLiteral("ok")).toBool();
    if (ok) {
        setLastActionText(tr("Disabled one capture session"));
    }
    refresh();
    return ok;
}

bool InputCapturePrivacyModel::releaseSession(const QString &sessionHandle, const QString &reason)
{
    const QString normalized = sessionHandle.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    const QString normalizedReason = reason.trimmed().isEmpty()
        ? QStringLiteral("released-by-user")
        : reason.trimmed();
    QDBusInterface iface(QString::fromLatin1(kService),
                         QString::fromLatin1(kPath),
                         QString::fromLatin1(kIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("ReleaseSession"),
                                               normalized,
                                               normalizedReason);
    const bool ok = reply.isValid() && reply.value().value(QStringLiteral("ok")).toBool();
    if (ok) {
        setLastActionText(tr("Released one capture session"));
    }
    refresh();
    return ok;
}

void InputCapturePrivacyModel::onCaptureActiveChanged(bool, const QString &)
{
    refresh();
}

void InputCapturePrivacyModel::onSessionStateChanged(const QString &, bool, int)
{
    refresh();
}

void InputCapturePrivacyModel::setState(bool activeState,
                                        const QString &activeSessionHandle,
                                        int enabledCountValue,
                                        const QVariantList &sessionItemsValue)
{
    const bool activeChangedValue = m_active != activeState;
    const bool sessionChangedValue = m_activeSession != activeSessionHandle;
    const int normalizedCount = qMax(0, enabledCountValue);
    const bool countChangedValue = m_enabledCount != normalizedCount;
    const bool itemsChangedValue = m_sessionItems != sessionItemsValue;
    if (!activeChangedValue && !sessionChangedValue && !countChangedValue && !itemsChangedValue) {
        return;
    }
    m_active = activeState;
    m_activeSession = activeSessionHandle;
    m_enabledCount = normalizedCount;
    m_sessionItems = sessionItemsValue;

    if (activeChangedValue) {
        emit activeChanged();
    }
    if (sessionChangedValue) {
        emit activeSessionChanged();
    }
    if (countChangedValue) {
        emit enabledCountChanged();
    }
    if (itemsChangedValue) {
        emit sessionItemsChanged();
    }
    emit presentationChanged();
}

void InputCapturePrivacyModel::setLastActionText(const QString &text)
{
    const QString normalized = text.trimmed();
    if (m_lastActionText == normalized) {
        return;
    }
    m_lastActionText = normalized;
    emit presentationChanged();
}

