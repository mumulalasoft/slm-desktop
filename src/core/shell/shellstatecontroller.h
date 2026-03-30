#pragma once

#include <QObject>
#include <QtQml/qqml.h>

class ShellStateController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ShellStateController is instantiated in C++ and exposed as a context property")

    Q_PROPERTY(bool launchpadVisible READ launchpadVisible WRITE setLaunchpadVisible NOTIFY launchpadVisibleChanged)
    Q_PROPERTY(bool workspaceOverviewVisible READ workspaceOverviewVisible WRITE setWorkspaceOverviewVisible NOTIFY workspaceOverviewVisibleChanged)
    Q_PROPERTY(bool toTheSpotVisible READ toTheSpotVisible WRITE setToTheSpotVisible NOTIFY toTheSpotVisibleChanged)
    Q_PROPERTY(bool styleGalleryVisible READ styleGalleryVisible WRITE setStyleGalleryVisible NOTIFY styleGalleryVisibleChanged)
    Q_PROPERTY(bool showDesktop READ showDesktop WRITE setShowDesktop NOTIFY showDesktopChanged)

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
    bool styleGalleryVisible() const;
    bool showDesktop() const;

    qreal topBarOpacity() const;
    qreal dockOpacity() const;
    bool workspaceBlurred() const;
    qreal workspaceBlurAlpha() const;
    bool workspaceInteractionBlocked() const;
    bool anyOverlayVisible() const;

    Q_INVOKABLE void setLaunchpadVisible(bool visible);
    Q_INVOKABLE void setWorkspaceOverviewVisible(bool visible);
    Q_INVOKABLE void setToTheSpotVisible(bool visible);
    Q_INVOKABLE void setStyleGalleryVisible(bool visible);
    Q_INVOKABLE void setShowDesktop(bool active);

    Q_INVOKABLE void toggleLaunchpad();
    Q_INVOKABLE void toggleWorkspaceOverview();
    Q_INVOKABLE void toggleToTheSpot();
    Q_INVOKABLE void dismissAllOverlays();

signals:
    void launchpadVisibleChanged(bool visible);
    void workspaceOverviewVisibleChanged(bool visible);
    void toTheSpotVisibleChanged(bool visible);
    void styleGalleryVisibleChanged(bool visible);
    void showDesktopChanged(bool active);

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
    bool m_styleGalleryVisible = false;
    bool m_showDesktop = false;

    // Cached derived values
    qreal m_topBarOpacity = 1.0;
    qreal m_dockOpacity = 1.0;
    bool m_workspaceBlurred = false;
    qreal m_workspaceBlurAlpha = 0.0;
    bool m_workspaceInteractionBlocked = false;
    bool m_anyOverlayVisible = false;
};
