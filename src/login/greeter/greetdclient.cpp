#include "greetdclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>

namespace Slm::Login {

GreetdClient::GreetdClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QLocalSocket::connected,
            this, &GreetdClient::onConnected);
    connect(&m_socket, &QLocalSocket::disconnected,
            this, &GreetdClient::onDisconnected);
    connect(&m_socket, &QLocalSocket::readyRead,
            this, &GreetdClient::onReadyRead);
    connect(&m_socket, &QLocalSocket::errorOccurred,
            this, &GreetdClient::onSocketError);
    tryConnect();
}

void GreetdClient::tryConnect()
{
    const QString sockPath = QString::fromLocal8Bit(qgetenv("GREETD_SOCK"));
    if (sockPath.isEmpty()) {
        qWarning("slm-greeter: GREETD_SOCK is not set — running without greetd");
        return;
    }
    m_socket.connectToServer(sockPath);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void GreetdClient::onConnected()
{
    m_connected = true;
    emit connectedChanged();
    qInfo("slm-greeter: connected to greetd");
}

void GreetdClient::onDisconnected()
{
    m_connected = false;
    emit connectedChanged();
    qWarning("slm-greeter: disconnected from greetd");
}

void GreetdClient::onSocketError(QLocalSocket::LocalSocketError socketError)
{
    qWarning("slm-greeter: socket error %d: %s",
             static_cast<int>(socketError),
             qUtf8Printable(m_socket.errorString()));
}

void GreetdClient::onReadyRead()
{
    m_readBuf.append(m_socket.readAll());

    while (m_readBuf.size() >= 4) {
        // Read 4-byte little-endian payload length (greetd IPC framing).
        quint32 payloadLen = 0;
        memcpy(&payloadLen, m_readBuf.constData(), 4);
        payloadLen = qFromLittleEndian(payloadLen);

        if (static_cast<int>(m_readBuf.size()) < 4 + static_cast<int>(payloadLen)) {
            break; // wait for more data
        }

        const QByteArray payload = m_readBuf.mid(4, static_cast<int>(payloadLen));
        m_readBuf.remove(0, 4 + static_cast<int>(payloadLen));

        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseErr);
        if (doc.isNull() || !doc.isObject()) {
            qWarning("slm-greeter: bad JSON from greetd: %s",
                     qUtf8Printable(parseErr.errorString()));
            continue;
        }
        handleResponse(doc.object());
    }
}

// ── Protocol send ─────────────────────────────────────────────────────────────

void GreetdClient::sendMessage(const QJsonObject &obj)
{
    if (!m_connected) {
        qWarning("slm-greeter: sendMessage called while not connected");
        return;
    }
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    const quint32 len        = static_cast<quint32>(payload.size());
    quint32 lenLE            = qToLittleEndian(len);

    QByteArray frame;
    frame.resize(4);
    memcpy(frame.data(), &lenLE, 4);
    frame.append(payload);
    m_socket.write(frame);
    m_socket.flush();
}

// ── Public API ────────────────────────────────────────────────────────────────

void GreetdClient::createSession(const QString &username)
{
    sendMessage({
        {QStringLiteral("type"),     QStringLiteral("create_session")},
        {QStringLiteral("username"), username},
    });
}

void GreetdClient::postAuthResponse(const QString &response)
{
    sendMessage({
        {QStringLiteral("type"),     QStringLiteral("post_auth_message_response")},
        {QStringLiteral("response"), response},
    });
}

void GreetdClient::startSession(const QStringList &cmd, const QVariantMap &env)
{
    QJsonArray cmdArray;
    for (const QString &s : cmd) cmdArray.append(s);

    QJsonArray envArray;
    for (auto it = env.constBegin(); it != env.constEnd(); ++it) {
        envArray.append(it.key() + QStringLiteral("=") + it.value().toString());
    }

    sendMessage({
        {QStringLiteral("type"), QStringLiteral("start_session")},
        {QStringLiteral("cmd"),  cmdArray},
        {QStringLiteral("env"),  envArray},
    });
}

void GreetdClient::cancelSession()
{
    sendMessage({{QStringLiteral("type"), QStringLiteral("cancel_session")}});
}

// ── Response handling ─────────────────────────────────────────────────────────

void GreetdClient::handleResponse(const QJsonObject &response)
{
    const QString type = response.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("auth_message")) {
        emit authMessage(
            response.value(QStringLiteral("auth_message_type")).toString(),
            response.value(QStringLiteral("auth_message")).toString());
    } else if (type == QStringLiteral("success")) {
        emit success();
    } else if (type == QStringLiteral("error")) {
        emit error(
            response.value(QStringLiteral("error_type")).toString(),
            response.value(QStringLiteral("description")).toString());
    } else {
        qWarning("slm-greeter: unknown greetd response type: %s", qUtf8Printable(type));
    }
}

} // namespace Slm::Login
