#include "xdgportalscontroller.h"

XdgPortalsController::XdgPortalsController(QObject *parent)
    : QObject(parent)
{
    connect(&m_service, &PortalBridgeService::descriptorsChanged, this, [this] {
        emit interfacesChanged();
        emit descriptorsChanged();
    });
    connect(&m_service, &PortalBridgeService::configChanged,
            this, &XdgPortalsController::overridesChanged);
}

QStringList  XdgPortalsController::interfaces()     const { return m_service.interfaces(); }
QVariantList XdgPortalsController::descriptors()    const { return m_service.descriptors(); }
QVariantMap  XdgPortalsController::overrides()      const { return m_service.handlerOverrides(); }
QString      XdgPortalsController::defaultHandler() const { return m_service.defaultHandler(); }
QString      XdgPortalsController::lastError()      const { return m_lastError; }

QVariantList XdgPortalsController::handlersFor(const QString &iface) const
{
    return m_service.handlersFor(iface);
}

QString XdgPortalsController::preferredHandler(const QString &iface) const
{
    return m_service.preferredHandler(iface);
}

bool XdgPortalsController::setPreferredHandler(const QString &iface, const QString &handler)
{
    const bool ok = m_service.setPreferredHandler(iface, handler);
    if (!ok) {
        m_lastError = m_service.lastError();
        emit lastErrorChanged();
    }
    return ok;
}

bool XdgPortalsController::resetAllHandlers()
{
    const bool ok = m_service.resetAllHandlers();
    if (!ok) {
        m_lastError = m_service.lastError();
        emit lastErrorChanged();
    }
    return ok;
}

void XdgPortalsController::refresh()
{
    m_service.reload();
}
