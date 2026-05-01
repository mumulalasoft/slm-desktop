#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>

class QWindow;
class WlrLayerShell;

class AppDeckLayerShellController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool supported READ isSupported NOTIFY supportedChanged)

public:
    explicit AppDeckLayerShellController(WlrLayerShell *fallbackLayerShell, QObject *parent = nullptr);

    bool isSupported() const;

    Q_INVOKABLE bool attach(QWindow *window,
                            int width,
                            int height,
                            int inputX,
                            int inputY,
                            int inputWidth,
                            int inputHeight);
    Q_INVOKABLE bool setDock(QWindow *window,
                                  int width,
                                  int height,
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

signals:
    void supportedChanged();

private:
    bool configure(QWindow *window,
                   int layer,
                   int keyboardInteractivity,
                   int width,
                   int height,
                   const QRect &inputRegion);
    bool applyGeometry(QWindow *window, int width, int height, const QRect &inputRegion);
    void watchWindow(QWindow *window);

    WlrLayerShell *m_fallbackLayerShell = nullptr;
    QPointer<QWindow> m_window;
    bool m_attached = false;
    int m_lastLayer = -1;
    int m_lastKeyboardInteractivity = -1;
};
