#include "screenshotmanager.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusUnixFileDescriptor>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QPainter>
#include <QPixmap>
#include <QProcessEnvironment>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

ScreenshotManager::ScreenshotManager(QObject *parent)
    : QObject(parent)
{
}

void ScreenshotManager::setLockedProvider(std::function<bool()> provider)
{
    m_lockedProvider = std::move(provider);
}

bool ScreenshotManager::isSessionLocked() const
{
    return m_lockedProvider && m_lockedProvider();
}

QVariantMap ScreenshotManager::lockedResult(const QString &mode)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("mode"), mode);
    result.insert(QStringLiteral("error"), QStringLiteral("session-locked"));
    return result;
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

    const QDBusMessage reply = QDBusConnection::sessionBus().call(message,
                                                                  QDBus::BlockWithGui,
                                                                  3000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-call-failed"));
        result.insert(QStringLiteral("details"), reply.errorMessage());
        return result;
    }
    if (reply.arguments().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("portal-invalid-handle"));
        return result;
    }

    const QDBusObjectPath requestObject = qdbus_cast<QDBusObjectPath>(reply.arguments().constFirst());
    const QString requestPath = requestObject.path();
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

QVariantMap ScreenshotManager::captureViaScreenGrab(const QString &mode,
                                                    const QString &targetPath,
                                                    bool cropEnabled,
                                                    const QRect &cropRect)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("mode"), mode);
    result.insert(QStringLiteral("path"), targetPath);

    if (qGuiApp != nullptr && QGuiApplication::platformName() == QStringLiteral("wayland")) {
        result.insert(QStringLiteral("error"), QStringLiteral("qt-grab-unsupported-wayland"));
        return result;
    }

    if (qGuiApp == nullptr) {
        result.insert(QStringLiteral("error"), QStringLiteral("qt-gui-unavailable"));
        return result;
    }

    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("screen-unavailable"));
        return result;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (cropEnabled) {
        const QPoint center = cropRect.center();
        if (QScreen *targetScreen = QGuiApplication::screenAt(center)) {
            screen = targetScreen;
        }
    }
    if (!screen) {
        screen = screens.constFirst();
    }

    QRect grabRect;
    if (cropEnabled) {
        grabRect = cropRect.intersected(screen->geometry());
        if (grabRect.width() <= 0 || grabRect.height() <= 0) {
            result.insert(QStringLiteral("error"), QStringLiteral("invalid-crop"));
            return result;
        }
    } else {
        grabRect = screen->geometry();
    }

    const QRect localRect = grabRect.translated(-screen->geometry().topLeft());
    QPixmap pixmap = screen->grabWindow(0,
                                        localRect.x(),
                                        localRect.y(),
                                        localRect.width(),
                                        localRect.height());
    if (pixmap.isNull()) {
        result.insert(QStringLiteral("error"), QStringLiteral("qt-grab-failed"));
        return result;
    }

    if (!pixmap.save(targetPath, "PNG")) {
        result.insert(QStringLiteral("error"), QStringLiteral("qt-save-failed"));
        return result;
    }

    const QFileInfo outFi(targetPath);
    if (!outFi.exists() || outFi.size() <= 0) {
        result.insert(QStringLiteral("error"), QStringLiteral("empty-output"));
        return result;
    }

    result.insert(QStringLiteral("ok"), true);
    return result;
}

