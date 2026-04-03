#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// DbusInspectorController — browse and introspect D-Bus session bus objects.
//
// Workflow:
//   1. refreshServices() → populates services list from ListNames()
//   2. setSelectedService() → introspects "/" to discover root interfaces + child paths
//   3. setSelectedPath() → introspects that path, populates interfaces
//
// Exposed to QML as the "DbusInspector" context property.
//
class DbusInspectorController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList  services        READ services        NOTIFY servicesChanged)
    Q_PROPERTY(bool         showAllServices READ showAllServices WRITE setShowAllServices
               NOTIFY showAllServicesChanged)
    Q_PROPERTY(QString      selectedService READ selectedService WRITE setSelectedService
               NOTIFY selectedServiceChanged)
    Q_PROPERTY(QStringList  paths           READ paths           NOTIFY pathsChanged)
    Q_PROPERTY(QString      selectedPath    READ selectedPath    WRITE setSelectedPath
               NOTIFY selectedPathChanged)
    Q_PROPERTY(QVariantList interfaces      READ interfaces      NOTIFY interfacesChanged)
    Q_PROPERTY(QString      introspectError READ introspectError NOTIFY introspectErrorChanged)
    Q_PROPERTY(bool         loading         READ loading         NOTIFY loadingChanged)

public:
    explicit DbusInspectorController(QObject *parent = nullptr);

    QStringList  services()        const { return m_services;        }
    bool         showAllServices() const { return m_showAll;         }
    QString      selectedService() const { return m_selectedService; }
    QStringList  paths()           const { return m_paths;           }
    QString      selectedPath()    const { return m_selectedPath;    }
    QVariantList interfaces()      const { return m_interfaces;      }
    QString      introspectError() const { return m_introspectError; }
    bool         loading()         const { return m_loading;         }

    void setShowAllServices(bool show);
    void setSelectedService(const QString &service);
    void setSelectedPath(const QString &path);

    Q_INVOKABLE void refreshServices();

    // Returns a human-readable signature string from an arg list.
    // args: [{name, type, direction}]
    Q_INVOKABLE QString formatArgList(const QVariantList &args, const QString &direction) const;

signals:
    void servicesChanged();
    void showAllServicesChanged();
    void selectedServiceChanged();
    void pathsChanged();
    void selectedPathChanged();
    void interfacesChanged();
    void introspectErrorChanged();
    void loadingChanged();

private:
    // Calls org.freedesktop.DBus.Introspectable.Introspect on service+path.
    // Populates m_interfaces and optionally extends m_paths with child nodes.
    void doIntrospect(const QString &service, const QString &path, bool appendPaths);

    // Parse introspection XML. Returns interface list; appends child paths to childPaths.
    static QVariantList parseXml(const QString &xml,
                                  const QString &basePath,
                                  QStringList   &childPaths);

    void applyServiceFilter();
    void setLoading(bool v);
    void setIntrospectError(const QString &err);

    QStringList  m_allServices;
    QStringList  m_services;
    bool         m_showAll         = false;
    QString      m_selectedService;
    QStringList  m_paths;
    QString      m_selectedPath;
    QVariantList m_interfaces;
    QString      m_introspectError;
    bool         m_loading         = false;
};
