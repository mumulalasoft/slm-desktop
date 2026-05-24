// AppDeck compositor event bus (docs/APPDECK.md §2.2).
// Re-exposes existing shell state as the named events the spec lists, so the
// QML side has a single Connections target instead of scattered listeners
// across ShellPolicyController + WindowingBackend + QGuiApplication.
#pragma once

#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QRect>
#include <QString>

class QScreen;
class ShellPolicyController;
class ShellStateController;

class AppDeckCompositorEvents : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRect screenGeometry READ screenGeometry NOTIFY screenGeometryChanged)
    Q_PROPERTY(qreal scaleFactor READ scaleFactor NOTIFY scaleFactorChanged)
    Q_PROPERTY(bool fullscreenActive READ fullscreenActive NOTIFY fullscreenStateChanged)

public:
    explicit AppDeckCompositorEvents(ShellPolicyController *policy,
                                     ShellStateController *state,
                                     QObject *parent = nullptr);

    QRect screenGeometry() const { return m_screenGeometry; }
    qreal scaleFactor() const { return m_scaleFactor; }
    bool fullscreenActive() const { return m_fullscreenActive; }

    // QML/test-side entry points: QML mouse handlers and WindowingBackend
    // forwarders call into these so this bus is the single AppDeck consumer
    // sees. Keeping these as Q_INVOKABLE rather than direct connect() to the
    // backend keeps the class testable in isolation.
    Q_INVOKABLE void notifyOutsidePointer(const QPointF &globalPos);
    Q_INVOKABLE void notifyAppActivated(const QString &appId);
    Q_INVOKABLE void notifyWorkspaceFocused(int index);

signals:
    void outsidePointerPressed(QPointF globalPos);
    void appActivated(QString appId);
    void workspaceFocused(int workspaceIndex);
    void fullscreenStateChanged(bool active);
    void screenGeometryChanged(QRect geometry);
    void scaleFactorChanged(qreal scale);

private slots:
    void onPolicyChanged();
    void onPrimaryScreenChanged(QScreen *screen);
    void onPrimaryScreenGeometryChanged(const QRect &geometry);
    void onLogicalDotsPerInchChanged(qreal dpi);

private:
    void wireScreen(QScreen *screen);

    QPointer<ShellPolicyController> m_policy;
    QPointer<ShellStateController> m_state;

    QRect m_screenGeometry;
    qreal m_scaleFactor = 1.0;
    bool m_fullscreenActive = false;
    QString m_lastAppId;
    int m_lastWorkspaceIndex = -1;

    QPointer<QScreen> m_currentScreen;
};
