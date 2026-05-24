#pragma once

#include <QObject>
#include <QTimer>

class CursorController : public QObject {
    Q_OBJECT
public:
    explicit CursorController(QObject *parent = nullptr);

    Q_INVOKABLE void startBusy(int durationMs = 1200);
    Q_INVOKABLE void stopBusy();

private:
    QTimer m_busyTimer;
    bool m_busyActive = false;
};
