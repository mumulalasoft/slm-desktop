#include "externalindicatorregistry.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <algorithm>

namespace {
constexpr const char *kService = "org.slm.IndicatorService";
constexpr const char *kPath = "/org/slm/IndicatorRegistry";
constexpr const char *kLegacyService = "org.desktop_shell.IndicatorService";
constexpr const char *kLegacyPath = "/org/desktop_shell/IndicatorRegistry";

class LegacyIndicatorRegistryAdaptor : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktop_shell.IndicatorRegistry")

public:
    explicit LegacyIndicatorRegistryAdaptor(ExternalIndicatorRegistry *registry, QObject *parent = nullptr)
        : QObject(parent)
        , m_registry(registry)
    {
        if (m_registry) {
            connect(m_registry, &ExternalIndicatorRegistry::entriesChanged,
                    this, &LegacyIndicatorRegistryAdaptor::entriesChanged);
            connect(m_registry, &ExternalIndicatorRegistry::IndicatorAdded,
                    this, &LegacyIndicatorRegistryAdaptor::IndicatorAdded);
            connect(m_registry, &ExternalIndicatorRegistry::IndicatorRemoved,
                    this, &LegacyIndicatorRegistryAdaptor::IndicatorRemoved);
        }
    }

public slots:
    int RegisterIndicator(const QString &name,
                          const QString &source,
                          int order,
                          bool visible,
                          bool enabled,
                          const QVariantMap &properties)
    {
        return m_registry ? m_registry->RegisterIndicator(name, source, order, visible, enabled, properties) : -1;
    }

    int RegisterIndicatorSimple(const QString &name,
                                const QString &source,
                                int order,
                                bool visible,
                                bool enabled)
    {
        return m_registry ? m_registry->RegisterIndicatorSimple(name, source, order, visible, enabled) : -1;
    }

    bool UnregisterIndicator(int id)
    {
        return m_registry && m_registry->UnregisterIndicator(id);
    }

    void ClearIndicators()
    {
        if (m_registry) {
            m_registry->ClearIndicators();
        }
    }

    QVariantList ListIndicators() const
    {
        return m_registry ? m_registry->ListIndicators() : QVariantList{};
    }

signals:
    void entriesChanged();
    void IndicatorAdded(int id);
    void IndicatorRemoved(int id);

private:
    ExternalIndicatorRegistry *m_registry = nullptr;
};
} // namespace

ExternalIndicatorRegistry::ExternalIndicatorRegistry(QObject *parent)
    : QObject(parent)
{
    m_legacyAdaptor = new LegacyIndicatorRegistryAdaptor(this, this);
    registerDbusService();
}

ExternalIndicatorRegistry::~ExternalIndicatorRegistry()
{
    if (m_serviceRegistered) {
        QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
        QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
    }
    if (m_legacyServiceRegistered) {
        QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kLegacyPath));
        QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kLegacyService));
    }
}

QVariantList ExternalIndicatorRegistry::entries() const
{
    QVariantList out;
    out.reserve(m_entries.size());
    for (const ExternalIndicatorEntry &entry : m_entries) {
        out << toMap(entry);
    }
    return out;
}

bool ExternalIndicatorRegistry::serviceRegistered() const
{
    return m_serviceRegistered;
}

int ExternalIndicatorRegistry::registerIndicator(const QVariantMap &spec)
{
    const QString source = spec.value(QStringLiteral("source")).toString().trimmed();
    if (source.isEmpty()) {
        return -1;
    }

    ExternalIndicatorEntry entry;
    entry.id = m_nextId++;
    entry.name = spec.value(QStringLiteral("name")).toString().trimmed();
    if (entry.name.isEmpty()) {
        entry.name = QStringLiteral("indicator-%1").arg(entry.id);
    }
    entry.source = source;
    entry.order = spec.value(QStringLiteral("order"), 1000).toInt();
    entry.visible = spec.value(QStringLiteral("visible"), true).toBool();
    entry.enabled = spec.value(QStringLiteral("enabled"), true).toBool();
    entry.properties = spec.value(QStringLiteral("properties")).toMap();

    return insertEntry(entry);
}

bool ExternalIndicatorRegistry::unregisterIndicator(int id)
{
    if (id < 0) {
        return false;
    }

    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id != id) {
            continue;
        }
        m_entries.removeAt(i);
        emit entriesChanged();
        emit IndicatorRemoved(id);
        return true;
    }
    return false;
}

