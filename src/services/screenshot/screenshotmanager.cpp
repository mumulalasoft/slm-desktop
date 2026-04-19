#include "screenshotmanager.h"

#include <QDateTime>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingReply>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

ScreenshotManager::ScreenshotManager(QObject *parent)
    : QObject(parent)
{
}

void ScreenshotManager::onPortalResponse(uint response, const QVariantMap &results)
{
    m_portalGotResponse = true;
    m_portalResponseCode = response;
    m_portalPayload = results;
    if (m_portalLoop != nullptr) {
        m_portalLoop->quit();
    }
}

QString ScreenshotManager::defaultOutputPath()
{
    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty()) {
        pictures = QDir::homePath() + QStringLiteral("/Pictures");
    }
    const QString targetDir = pictures + QStringLiteral("/Screenshots");
    QDir().mkpath(targetDir);
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    return targetDir + QStringLiteral("/Screenshot-") + ts + QStringLiteral(".png");
}

QString ScreenshotManager::normalizedOutputPath(const QString &path)
{
    QString out = path.trimmed();
    if (out.isEmpty()) {
        out = defaultOutputPath();
    }
    QFileInfo fi(out);
    QDir().mkpath(fi.absolutePath());
    return out;
}

QVariantMap ScreenshotManager::runCapture(const QStringList &args,
                                          const QString &mode,
                                          const QString &targetPath)
{
    (void)args;
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("mode"), mode);
    result.insert(QStringLiteral("path"), targetPath);
    result.insert(QStringLiteral("error"), QStringLiteral("external-capture-disabled"));
    return result;
}

QVariantMap ScreenshotManager::captureViaPortal(const QString &mode,
                                                const QString &targetPath,
                                                bool cropEnabled,
                                                const QRect &cropRect)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("mode"), mode);
    result.insert(QStringLiteral("path"), targetPath);

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.Screenshot"),
        QStringLiteral("Screenshot"));

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"),
                   QStringLiteral("ds_shot_%1").arg(QDateTime::currentMSecsSinceEpoch()));
    options.insert(QStringLiteral("interactive"), false);
    message << QString() << options;

    QDBusPendingReply<QDBusObjectPath> pending = QDBusConnection::sessionBus().call(message);
    pending.waitForFinished();
    if (pending.isError()) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-call-failed"));
        result.insert(QStringLiteral("details"), pending.error().message());
        return result;
    }

    const QString requestPath = pending.value().path();
    if (requestPath.trimmed().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-invalid-handle"));
        return result;
    }

    QVariantMap payload;
    uint responseCode = 2;
    bool gotSignal = false;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    const bool connected = QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        requestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(onPortalResponse(uint,QVariantMap)));
    if (!connected) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-connect-failed"));
        return result;
    }

    m_portalLoop = &loop;
    m_portalGotResponse = false;
    m_portalResponseCode = 2;
    m_portalPayload.clear();
    timer.start(9000);
    loop.exec();
    m_portalLoop = nullptr;

    gotSignal = m_portalGotResponse;
    responseCode = m_portalResponseCode;
    payload = m_portalPayload;

    QDBusConnection::sessionBus().disconnect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        requestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(onPortalResponse(uint,QVariantMap)));

    QDBusMessage closeMsg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        requestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Close"));
    QDBusConnection::sessionBus().call(closeMsg, QDBus::NoBlock);

    if (!gotSignal) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-timeout"));
        return result;
    }
    if (responseCode != 0) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-cancelled"));
        return result;
    }

    const QString uri = payload.value(QStringLiteral("uri")).toString().trimmed();
    const QUrl url(uri);
    const QString sourcePath = url.isLocalFile() ? url.toLocalFile() : QString();
    if (sourcePath.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-missing-uri"));
        return result;
    }

    QImage image(sourcePath);
    if (image.isNull()) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-invalid-image"));
        return result;
    }

    if (cropEnabled) {
        QRect bounded = cropRect.intersected(QRect(0, 0, image.width(), image.height()));
        if (bounded.width() <= 0 || bounded.height() <= 0) {
            result.insert(QStringLiteral("error"), QStringLiteral("portal-invalid-crop"));
            return result;
        }
        image = image.copy(bounded);
    }

    if (!image.save(targetPath, "PNG")) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-save-failed"));
        return result;
    }

    QFileInfo outFi(targetPath);
    if (!outFi.exists() || outFi.size() <= 0) {
        result.insert(QStringLiteral("error"), QStringLiteral("empty-output"));
        return result;
    }
    result.insert(QStringLiteral("ok"), true);
    return result;
}