QVariantMap ScreenshotManager::captureViaKWinScreenShot2(const QString &mode,
                                                         const QString &targetPath,
                                                         const QRect &captureRect)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("mode"), mode);
    result.insert(QStringLiteral("path"), targetPath);

    if (captureRect.width() <= 0 || captureRect.height() <= 0) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-invalid-geometry"));
        return result;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-dbus-unavailable"));
        return result;
    }
    const QDBusReply<bool> serviceReply =
        bus.interface()->isServiceRegistered(QStringLiteral("org.kde.KWin"));
    if (!serviceReply.isValid() || !serviceReply.value()) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-unavailable"));
        return result;
    }

    QTemporaryFile rawFile(QDir::tempPath() + QStringLiteral("/slm-kwin-shot-XXXXXX.raw"));
    rawFile.setAutoRemove(true);
    if (!rawFile.open()) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-tempfile-failed"));
        return result;
    }

    QVariantMap options;
    options.insert(QStringLiteral("include-cursor"), false);
    options.insert(QStringLiteral("native-resolution"), false);

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/org/kde/KWin/ScreenShot2"),
        QStringLiteral("org.kde.KWin.ScreenShot2"),
        QStringLiteral("CaptureArea"));
    message << captureRect.x()
            << captureRect.y()
            << static_cast<uint>(captureRect.width())
            << static_cast<uint>(captureRect.height())
            << options
            << QVariant::fromValue(QDBusUnixFileDescriptor(rawFile.handle()));

    const QDBusMessage reply = bus.call(message, QDBus::BlockWithGui, 3000);
    rawFile.flush();
    if (reply.type() == QDBusMessage::ErrorMessage) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-capture-failed"));
        result.insert(QStringLiteral("details"), reply.errorMessage());
        return result;
    }
    if (reply.arguments().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-empty-reply"));
        return result;
    }

    const QVariantMap meta = qdbus_cast<QVariantMap>(reply.arguments().constFirst());
    if (meta.value(QStringLiteral("type")).toString() != QStringLiteral("raw")) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-unsupported-type"));
        return result;
    }
    const int width = meta.value(QStringLiteral("width")).toInt();
    const int height = meta.value(QStringLiteral("height")).toInt();
    const int stride = meta.value(QStringLiteral("stride")).toInt();
    bool formatOk = false;
    const int formatValue = meta.value(QStringLiteral("format")).toInt(&formatOk);
    if (width <= 0 || height <= 0 || stride <= 0 || !formatOk || formatValue < 0) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-invalid-metadata"));
        return result;
    }

    rawFile.seek(0);
    const QByteArray bytes = rawFile.readAll();
    const qsizetype expectedMin = static_cast<qsizetype>(stride) * height;
    if (bytes.size() < expectedMin) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-short-read"));
        return result;
    }

    const QImage image(reinterpret_cast<const uchar *>(bytes.constData()),
                       width,
                       height,
                       stride,
                       static_cast<QImage::Format>(formatValue));
    if (image.isNull()) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-invalid-image"));
        return result;
    }
    if (!image.copy().save(targetPath, "PNG")) {
        result.insert(QStringLiteral("error"), QStringLiteral("kwin-save-failed"));
        return result;
    }

    const QFileInfo outFi(targetPath);
    if (!outFi.exists() || outFi.size() <= 0) {
        result.insert(QStringLiteral("error"), QStringLiteral("empty-output"));
        return result;
    }

    result.insert(QStringLiteral("ok"), true);
    return result;
}

