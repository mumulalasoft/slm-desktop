#pragma once

#include <QObject>
#include <QString>

namespace Slm::Login {

class AuthSession : public QObject
{
    Q_OBJECT

public:
    explicit AuthSession(QObject *parent = nullptr);
    ~AuthSession() override;

    bool start(void *identity, const QString &cookie, QString *error = nullptr);
    void cancel();
    void respondPassword(const QString &password);
    void handleBackendCompleted(bool gainedAuthorization);

    bool isActive() const;

Q_SIGNALS:
    void request(QString prompt, bool echoOn);
    void showError(QString message);
    void showInfo(QString message);
    void completed(bool gainedAuthorization);

private:
    void clearSession();

    void *m_session = nullptr;
    bool m_active = false;
};

} // namespace Slm::Login
