#include "dbusinspectorcontroller.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QXmlStreamReader>
#include <QDebug>

static constexpr int kCallTimeoutMs = 3000;

// ── XML parser ────────────────────────────────────────────────────────────────

QVariantList DbusInspectorController::parseXml(const QString &xml,
                                                const QString &basePath,
                                                QStringList   &childPaths)
{
    QVariantList result;
    QXmlStreamReader r(xml);

    // Per-interface state
    QVariantMap currentIface;
    QVariantList ifaceMethods, ifaceProps, ifaceSigs;

    // Per-method/signal state
    QString      currentKind;   // "method" | "signal" | ""
    QVariantMap  currentCallable;
    QVariantList currentArgs;

    while (!r.atEnd()) {
        r.readNext();

        if (r.isStartElement()) {
            const QStringView tag = r.name();

            if (tag == QLatin1String("interface")) {
                currentIface = {{ QStringLiteral("name"),
                                   r.attributes().value(QLatin1String("name")).toString() }};
                ifaceMethods.clear();
                ifaceProps.clear();
                ifaceSigs.clear();

            } else if (!currentIface.isEmpty() && tag == QLatin1String("method")) {
                currentKind = QStringLiteral("method");
                currentCallable = {{ QStringLiteral("name"),
                                      r.attributes().value(QLatin1String("name")).toString() }};
                currentArgs.clear();

            } else if (!currentIface.isEmpty() && tag == QLatin1String("signal")) {
                currentKind = QStringLiteral("signal");
                currentCallable = {{ QStringLiteral("name"),
                                      r.attributes().value(QLatin1String("name")).toString() }};
                currentArgs.clear();

            } else if (tag == QLatin1String("arg")) {
                currentArgs.append(QVariantMap{
                    { QStringLiteral("name"),      r.attributes().value(QLatin1String("name")).toString()      },
                    { QStringLiteral("type"),      r.attributes().value(QLatin1String("type")).toString()      },
                    { QStringLiteral("direction"), r.attributes().value(QLatin1String("direction")).toString() },
                });

            } else if (!currentIface.isEmpty() && tag == QLatin1String("property")) {
                ifaceProps.append(QVariantMap{
                    { QStringLiteral("name"),   r.attributes().value(QLatin1String("name")).toString()   },
                    { QStringLiteral("type"),   r.attributes().value(QLatin1String("type")).toString()   },
                    { QStringLiteral("access"), r.attributes().value(QLatin1String("access")).toString() },
                });

            } else if (tag == QLatin1String("node")) {
                const QString nodeName =
                    r.attributes().value(QLatin1String("name")).toString();
                if (!nodeName.isEmpty() && nodeName != QLatin1String(".")) {
                    QString childPath = basePath;
                    if (childPath != QLatin1String("/"))
                        childPath += QLatin1Char('/');
                    childPath += nodeName;
                    childPaths.append(childPath);
                }
            }

        } else if (r.isEndElement()) {
            const QStringView tag = r.name();

            if (tag == QLatin1String("method") && currentKind == QLatin1String("method")) {
                currentCallable[QStringLiteral("args")] = currentArgs;
                ifaceMethods.append(currentCallable);
                currentKind.clear();

            } else if (tag == QLatin1String("signal") && currentKind == QLatin1String("signal")) {
                currentCallable[QStringLiteral("args")] = currentArgs;
                ifaceSigs.append(currentCallable);
                currentKind.clear();

            } else if (tag == QLatin1String("interface") && !currentIface.isEmpty()) {
                currentIface[QStringLiteral("methods")]    = ifaceMethods;
                currentIface[QStringLiteral("properties")] = ifaceProps;
                currentIface[QStringLiteral("signals")]    = ifaceSigs;
                result.append(currentIface);
                currentIface.clear();
            }
        }
    }
    return result;
}

// ── Controller ────────────────────────────────────────────────────────────────

DbusInspectorController::DbusInspectorController(QObject *parent)
    : QObject(parent)
{
    refreshServices();
}

void DbusInspectorController::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}

void DbusInspectorController::setIntrospectError(const QString &err)
{
    if (m_introspectError == err) return;
    m_introspectError = err;
    emit introspectErrorChanged();
}