QVariantMap ScreenshotManager::captureViaOwnWindows(const QString &mode,
                                                    const QString &targetPath,
                                                    bool cropEnabled,
                                                    const QRect &cropRect)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("mode"), mode);
    result.insert(QStringLiteral("path"), targetPath);

    if (qGuiApp == nullptr) {
        result.insert(QStringLiteral("error"), QStringLiteral("qt-gui-unavailable"));
        return result;
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        result.insert(QStringLiteral("error"), QStringLiteral("screen-unavailable"));
        return result;
    }

    const QRect screenRect = screen->geometry();
    const QRect targetRect = cropEnabled ? cropRect.intersected(screenRect) : screenRect;
    if (targetRect.width() <= 0 || targetRect.height() <= 0) {
        result.insert(QStringLiteral("error"), QStringLiteral("invalid-crop"));
        return result;
    }

    QQuickWindow *captureWindow = nullptr;
    QRect captureRect;
    qsizetype captureArea = 0;
    const QList<QWindow *> windows = QGuiApplication::topLevelWindows();
    for (QWindow *window : windows) {
        auto *quickWindow = qobject_cast<QQuickWindow *>(window);
        if (!quickWindow || !quickWindow->isVisible() || !quickWindow->isExposed()
            || quickWindow->width() <= 0 || quickWindow->height() <= 0) {
            continue;
        }
        const Qt::WindowType type = quickWindow->type();
        if (type == Qt::Popup || type == Qt::ToolTip || type == Qt::SplashScreen) {
            continue;
        }

        const QRect windowRect(quickWindow->x(), quickWindow->y(),
                               quickWindow->width(), quickWindow->height());
        const QRect drawRect = windowRect.intersected(targetRect);
        if (drawRect.isEmpty()) {
            continue;
        }
        const qsizetype area = qsizetype(drawRect.width()) * qsizetype(drawRect.height());
        if (area <= captureArea) {
            continue;
        }
        captureWindow = quickWindow;
        captureRect = windowRect;
        captureArea = area;
    }

    if (!captureWindow) {
        result.insert(QStringLiteral("error"), QStringLiteral("qt-own-window-unavailable"));
        return result;
    }

    qInfo().noquote() << QStringLiteral("[slm][screenshot] own-window-grab-start mode=%1 title=%2 object=%3 geometry=%4,%5 %6x%7")
                             .arg(mode,
                                  captureWindow->title(),
                                  captureWindow->objectName())
                             .arg(captureRect.x())
                             .arg(captureRect.y())
                             .arg(captureRect.width())
                             .arg(captureRect.height());
    const QImage windowImage = captureWindow->grabWindow();
    qInfo().noquote() << QStringLiteral("[slm][screenshot] own-window-grab-finish mode=%1 null=%2 size=%3x%4")
                             .arg(mode)
                             .arg(windowImage.isNull())
                             .arg(windowImage.width())
                             .arg(windowImage.height());
    if (windowImage.isNull()) {
        result.insert(QStringLiteral("error"), QStringLiteral("qt-own-window-grab-failed"));
        return result;
    }

    QImage composite(targetRect.size(), QImage::Format_ARGB32_Premultiplied);
    composite.fill(QColor(QStringLiteral("#202124")));
    QPainter painter(&composite);
    const QRect imageRect(captureRect.topLeft(), windowImage.size());
    const QRect drawRect = imageRect.intersected(targetRect);
    const QRect sourceRect(drawRect.topLeft() - imageRect.topLeft(), drawRect.size());
    painter.drawImage(drawRect.translated(-targetRect.topLeft()), windowImage, sourceRect);
    painter.end();

    if (drawRect.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("qt-own-window-outside-target"));
        return result;
    }
    if (!composite.save(targetPath, "PNG")) {
        result.insert(QStringLiteral("error"), QStringLiteral("qt-own-window-save-failed"));
        return result;
    }
    const QFileInfo outFi(targetPath);
    if (!outFi.exists() || outFi.size() <= 0) {
        result.insert(QStringLiteral("error"), QStringLiteral("empty-output"));
        return result;
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("backend"), QStringLiteral("qt-own-windows"));
    result.insert(QStringLiteral("capturedWindows"), 1);
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
    if (!QFileInfo::exists(socketPath)) {
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
    if (isSessionLocked()) {
        return lockedResult(op);
    }
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
    if (isSessionLocked()) {
        return lockedResult(QStringLiteral("fullscreen"));
    }
    const QString target = normalizedOutputPath(outputPath);
    if (qGuiApp != nullptr && QGuiApplication::platformName() == QStringLiteral("wayland")) {
        const QVariantMap ownWindowsResult = captureViaOwnWindows(QStringLiteral("fullscreen"),
                                                                  target,
                                                                  false,
                                                                  QRect());
        if (ownWindowsResult.value(QStringLiteral("ok")).toBool()) {
            return ownWindowsResult;
        }
    }
    const QVariantMap nativeResult = captureViaCompositor(
        QStringLiteral("screenshot fullscreen \"%1\"").arg(target),
        QStringLiteral("fullscreen"),
        target);
    if (nativeResult.value(QStringLiteral("ok")).toBool()) {
        return nativeResult;
    }
    QRect screenRect;
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        screenRect = screen->geometry();
    }
    const QVariantMap kwinResult = captureViaKWinScreenShot2(QStringLiteral("fullscreen"),
                                                             target,
                                                             screenRect);
    if (kwinResult.value(QStringLiteral("ok")).toBool()) {
        return kwinResult;
    }
    const QVariantMap qtResult = captureViaScreenGrab(QStringLiteral("fullscreen"),
                                                      target,
                                                      false,
                                                      QRect());
    if (qtResult.value(QStringLiteral("ok")).toBool()) {
        return qtResult;
    }
    const QVariantMap ownWindowsResult = captureViaOwnWindows(QStringLiteral("fullscreen"),
                                                              target,
                                                              false,
                                                              QRect());
    if (ownWindowsResult.value(QStringLiteral("ok")).toBool()) {
        return ownWindowsResult;
    }
    return captureViaPortal(QStringLiteral("fullscreen"), target, false, QRect());
}

