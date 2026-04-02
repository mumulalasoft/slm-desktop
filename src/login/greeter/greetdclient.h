#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>

namespace Slm::Login {

// GreetdClient implements the greetd IPC protocol over a Unix domain socket.
// Wire format: 4-byte little-endian uint32 payload length followed by UTF-8 JSON.
// Socket path is read from $GREETD_SOCK.
class GreetdClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)

public:
    explicit GreetdClient(QObject *parent = nullptr);

    bool isConnected() const { return m_connected; }

    Q_INVOKABLE void createSession(const QString &username);
    Q_INVOKABLE void postAuthResponse(const QString &response);
    Q_INVOKABLE void startSession(const QStringList &cmd, const QVariantMap &env);
    Q_INVOKABLE void cancelSession();

signals:
    void connectedChanged();

    // auth_message_type: "visible" | "secret" | "info" | "error"
    void authMessage(const QString &type, const QString &message);

    // greetd replied with success to the last request.
    void success();

    // greetd replied with an error.
    void error(const QString &errorType, const QString &description);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QLocalSocket::LocalSocketError socketError);

private:
    void sendMessage(const QJsonObject &obj);
    void handleResponse(const QJsonObject &response);
    void tryConnect();

    QLocalSocket m_socket;
    QByteArray   m_readBuf;
    bool         m_connected = false;
};

} // namespace Slm::Login
