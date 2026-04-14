#pragma once

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVector>
#include <QVariantMap>

class AppExecutionGate;
class ScreenshotManager;
class WorkspaceManager;
class WindowingBackendManager;

class AppCommandRouter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap lastEvent READ lastEvent NOTIFY recentEventsChanged)
    Q_PROPERTY(int eventCount READ eventCount NOTIFY recentEventsChanged)
    Q_PROPERTY(int failureCount READ failureCount NOTIFY recentEventsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY recentEventsChanged)
public:
    explicit AppCommandRouter(AppExecutionGate *gate,
                              ScreenshotManager *screenshotManager = nullptr,
                              QObject *desktopSettings = nullptr,
                              WorkspaceManager *workspaceManager = nullptr,
                              WindowingBackendManager *windowingBackend = nullptr,
                              QObject *parent = nullptr);

    Q_INVOKABLE bool route(const QString &action, const QVariantMap &payload = QVariantMap(),
                           const QString &source = QStringLiteral("router"));
    Q_INVOKABLE QVariantMap routeWithResult(const QString &action,
                                            const QVariantMap &payload = QVariantMap(),
                                            const QString &source = QStringLiteral("router"));
    Q_INVOKABLE QStringList supportedActions() const;
    Q_INVOKABLE bool isSupportedAction(const QString &action) const;
    Q_INVOKABLE bool launchDesktopId(const QString &desktopId,
                                     const QString &source = QStringLiteral("router"));
    Q_INVOKABLE bool openFromFileManager(const QString &targetPathOrUrl);
    Q_INVOKABLE bool execFromTerminal(const QString &command,
                                      const QString &workingDirectory = QString());
    Q_INVOKABLE QVariantList recentEvents() const;
    Q_INVOKABLE QVariantList recentFailures(int maxItems = 20) const;
    Q_INVOKABLE QVariantMap actionStats() const;
    Q_INVOKABLE QVariantMap diagnosticSnapshot() const;
    Q_INVOKABLE QString diagnosticsJson(bool pretty = false) const;
    Q_INVOKABLE void clearRecentEvents();
    QVariantMap lastEvent() const;
    int eventCount() const;
    int failureCount() const;
    QString lastError() const;

signals:
    void routed(QString action, QString source, bool success);
    void routedDetailed(QVariantMap result);
    void recentEventsChanged();

private:
    void recordEvent(const QString &action, const QString &source, bool success,
                     const QString &detail = QString());
    bool verboseLoggingEnabled() const;

    struct RouterEvent {
        QString action;
        QString source;
        bool success = false;
        QString detail;
        qint64 epochMs = 0;
    };

    AppExecutionGate *m_gate = nullptr;
    QObject *m_desktopSettings = nullptr;
    ScreenshotManager *m_screenshotManager = nullptr;
    WorkspaceManager *m_workspaceManager = nullptr;
    WindowingBackendManager *m_windowingBackend = nullptr;
    QVector<RouterEvent> m_recentEvents;
};
