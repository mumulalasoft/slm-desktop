#pragma once
#include <QObject>
#include <QTimer>

namespace Slm::Login {

class SessionWatchdog : public QObject
{
    Q_OBJECT
public:
    explicit SessionWatchdog(QObject *parent = nullptr);

private slots:
    void onHealthyTimeout();

private:
    void markSessionHealthy();
    QTimer m_timer;
};

} // namespace Slm::Login
