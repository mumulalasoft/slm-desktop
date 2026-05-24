#include "cursorcontroller.h"

#include <QGuiApplication>
#include <QCursor>

CursorController::CursorController(QObject *parent)
    : QObject(parent)
{
    m_busyTimer.setSingleShot(true);
    connect(&m_busyTimer, &QTimer::timeout, this, &CursorController::stopBusy);
}

void CursorController::startBusy(int durationMs)
{
    if (!m_busyActive) {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
        m_busyActive = true;
    }

    // duration <= 0 means indefinite busy state until stopBusy() is called.
    if (durationMs <= 0) {
        m_busyTimer.stop();
        return;
    }

    if (durationMs < 150) {
        durationMs = 150;
    }
    m_busyTimer.start(durationMs);
}

void CursorController::stopBusy()
{
    if (!m_busyActive) {
        return;
    }
    m_busyTimer.stop();
    QGuiApplication::restoreOverrideCursor();
    m_busyActive = false;
}
