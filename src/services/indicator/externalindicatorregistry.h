#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct ExternalIndicatorEntry {
    int id = -1;
    QString name;
    QString source;
    int order = 1000;
    bool visible = true;
    bool enabled = true;
    QVariantMap properties;
};

class ExternalIndicatorRegistry : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.IndicatorRegistry")
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit ExternalIndicatorRegistry(QObject *parent = nullptr);
    ~ExternalIndicatorRegistry() override;

    QVariantList entries() const;
    bool serviceRegistered() const;

    Q_INVOKABLE int registerIndicator(const QVariantMap &spec);
    Q_INVOKABLE bool unregisterIndicator(int id);
    Q_INVOKABLE void clearIndicators();

public slots:
    int RegisterIndicator(const QString &name,
                          const QString &source,
                          int order,
                          bool visible,
                          bool enabled,
                          const QVariantMap &properties);
    int RegisterIndicatorSimple(const QString &name,
                                const QString &source,
                                int order,
                                bool visible,
                                bool enabled);
    bool UnregisterIndicator(int id);
    void ClearIndicators();
    QVariantList ListIndicators() const;

signals:
    void entriesChanged();
    void serviceRegisteredChanged();
    void legacyServiceRegisteredChanged();
    void IndicatorAdded(int id);
    void IndicatorRemoved(int id);

private:
    void registerDbusService();
    int insertEntry(const ExternalIndicatorEntry &entry);
    void sortEntries();
    QVariantMap toMap(const ExternalIndicatorEntry &entry) const;

    QVector<ExternalIndicatorEntry> m_entries;
    int m_nextId = 1;
    bool m_serviceRegistered = false;
    bool m_legacyServiceRegistered = false;
    QObject *m_legacyAdaptor = nullptr;
};