QVariantMap ScreenshotManager::captureWindow(int x, int y, int width, int height,
                                             const QString &outputPath)
{
    if (isSessionLocked()) {
        return lockedResult(QStringLiteral("window"));
    }
    QVariantMap result;
    const QString target = normalizedOutputPath(outputPath);
    if (width <= 0 || height <= 0) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("mode"), QStringLiteral("window"));
        result.insert(QStringLiteral("path"), target);
        result.insert(QStringLiteral("error"), QStringLiteral("invalid-window-geometry"));
        return result;
    }

    const QRect rect(x, y, width, height);
    if (qGuiApp != nullptr && QGuiApplication::platformName() == QStringLiteral("wayland")) {
        const QVariantMap ownWindowsResult = captureViaOwnWindows(QStringLiteral("window"),
                                                                  target,
                                                                  true,
                                                                  rect);
        if (ownWindowsResult.value(QStringLiteral("ok")).toBool()) {
            return ownWindowsResult;
        }
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
    const QVariantMap kwinResult = captureViaKWinScreenShot2(QStringLiteral("window"),
                                                             target,
                                                             rect);
    if (kwinResult.value(QStringLiteral("ok")).toBool()) {
        return kwinResult;
    }
    const QVariantMap qtResult = captureViaScreenGrab(QStringLiteral("window"),
                                                      target,
                                                      true,
                                                      rect);
    if (qtResult.value(QStringLiteral("ok")).toBool()) {
        return qtResult;
    }
    const QVariantMap ownWindowsResult = captureViaOwnWindows(QStringLiteral("window"),
                                                              target,
                                                              true,
                                                              rect);
    if (ownWindowsResult.value(QStringLiteral("ok")).toBool()) {
        return ownWindowsResult;
    }
    return captureViaPortal(QStringLiteral("window"),
                            target,
                            true,
                            rect);
}

QVariantMap ScreenshotManager::captureArea(const QString &outputPath)
{
    if (isSessionLocked()) {
        return lockedResult(QStringLiteral("area"));
    }
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
    if (isSessionLocked()) {
        return lockedResult(QStringLiteral("area"));
    }
    QVariantMap result;
    const QString target = normalizedOutputPath(outputPath);
    if (width <= 0 || height <= 0) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("mode"), QStringLiteral("area"));
        result.insert(QStringLiteral("path"), target);
        result.insert(QStringLiteral("error"), QStringLiteral("invalid-area-geometry"));
        return result;
    }

    const QRect rect(x, y, width, height);
    if (qGuiApp != nullptr && QGuiApplication::platformName() == QStringLiteral("wayland")) {
        const QVariantMap ownWindowsResult = captureViaOwnWindows(QStringLiteral("area"),
                                                                  target,
                                                                  true,
                                                                  rect);
        if (ownWindowsResult.value(QStringLiteral("ok")).toBool()) {
            return ownWindowsResult;
        }
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
    const QVariantMap kwinResult = captureViaKWinScreenShot2(QStringLiteral("area"),
                                                             target,
                                                             rect);
    if (kwinResult.value(QStringLiteral("ok")).toBool()) {
        return kwinResult;
    }
    const QVariantMap qtResult = captureViaScreenGrab(QStringLiteral("area"),
                                                      target,
                                                      true,
                                                      rect);
    if (qtResult.value(QStringLiteral("ok")).toBool()) {
        return qtResult;
    }
    const QVariantMap ownWindowsResult = captureViaOwnWindows(QStringLiteral("area"),
                                                              target,
                                                              true,
                                                              rect);
    if (ownWindowsResult.value(QStringLiteral("ok")).toBool()) {
        return ownWindowsResult;
    }
    return captureViaPortal(QStringLiteral("area"),
                            target,
                            true,
                            rect);
}
