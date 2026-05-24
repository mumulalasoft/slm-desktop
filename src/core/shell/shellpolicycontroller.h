#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class ShellStateController;
class WindowingBackendManager;

class ShellPolicyController : public QObject
{
    Q_OBJECT
    // No QML_ELEMENT: exposed via context property "ShellPolicyController".

    Q_PROPERTY(VisibilityPolicy visibilityPolicy READ visibilityPolicy NOTIFY policyStateChanged)
    Q_PROPERTY(QString visibilityPolicyName READ visibilityPolicyName NOTIFY policyStateChanged)
    Q_PROPERTY(bool fullscreenActive READ fullscreenActive NOTIFY policyStateChanged)
    Q_PROPERTY(bool immersiveFullscreen READ immersiveFullscreen NOTIFY policyStateChanged)
    Q_PROPERTY(bool presentationActive READ presentationActive NOTIFY policyStateChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY policyStateChanged)
    Q_PROPERTY(QString focusedAppId READ focusedAppId NOTIFY policyStateChanged)
    Q_PROPERTY(QString focusedWindowTitle READ focusedWindowTitle NOTIFY policyStateChanged)
    Q_PROPERTY(QVariantMap focusedWindow READ focusedWindow NOTIFY policyStateChanged)

    Q_PROPERTY(bool crownSurfaceVisible READ crownSurfaceVisible NOTIFY surfacePolicyChanged)
    Q_PROPERTY(bool crownContentVisible READ crownContentVisible NOTIFY surfacePolicyChanged)
    Q_PROPERTY(bool appDeckSurfaceVisible READ appDeckSurfaceVisible NOTIFY surfacePolicyChanged)
    Q_PROPERTY(bool appDeckContentVisible READ appDeckContentVisible NOTIFY surfacePolicyChanged)
    Q_PROPERTY(bool notificationsAllowed READ notificationsAllowed NOTIFY surfacePolicyChanged)
    Q_PROPERTY(bool notificationsSuppressed READ notificationsSuppressed NOTIFY surfacePolicyChanged)
    Q_PROPERTY(bool compactNotifications READ compactNotifications NOTIFY surfacePolicyChanged)
    Q_PROPERTY(bool edgeRevealEnabled READ edgeRevealEnabled NOTIFY surfacePolicyChanged)
    Q_PROPERTY(bool topEdgeRevealActive READ topEdgeRevealActive NOTIFY surfacePolicyChanged)
    Q_PROPERTY(bool bottomEdgeRevealActive READ bottomEdgeRevealActive NOTIFY surfacePolicyChanged)
    Q_PROPERTY(int edgeRevealSize READ edgeRevealSize WRITE setEdgeRevealSize NOTIFY edgeRevealSizeChanged)
    Q_PROPERTY(int Normal READ normalPolicy CONSTANT)
    Q_PROPERTY(int AppFullscreen READ appFullscreenPolicy CONSTANT)
    Q_PROPERTY(int ImmersiveFullscreen READ immersiveFullscreenPolicy CONSTANT)
    Q_PROPERTY(int Presentation READ presentationPolicy CONSTANT)
    Q_PROPERTY(int Locked READ lockedPolicy CONSTANT)

public:
    enum VisibilityPolicy {
        Normal = 0,
        AppFullscreen,
        ImmersiveFullscreen,
        Presentation,
        Locked
    };
    Q_ENUM(VisibilityPolicy)

    explicit ShellPolicyController(QObject *parent = nullptr);
    ShellPolicyController(ShellStateController *stateController,
                          WindowingBackendManager *windowingBackend,
                          QObject *parent = nullptr);

    void setStateController(ShellStateController *stateController);
    void setWindowingBackend(WindowingBackendManager *windowingBackend);

    VisibilityPolicy visibilityPolicy() const;
    QString visibilityPolicyName() const;
    bool fullscreenActive() const;
    bool immersiveFullscreen() const;
    bool presentationActive() const;
    bool locked() const;
    QString focusedAppId() const;
    QString focusedWindowTitle() const;
    QVariantMap focusedWindow() const;

    bool crownSurfaceVisible() const;
    bool crownContentVisible() const;
    bool appDeckSurfaceVisible() const;
    bool appDeckContentVisible() const;
    bool notificationsAllowed() const;
    bool notificationsSuppressed() const;
    bool compactNotifications() const;
    bool edgeRevealEnabled() const;
    bool topEdgeRevealActive() const;
    bool bottomEdgeRevealActive() const;
    int edgeRevealSize() const;
    void setEdgeRevealSize(int size);
    int normalPolicy() const;
    int appFullscreenPolicy() const;
    int immersiveFullscreenPolicy() const;
    int presentationPolicy() const;
    int lockedPolicy() const;

    Q_INVOKABLE QString policyName(int policy) const;
    Q_INVOKABLE void setWindowSnapshot(const QVariantList &windows,
                                       const QString &reason = QStringLiteral("snapshot"));
    Q_INVOKABLE void requestTopEdgeReveal(const QString &reason = QStringLiteral("pointer"));
    Q_INVOKABLE void requestBottomEdgeReveal(const QString &reason = QStringLiteral("pointer"));
    Q_INVOKABLE void clearReveal(const QString &reason = QStringLiteral("timeout"));

signals:
    void policyStateChanged();
    void surfacePolicyChanged();
    void edgeRevealSizeChanged();

private slots:
    void refreshFromCompositorState();

private:
    struct Evaluation {
        VisibilityPolicy policy = Normal;
        bool fullscreen = false;
        QVariantMap focusedWindow;
        QString appId;
        QString title;
    };

    Evaluation evaluateWindows(const QVariantList &windows) const;
    VisibilityPolicy classifyFullscreenIntent(const QVariantMap &window) const;
    bool isFullscreenWindow(const QVariantMap &window) const;
    bool isMappedCandidate(const QVariantMap &window) const;
    bool isShellWindow(const QVariantMap &window) const;
    bool appIdContainsAny(const QString &appId, const QStringList &tokens) const;
    void applyEvaluation(const Evaluation &evaluation, const QString &reason);
    void setVisibilityPolicy(VisibilityPolicy policy,
                             bool fullscreen,
                             const QVariantMap &focusedWindow,
                             const QString &appId,
                             const QString &title,
                             const QString &reason);
    void clearRevealInternal(const QString &reason, bool emitChanges);
    void restartRevealTimeout();
    void logLayerState(const QString &reason) const;

    QPointer<ShellStateController> m_stateController;
    QPointer<WindowingBackendManager> m_windowingBackend;
    QPointer<QObject> m_compositorState;
    QVariantList m_windows;
    QVariantMap m_focusedWindow;
    VisibilityPolicy m_policy = Normal;
    bool m_fullscreenActive = false;
    QString m_focusedAppId;
    QString m_focusedWindowTitle;
    bool m_topEdgeRevealActive = false;
    bool m_bottomEdgeRevealActive = false;
    int m_edgeRevealSize = 6;
    int m_revealTimeoutMs = 1600;
    QTimer m_revealTimer;
};
