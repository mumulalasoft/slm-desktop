#pragma once

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>

class KWinWaylandIpcClient;
class QTimer;

class KWinWaylandStateModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QVariantMap lastEvent READ lastEvent NOTIFY lastEventChanged)
    Q_PROPERTY(bool switcherActive READ switcherActive NOTIFY switcherChanged)
    Q_PROPERTY(int switcherIndex READ switcherIndex NOTIFY switcherChanged)
    Q_PROPERTY(int switcherCount READ switcherCount NOTIFY switcherChanged)
    Q_PROPERTY(int switcherSelectedSlot READ switcherSelectedSlot NOTIFY switcherChanged)
    Q_PROPERTY(QString switcherViewId READ switcherViewId NOTIFY switcherChanged)
    Q_PROPERTY(QString switcherTitle READ switcherTitle NOTIFY switcherChanged)
    Q_PROPERTY(QString switcherAppId READ switcherAppId NOTIFY switcherChanged)
    Q_PROPERTY(QVariantList switcherEntries READ switcherEntries NOTIFY switcherChanged)
    Q_PROPERTY(int activeSpace READ activeSpace NOTIFY spaceChanged)
    Q_PROPERTY(int spaceCount READ spaceCount NOTIFY spaceChanged)

public:
    explicit KWinWaylandStateModel(QObject *parent = nullptr);

    bool connected() const;
    QVariantMap lastEvent() const;
    bool switcherActive() const;
    int switcherIndex() const;
    int switcherCount() const;
    int switcherSelectedSlot() const;
    QString switcherViewId() const;
    QString switcherTitle() const;
    QString switcherAppId() const;
    QVariantList switcherEntries() const;
    int activeSpace() const;
    int spaceCount() const;

    Q_INVOKABLE QVariantMap windowAt(int index) const;
    Q_INVOKABLE int windowCount() const;
    Q_INVOKABLE void clear();

public slots:
    void setIpcClient(KWinWaylandIpcClient *client);

signals:
    void connectedChanged();
    void lastEventChanged();
    void switcherChanged();
    void spaceChanged();

private:
    struct IconInfo {
        QString iconSource;
        QString iconName;
    };

    void onConnectedChanged();
    void onEventReceived(const QString &event, const QVariantMap &payload);
    void refreshSpaces();
    void refreshWindows();
    void scheduleSpaceRefresh(int delayMs = 0);
    void scheduleWindowRefresh(int delayMs = 0);
    void onSpacePollTick();
    void onWindowPollTick();
    void bindDbusSignals();
    void unbindDbusSignals();
    void reconfigurePollingStrategy();
    int readDesktopCount() const;
    int readActiveDesktop(int desktopCount) const;
    QVariantMap readWindowFromObjectPath(const QString &path);
    QVector<QVariantMap> readWindowsFromObjectModel();
    QVector<QVariantMap> readWindowsFromSupportInformation();
    void requestSupportInformationAsync();
    void onSupportInformationReady(quint64 requestId, const QVector<QVariantMap> &parsed);
    QVariantList parseGeometryList(const QVariant &value);
    QVariantMap parseSupportWindowBlock(const QStringList &block);
    void setWindows(const QVector<QVariantMap> &next);
    void publishSyntheticEvent(const QString &eventName, const QVariantMap &extra = {});
    IconInfo resolveIcon(const QString &appId, const QString &title);
    void ensureIconCache();
    void maybeLogProfileSnapshot();

private slots:
    void onKWinPropertiesChanged(const QString &interfaceName,
                                 const QVariantMap &changedProperties,
                                 const QStringList &invalidatedProperties);
    void onVdmPropertiesChanged(const QString &interfaceName,
                                const QVariantMap &changedProperties,
                                const QStringList &invalidatedProperties);

private:
    KWinWaylandIpcClient *m_ipc = nullptr;
    QVariantMap m_lastEvent;
    bool m_switcherActive = false;
    int m_switcherIndex = 0;
    int m_switcherCount = 0;
    int m_switcherSelectedSlot = 0;
    QString m_switcherViewId;
    QString m_switcherTitle;
    QString m_switcherAppId;
    QVariantList m_switcherEntries;
    int m_activeSpace = 1;
    int m_spaceCount = 1;
    QTimer *m_spaceProbeTimer = nullptr;
    QTimer *m_windowProbeTimer = nullptr;
    QTimer *m_spaceRefreshDebounceTimer = nullptr;
    QTimer *m_windowRefreshDebounceTimer = nullptr;
    QTimer *m_profileLogTimer = nullptr;
    QVector<QVariantMap> m_windows;
    QHash<QString, IconInfo> m_iconByToken;
    bool m_iconCacheReady = false;
    bool m_dbusSignalsBound = false;
    bool m_profileEnabled = false;
    quint64 m_refreshSpacesCount = 0;
    quint64 m_refreshWindowsCount = 0;
    quint64 m_signalTriggerSpaceCount = 0;
    quint64 m_signalTriggerWindowCount = 0;
    quint64 m_pollTriggerSpaceCount = 0;
    quint64 m_pollTriggerWindowCount = 0;
    quint64 m_objectModelHitCount = 0;
    quint64 m_supportFallbackHitCount = 0;
    quint64 m_supportFallbackSkipCount = 0;
    int m_objectModelMissStreak = 0;
    qint64 m_lastSupportFallbackMs = 0;
    int m_supportFallbackBaseIntervalMs = 4500;
    int m_supportFallbackMinIntervalMs = 4500;
    int m_supportFallbackMaxIntervalMs = 18000;
    int m_spacePollIntervalMs = 3200;
    int m_windowPollIntervalMs = 3600;
    bool m_supportRequestInFlight = false;
    quint64 m_supportRequestSeq = 0;
    quint64 m_supportAppliedSeq = 0;
};
