#include "portaluibridge.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QEventLoop>
#include <QTimer>
#include <QUuid>

namespace {
constexpr const char kService[] = "org.slm.Desktop.PortalUI";
constexpr const char kPath[] = "/org/slm/Desktop/PortalUI";
constexpr int kChooserTimeoutMs = 180000;
constexpr int kConsentTimeoutMs = 180000;
}

PortalUiBridge::PortalUiBridge(QObject *parent)
    : QObject(parent)
{
    registerDbusService();
}

PortalUiBridge::~PortalUiBridge()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool PortalUiBridge::serviceRegistered() const
{
    return m_serviceRegistered;
}

QVariantMap PortalUiBridge::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_serviceRegistered},
    };
}

QVariantMap PortalUiBridge::FileChooser(const QVariantMap &options)
{
    if (!m_serviceRegistered) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("ui-service-unavailable")},
        };
    }

    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool completed = false;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        completed = false;
        loop.quit();
    });
    QObject::connect(this, &PortalUiBridge::fileChooserResultSubmitted, &loop, [&](const QString &id) {
        if (id != requestId) {
            return;
        }
        completed = true;
        loop.quit();
    });

    emit fileChooserRequested(requestId, options);
    timeout.start(kChooserTimeoutMs);
    loop.exec();

    if (!completed) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("canceled"), true},
            {QStringLiteral("error"), QStringLiteral("timeout")},
            {QStringLiteral("requestId"), requestId},
        };
    }

    const QVariantMap result = m_results.take(requestId).toMap();
    QVariantMap out = result;
    if (!out.contains(QStringLiteral("requestId"))) {
        out.insert(QStringLiteral("requestId"), requestId);
    }
    if (!out.contains(QStringLiteral("ok"))) {
        out.insert(QStringLiteral("ok"), false);
    }
    return out;
}

QVariantMap PortalUiBridge::ConsentRequest(const QVariantMap &payload)
{
    if (!m_serviceRegistered) {
        return {
            {QStringLiteral("decision"), QStringLiteral("deny")},
            {QStringLiteral("persist"), false},
            {QStringLiteral("scope"), QStringLiteral("session")},
            {QStringLiteral("reason"), QStringLiteral("ui-service-unavailable")},
        };
    }

    QString requestPath = payload.value(QStringLiteral("requestPath")).toString().trimmed();
    if (requestPath.isEmpty()) {
        requestPath = QStringLiteral("/org/slm/portal/request/consent/%1")
                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    bool completed = false;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        completed = false;
        loop.quit();
    });
    QObject::connect(this, &PortalUiBridge::consentResultSubmitted, &loop, [&](const QString &id) {
        if (id != requestPath) {
            return;
        }
        completed = true;
        loop.quit();
    });

    emit consentRequested(requestPath, payload);
    timeout.start(kConsentTimeoutMs);
    loop.exec();

    if (!completed) {
        return {
            {QStringLiteral("decision"), QStringLiteral("deny")},
            {QStringLiteral("persist"), false},
            {QStringLiteral("scope"), QStringLiteral("session")},
            {QStringLiteral("reason"), QStringLiteral("consent-timeout")},
        };
    }

    QVariantMap out = m_consentResults.take(requestPath).toMap();
    if (!out.contains(QStringLiteral("decision"))) {
        out.insert(QStringLiteral("decision"), QStringLiteral("deny"));
    }
    if (!out.contains(QStringLiteral("persist"))) {
        out.insert(QStringLiteral("persist"), false);
    }
    if (!out.contains(QStringLiteral("scope"))) {
        out.insert(QStringLiteral("scope"), QStringLiteral("session"));
    }
    if (!out.contains(QStringLiteral("reason"))) {
        out.insert(QStringLiteral("reason"), QStringLiteral("consent-ui-unhandled"));
    }
    return out;
}

void PortalUiBridge::submitFileChooserResult(const QString &requestId,
                                             const QVariantMap &result)
{
    if (requestId.trimmed().isEmpty()) {
        return;
    }
    m_results.insert(requestId, result);
    emit fileChooserResultSubmitted(requestId);
}

void PortalUiBridge::submitConsentResult(const QString &requestPath,
                                         const QVariantMap &result)
{
    if (requestPath.trimmed().isEmpty()) {
        return;
    }
    m_consentResults.insert(requestPath, result);
    emit consentResultSubmitted(requestPath);
}

void PortalUiBridge::registerDbusService()
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