QVariantMap ScreenshotManager::captureViaCompositor(const QString &command,
                                                    const QString &mode,
                                                    const QString &targetPath)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("mode"), mode);
    result.insert(QStringLiteral("path"), targetPath);

    const QString socketPath = QProcessEnvironment::systemEnvironment()
                                   .value(QStringLiteral("DS_IPC_SOCKET"),
                                          QStringLiteral("/tmp/slm-desktop-compositor.sock"))
                                   .trimmed();
    if (socketPath.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("ipc-socket-missing"));
        return result;
    }

    QLocalSocket socket;
    socket.connectToServer(socketPath);
    if (!socket.waitForConnected(500)) {
        result.insert(QStringLiteral("error"), QStringLiteral("ipc-connect-failed"));
        return result;
    }

    const QByteArray payload = command.toUtf8() + '\n';
    if (socket.write(payload) <= 0 || !socket.waitForBytesWritten(500)) {
        result.insert(QStringLiteral("error"), QStringLiteral("ipc-send-failed"));
        return result;
    }

    QByteArray buffer;
    const qint64 deadlineMs = QDateTime::currentMSecsSinceEpoch() + 2200;
    while (QDateTime::currentMSecsSinceEpoch() < deadlineMs) {
        if (!socket.waitForReadyRead(120)) {
            continue;
        }
        buffer.append(socket.readAll());
        for (;;) {
            const int nl = buffer.indexOf('\n');
            if (nl < 0) {
                break;
            }
            const QByteArray line = buffer.left(nl).trimmed();
            buffer.remove(0, nl + 1);
            if (line.isEmpty()) {
                continue;
            }
            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                continue;
            }
            const QJsonObject obj = doc.object();
            const QString eventName = obj.value(QStringLiteral("event")).toString();
            const QString replyName = obj.value(QStringLiteral("reply")).toString();
            if (eventName != QStringLiteral("screenshot") &&
                replyName != QStringLiteral("screenshot")) {
                continue;
            }
            const QString eventPath = obj.value(QStringLiteral("path")).toString().trimmed();
            if (!eventPath.isEmpty() && eventPath != targetPath) {
                continue;
            }
            const bool ok = obj.value(QStringLiteral("ok")).toBool(false);
            result.insert(QStringLiteral("ok"), ok);
            result.insert(QStringLiteral("error"),
                          obj.value(QStringLiteral("error")).toString().trimmed());
            if (ok) {
                QFileInfo outFi(targetPath);
                if (outFi.exists() && outFi.size() > 0) {
                    return result;
                }
                result.insert(QStringLiteral("ok"), false);
                result.insert(QStringLiteral("error"), QStringLiteral("empty-output"));
                return result;
            }
            return result;
        }
    }

    result.insert(QStringLiteral("error"), QStringLiteral("ipc-timeout"));
    return result;
}

QVariantMap ScreenshotManager::capture(const QString &mode,
                                       int x,
                                       int y,
                                       int width,
                                       int height,
                                       const QString &outputPath)
{
    const QString op = mode.trimmed().toLower();
    if (op == QStringLiteral("fullscreen")) {
        return captureFullscreen(outputPath);
    }
    if (op == QStringLiteral("window")) {
        return captureWindow(x, y, width, height, outputPath);
    }
    if (op == QStringLiteral("area")) {
        if (width > 0 && height > 0) {
            return captureAreaRect(x, y, width, height, outputPath);
        }
        return captureArea(outputPath);
    }

    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("mode"), op);
    result.insert(QStringLiteral("error"), QStringLiteral("unsupported-mode"));
    return result;
}

QVariantMap ScreenshotManager::captureFullscreen(const QString &outputPath)
{
    const QString target = normalizedOutputPath(outputPath);
    const QVariantMap nativeResult = captureViaCompositor(
        QStringLiteral("screenshot fullscreen \"%1\"").arg(target),
        QStringLiteral("fullscreen"),
        target);
    if (nativeResult.value(QStringLiteral("ok")).toBool()) {
        return nativeResult;
    }
    return captureViaPortal(QStringLiteral("fullscreen"), target, false, QRect());
}

QVariantMap ScreenshotManager::captureWindow(int x, int y, int width, int height,
                                             const QString &outputPath)
{
    QVariantMap result;
    const QString target = normalizedOutputPath(outputPath);
    if (width <= 0 || height <= 0) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("mode"), QStringLiteral("window"));
        result.insert(QStringLiteral("path"), target);
        result.insert(QStringLiteral("error"), QStringLiteral("invalid-window-geometry"));
        return result;
    }

    const QVariantMap nativeResult = captureViaCompositor(
        QStringLiteral("screenshot window %1 %2 %3 %4 \"%5\"")
            .arg(x)
            .arg(y)
            .arg(width)
            .arg(height)
            .arg(target),
        QStringLiteral("window"),
        target);
    if (nativeResult.value(QStringLiteral("ok")).toBool()) {
        return nativeResult;
    }
    return captureViaPortal(QStringLiteral("window"),
                            target,
                            true,
                            QRect(x, y, width, height));
}

QVariantMap ScreenshotManager::captureArea(const QString &outputPath)
{
    QVariantMap result;
    const QString target = normalizedOutputPath(outputPath);
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("mode"), QStringLiteral("area"));
    result.insert(QStringLiteral("path"), target);
    result.insert(QStringLiteral("error"), QStringLiteral("area-selection-requires-ui-selector"));
    return result;
}

QVariantMap ScreenshotManager::captureAreaRect(int x, int y, int width, int height,
                                               const QString &outputPath)
{
    QVariantMap result;
    const QString target = normalizedOutputPath(outputPath);
    if (width <= 0 || height <= 0) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("mode"), QStringLiteral("area"));
        result.insert(QStringLiteral("path"), target);
        result.insert(QStringLiteral("error"), QStringLiteral("invalid-area-geometry"));
        return result;
    }

    const QVariantMap nativeResult = captureViaCompositor(
        QStringLiteral("screenshot area %1 %2 %3 %4 \"%5\"")
            .arg(x)
            .arg(y)
            .arg(width)
            .arg(height)
            .arg(target),
        QStringLiteral("area"),
        target);
    if (nativeResult.value(QStringLiteral("ok")).toBool()) {
        return nativeResult;
    }
    return captureViaPortal(QStringLiteral("area"),
                            target,
                            true,
                            QRect(x, y, width, height));
}