void DbusInspectorController::applyServiceFilter()
{
    QStringList filtered;
    for (const QString &s : std::as_const(m_allServices)) {
        if (m_showAll || !s.startsWith(QLatin1Char(':')))
            filtered.append(s);
    }
    filtered.sort(Qt::CaseInsensitive);
    if (filtered != m_services) {
        m_services = filtered;
        emit servicesChanged();
    }
}

void DbusInspectorController::refreshServices()
{
    const QDBusReply<QStringList> reply =
        QDBusConnection::sessionBus().interface()->registeredServiceNames();
    if (!reply.isValid()) {
        qWarning() << "[DbusInspector] registeredServiceNames failed:" << reply.error().message();
        return;
    }
    m_allServices = reply.value();
    applyServiceFilter();
}

void DbusInspectorController::setShowAllServices(bool show)
{
    if (m_showAll == show) return;
    m_showAll = show;
    emit showAllServicesChanged();
    applyServiceFilter();
}

void DbusInspectorController::setSelectedService(const QString &service)
{
    if (m_selectedService == service) return;
    m_selectedService = service;
    m_paths.clear();
    m_selectedPath.clear();
    m_interfaces.clear();
    setIntrospectError(QString());
    emit selectedServiceChanged();
    emit pathsChanged();
    emit selectedPathChanged();
    emit interfacesChanged();

    if (service.isEmpty()) return;

    // Introspect root; child paths go into m_paths.
    m_paths.append(QStringLiteral("/"));
    emit pathsChanged();
    doIntrospect(service, QStringLiteral("/"), true);

    // Auto-select root path to show its interfaces immediately.
    setSelectedPath(QStringLiteral("/"));
}

void DbusInspectorController::setSelectedPath(const QString &path)
{
    if (m_selectedPath == path) return;
    m_selectedPath = path;
    m_interfaces.clear();
    setIntrospectError(QString());
    emit selectedPathChanged();
    emit interfacesChanged();

    if (path.isEmpty() || m_selectedService.isEmpty()) return;
    doIntrospect(m_selectedService, path, false);
}

void DbusInspectorController::doIntrospect(const QString &service,
                                             const QString &path,
                                             bool appendPaths)
{
    setLoading(true);

    const QDBusMessage req = QDBusMessage::createMethodCall(
        service,
        path,
        QStringLiteral("org.freedesktop.DBus.Introspectable"),
        QStringLiteral("Introspect"));

    const QDBusMessage reply =
        QDBusConnection::sessionBus().call(req, QDBus::Block, kCallTimeoutMs);

    setLoading(false);

    if (reply.type() == QDBusMessage::ErrorMessage) {
        setIntrospectError(reply.errorMessage());
        return;
    }

    const QString xml = reply.arguments().value(0).toString();
    if (xml.isEmpty()) {
        setIntrospectError(QStringLiteral("Empty introspection response"));
        return;
    }

    QStringList childPaths;
    const QVariantList ifaces = parseXml(xml, path, childPaths);

    if (appendPaths) {
        for (const QString &cp : std::as_const(childPaths)) {
            if (!m_paths.contains(cp))
                m_paths.append(cp);
        }
        if (!childPaths.isEmpty())
            emit pathsChanged();
    }

    // Only update interfaces if this introspection is still for the selected path.
    if (m_selectedPath == path) {
        m_interfaces = ifaces;
        emit interfacesChanged();
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString DbusInspectorController::formatArgList(const QVariantList &args,
                                                const QString &direction) const
{
    QStringList parts;
    for (const QVariant &v : args) {
        const QVariantMap a = v.toMap();
        const QString dir = a.value(QStringLiteral("direction")).toString();
        if (!direction.isEmpty() && dir != direction)
            continue;
        const QString type = a.value(QStringLiteral("type")).toString();
        const QString name = a.value(QStringLiteral("name")).toString();
        parts.append(name.isEmpty() ? type : type + QLatin1Char(' ') + name);
    }
    return QLatin1Char('(') + parts.join(QStringLiteral(", ")) + QLatin1Char(')');
}
