#pragma once

#include <QObject>
#include <QHash>
#include <QMargins>
#include <QPointer>
#include <QRect>

class QWindow;
class WlrLayerShell;
class AppDeckBootstrapState;

#ifdef SLM_HAVE_LAYERSHELLQT
namespace LayerShellQt {
class Window;
}
#endif

class AppDeckLayerShellController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool supported READ isSupported NOTIFY supportedChanged)

public:
    explicit AppDeckLayerShellController(WlrLayerShell *fallbackLayerShell, QObject *parent = nullptr);

    bool isSupported() const;

    // Hubungkan bootstrap state agar controller dapat mengadvance-nya
    // saat attach via LayerShellQt berhasil (protokol configure/ack tidak tersedia
    // di path LayerShellQt karena ditangani internal oleh Qt).
    void setBootstrapState(AppDeckBootstrapState *state);

    // Konfigurasi LayerShellQt sebelum window pertama kali ditampilkan agar
    // tidak memicu warning "already has a shell integration".
    Q_INVOKABLE void prepareWindow(QWindow *window);
    Q_INVOKABLE void prepareTopBarWindow(QWindow *window);
    Q_INVOKABLE void prepareNotificationWindow(QWindow *window);
    Q_INVOKABLE void prepareSecurityOverlayWindow(QWindow *window);

    // Call this immediately before making the window visible so the grace
    // period starts relative to the actual surface creation, not prepareWindow.
    Q_INVOKABLE void onWindowAboutToShow();

    // Suspend/resume Qt's render loop on a QQuickWindow. Call suspendRendering
    // before showing the window to prevent the second wl_surface.commit (from
    // Qt's render thread) from landing before KWin's ack_configure arrives.
    // Call resumeRendering once the configure/ack cycle is complete (>50ms).
    Q_INVOKABLE void suspendRendering(QWindow *window);
    Q_INVOKABLE void resumeRendering(QWindow *window);

    Q_INVOKABLE bool setDock(QWindow *window,
                             int width,
                             int height,
                             int inputX,
                             int inputY,
                             int inputWidth,
                             int inputHeight);
    Q_INVOKABLE bool setDockAt(QWindow *window,
                               int width,
                               int height,
                               int marginLeft,
                               int marginBottom,
                               int inputX,
                               int inputY,
                               int inputWidth,
                               int inputHeight);
    Q_INVOKABLE bool setGrid(QWindow *window,
                             int width,
                             int height,
                             int inputX,
                             int inputY,
                             int inputWidth,
                             int inputHeight);
    Q_INVOKABLE bool setTopBar(QWindow *window,
                               int width,
                               int height,
                               int inputX,
                               int inputY,
                               int inputWidth,
                               int inputHeight,
                               int exclusiveZone);
    Q_INVOKABLE bool setSecurityOverlay(QWindow *window,
                                        int width,
                                        int height,
                                        int inputX,
                                        int inputY,
                                        int inputWidth,
                                        int inputHeight);
    Q_INVOKABLE bool setNotificationSurface(QWindow *window,
                                            int width,
                                            int height,
                                            int inputX,
                                            int inputY,
                                            int inputWidth,
                                            int inputHeight);

signals:
    void supportedChanged();
    void surfaceReady();

private:
    struct SurfaceState {
        QPointer<QWindow> window;
#ifdef SLM_HAVE_LAYERSHELLQT
        LayerShellQt::Window *layerWindow = nullptr;
#endif
        bool watched = false;
        bool attached = false;
        bool geometrySafe = false;
        bool exposeSeen = false;
        int lastLayer = -1;
        int lastKeyboardInteractivity = -1;
        // Cache of the last *applied* configure parameters (post-grace).
        // Used to short-circuit identical re-invocations from QML retry timers
        // and property change cascades.
        bool hasAppliedConfigure = false;
        int appliedAnchors = -1;
        int appliedExclusiveZone = -1;
        QString appliedScope;
        int appliedWidth = -1;
        int appliedHeight = -1;
        QRect appliedInputRegion;
        QMargins appliedMargins;
    };

    SurfaceState &stateFor(QWindow *window);
    bool configure(QWindow *window,
                   int layer,
                   int keyboardInteractivity,
                   int anchors,
                   int exclusiveZone,
                   const QString &scope,
                   int width,
                   int height,
                   const QRect &inputRegion,
                   const QMargins &explicitMargins = QMargins(),
                   bool useExplicitMargins = false);
    bool applyGeometry(QWindow *window, int width, int height, const QRect &inputRegion);
    bool eventFilter(QObject *obj, QEvent *event) override;
    void startGeometryGrace(QWindow *window);
    void watchWindow(QWindow *window);

    WlrLayerShell *m_fallbackLayerShell = nullptr;
    AppDeckBootstrapState *m_bootstrapState = nullptr;
    QHash<QWindow *, SurfaceState> m_surfaces;
};