void ExternalIndicatorRegistry::clearIndicators()
{
    if (m_entries.isEmpty()) {
        return;
    }
    m_entries.clear();
    emit entriesChanged();
}

int ExternalIndicatorRegistry::RegisterIndicator(const QString &name,
                                                 const QString &source,
                                                 int order,
                                                 bool visible,
                                                 bool enabled,
                                                 const QVariantMap &properties)
{
    QVariantMap spec;
    spec.insert(QStringLiteral("name"), name);
    spec.insert(QStringLiteral("source"), source);
    spec.insert(QStringLiteral("order"), order);
    spec.insert(QStringLiteral("visible"), visible);
    spec.insert(QStringLiteral("enabled"), enabled);
    spec.insert(QStringLiteral("properties"), properties);
    return registerIndicator(spec);
}

bool ExternalIndicatorRegistry::UnregisterIndicator(int id)
{
    return unregisterIndicator(id);
}

int ExternalIndicatorRegistry::RegisterIndicatorSimple(const QString &name,
                                                       const QString &source,
                                                       int order,
                                                       bool visible,
                                                       bool enabled)
{
    return RegisterIndicator(name, source, order, visible, enabled, QVariantMap());
}

void ExternalIndicatorRegistry::ClearIndicators()
{
    clearIndicators();
}

QVariantList ExternalIndicatorRegistry::ListIndicators() const
{
    return entries();
}

void ExternalIndicatorRegistry::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        m_legacyServiceRegistered = false;
        emit serviceRegisteredChanged();
        emit legacyServiceRegisteredChanged();
        return;
    }

    auto iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        m_legacyServiceRegistered = false;
        emit serviceRegisteredChanged();
        emit legacyServiceRegisteredChanged();
        return;
    }

    bool primaryRegistered = false;
    if (!iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        if (bus.registerService(QString::fromLatin1(kService))) {
            primaryRegistered = bus.registerObject(QString::fromLatin1(kPath),
                                                   this,
                                                   QDBusConnection::ExportAllSlots |
                                                       QDBusConnection::ExportAllSignals);
            if (!primaryRegistered) {
                bus.unregisterService(QString::fromLatin1(kService));
            }
        }
    }

    bool legacyRegistered = false;
    if (!iface->isServiceRegistered(QString::fromLatin1(kLegacyService)).value()) {
        if (bus.registerService(QString::fromLatin1(kLegacyService))) {
            legacyRegistered = bus.registerObject(QString::fromLatin1(kLegacyPath),
                                                  m_legacyAdaptor ? m_legacyAdaptor : this,
                                                  QDBusConnection::ExportAllSlots |
                                                      QDBusConnection::ExportAllSignals);
            if (!legacyRegistered) {
                bus.unregisterService(QString::fromLatin1(kLegacyService));
            }
        }
    }

    if (m_serviceRegistered != primaryRegistered) {
        m_serviceRegistered = primaryRegistered;
        emit serviceRegisteredChanged();
    } else {
        m_serviceRegistered = primaryRegistered;
    }
    if (m_legacyServiceRegistered != legacyRegistered) {
        m_legacyServiceRegistered = legacyRegistered;
        emit legacyServiceRegisteredChanged();
    } else {
        m_legacyServiceRegistered = legacyRegistered;
    }
}

int ExternalIndicatorRegistry::insertEntry(const ExternalIndicatorEntry &entry)
{
    m_entries.push_back(entry);
    sortEntries();
    emit entriesChanged();
    emit IndicatorAdded(entry.id);
    return entry.id;
}

void ExternalIndicatorRegistry::sortEntries()
{
    std::sort(m_entries.begin(), m_entries.end(), [](const ExternalIndicatorEntry &a, const ExternalIndicatorEntry &b) {
        if (a.order != b.order) {
            return a.order < b.order;
        }
        return a.name.localeAwareCompare(b.name) < 0;
    });
}

QVariantMap ExternalIndicatorRegistry::toMap(const ExternalIndicatorEntry &entry) const
{
    QVariantMap out;
    out.insert(QStringLiteral("id"), entry.id);
    out.insert(QStringLiteral("name"), entry.name);
    out.insert(QStringLiteral("source"), entry.source);
    out.insert(QStringLiteral("order"), entry.order);
    out.insert(QStringLiteral("visible"), entry.visible);
    out.insert(QStringLiteral("enabled"), entry.enabled);
    out.insert(QStringLiteral("properties"), entry.properties);
    return out;
}

#include "externalindicatorregistry.moc"
