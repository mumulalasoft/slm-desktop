#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class ShellStateController : public QObject
{
    Q_OBJECT
    // No QML_ELEMENT: exposed via context property "ShellStateController".
    // QML type registration causes the qmltyperegistrar to emit __has_include
    // guards using bare filenames that fail unless src/core/shell is explicitly
    // on the compiler search path — fragile across cmake invocations. Context
    // properties work fully (bindings, signals, invokables) without registration.

    Q_PROPERTY(bool launchpadVisible READ launchpadVisible WRITE setLaunchpadVisible NOTIFY launchpadVisibleChanged)
    Q_PROPERTY(bool workspaceOverviewVisible READ workspaceOverviewVisible WRITE setWorkspaceOverviewVisible NOTIFY workspaceOverviewVisibleChanged)
    Q_PROPERTY(bool toTheSpotVisible READ toTheSpotVisible WRITE setToTheSpotVisible NOTIFY toTheSpotVisibleChanged)
    // searchVisible is an explicit compatibility alias for toTheSpotVisible.
    Q_PROPERTY(bool searchVisible READ toTheSpotVisible WRITE setToTheSpotVisible NOTIFY searchVisibleChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(bool styleGalleryVisible READ styleGalleryVisible WRITE setStyleGalleryVisible NOTIFY styleGalleryVisibleChanged)
    Q_PROPERTY(bool showDesktop READ showDesktop WRITE setShowDesktop NOTIFY showDesktopChanged)
    Q_PROPERTY(bool lockScreenActive READ lockScreenActive WRITE setLockScreenActive NOTIFY lockScreenActiveChanged)
    Q_PROPERTY(bool notificationsVisible READ notificationsVisible WRITE setNotificationsVisible NOTIFY notificationsVisibleChanged)
    Q_PROPERTY(bool focusMode READ focusMode WRITE setFocusMode NOTIFY focusModeChanged)
    Q_PROPERTY(QString dockHoveredItem READ dockHoveredItem WRITE setDockHoveredItem NOTIFY dockHoveredItemChanged)
    Q_PROPERTY(QString dockExpandedItem READ dockExpandedItem WRITE setDockExpandedItem NOTIFY dockExpandedItemChanged)
    Q_PROPERTY(QVariantMap dragSession READ dragSession WRITE setDragSession NOTIFY dragSessionChanged)

    // Derived state — read-only from QML
    Q_PROPERTY(qreal topBarOpacity READ topBarOpacity NOTIFY topBarOpacityChanged)
    Q_PROPERTY(qreal dockOpacity READ dockOpacity NOTIFY dockOpacityChanged)
    Q_PROPERTY(bool workspaceBlurred READ workspaceBlurred NOTIFY workspaceBlurredChanged)
    Q_PROPERTY(qreal workspaceBlurAlpha READ workspaceBlurAlpha NOTIFY workspaceBlurAlphaChanged)
    Q_PROPERTY(bool workspaceInteractionBlocked READ workspaceInteractionBlocked NOTIFY workspaceInteractionBlockedChanged)
    Q_PROPERTY(bool anyOverlayVisible READ anyOverlayVisible NOTIFY anyOverlayVisibleChanged)

public:
    explicit ShellStateController(QObject *parent = nullptr);

    bool launchpadVisible() const;
    bool workspaceOverviewVisible() const;
    bool toTheSpotVisible() const;
    QString searchQuery() const;
    bool styleGalleryVisible() const;
    bool showDesktop() const;
    bool lockScreenActive() const;
    bool notificationsVisible() const;
    bool focusMode() const;
    QString dockHoveredItem() const;
    QString dockExpandedItem() const;
    QVariantMap dragSession() const;

    qreal topBarOpacity() const;
    qreal dockOpacity() const;
    bool workspaceBlurred() const;
    qreal workspaceBlurAlpha() const;
    bool workspaceInteractionBlocked() const;
    bool anyOverlayVisible() const;

    Q_INVOKABLE void setLaunchpadVisible(bool visible);
    Q_INVOKABLE void setWorkspaceOverviewVisible(bool visible);
    Q_INVOKABLE void setToTheSpotVisible(bool visible);
    Q_INVOKABLE void setSearchQuery(const QString &query);
    Q_INVOKABLE void setStyleGalleryVisible(bool visible);
    Q_INVOKABLE void setShowDesktop(bool active);
    Q_INVOKABLE void setLockScreenActive(bool active);
    Q_INVOKABLE void setNotificationsVisible(bool visible);
    Q_INVOKABLE void setFocusMode(bool active);
    Q_INVOKABLE void setDockHoveredItem(const QString &itemId);
    Q_INVOKABLE void setDockExpandedItem(const QString &itemId);
    Q_INVOKABLE void setDragSession(const QVariantMap &session);
    Q_INVOKABLE void clearDragSession();

    Q_INVOKABLE void toggleLaunchpad();
    Q_INVOKABLE void toggleWorkspaceOverview();
    Q_INVOKABLE void toggleToTheSpot();
    Q_INVOKABLE void dismissAllOverlays();

signals:
    void launchpadVisibleChanged(bool visible);
    void workspaceOverviewVisibleChanged(bool visible);
    void toTheSpotVisibleChanged(bool visible);
    void searchVisibleChanged(bool visible);
    void searchQueryChanged(const QString &query);
    void styleGalleryVisibleChanged(bool visible);
    void showDesktopChanged(bool active);
    void lockScreenActiveChanged(bool active);
    void notificationsVisibleChanged(bool visible);
    void focusModeChanged(bool active);
    void dockHoveredItemChanged(const QString &itemId);
    void dockExpandedItemChanged(const QString &itemId);
    void dragSessionChanged(const QVariantMap &session);

    void topBarOpacityChanged(qreal opacity);
    void dockOpacityChanged(qreal opacity);
    void workspaceBlurredChanged(bool blurred);
    void workspaceBlurAlphaChanged(qreal alpha);
    void workspaceInteractionBlockedChanged(bool blocked);
    void anyOverlayVisibleChanged(bool visible);

private:
    void recomputeDerivedState();

    bool m_launchpadVisible = false;
    bool m_workspaceOverviewVisible = false;
    bool m_toTheSpotVisible = false;
    QString m_searchQuery;
    bool m_styleGalleryVisible = false;
    bool m_showDesktop = false;
    bool m_lockScreenActive = false;
    bool m_notificationsVisible = false;
    bool m_focusMode = false;
    QString m_dockHoveredItem;
    QString m_dockExpandedItem;
    QVariantMap m_dragSession;

    // Cached derived values
    qreal m_topBarOpacity = 1.0;
    qreal m_dockOpacity = 1.0;
    bool m_workspaceBlurred = false;
    qreal m_workspaceBlurAlpha = 0.0;
    bool m_workspaceInteractionBlocked = false;
    bool m_anyOverlayVisible = false;
};
