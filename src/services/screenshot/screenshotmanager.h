#pragma once

#include <QObject>
#include <QRect>
#include <QVariantMap>

#include <functional>

class QEventLoop;

class ScreenshotManager : public QObject
{
    Q_OBJECT

public:
    explicit ScreenshotManager(QObject *parent = nullptr);

    // Provider returns true while the session is locked. Capture entry points
    // early-return {ok:false, error:"session-locked"} when the provider is
    // installed and reports true. Wired in production from SessionStateClient;
    // tests inject a lambda directly to avoid needing a DBus session bus.
    void setLockedProvider(std::function<bool()> provider);

    Q_INVOKABLE QVariantMap capture(const QString &mode,
                                    int x = 0,
                                    int y = 0,
                                    int width = 0,
                                    int height = 0,
                                    const QString &outputPath = QString());
    Q_INVOKABLE QVariantMap captureFullscreen(const QString &outputPath = QString());
    Q_INVOKABLE QVariantMap captureWindow(int x, int y, int width, int height,
                                          const QString &outputPath = QString());
    Q_INVOKABLE QVariantMap captureAreaRect(int x, int y, int width, int height,
                                            const QString &outputPath = QString());
    Q_INVOKABLE QVariantMap captureArea(const QString &outputPath = QString());

private:
    Q_SLOT void onPortalResponse(uint response, const QVariantMap &results);

    QVariantMap captureViaPortal(const QString &mode,
                                 const QString &targetPath,
                                 bool cropEnabled = false,
                                 const QRect &cropRect = QRect());
    QVariantMap captureViaScreenGrab(const QString &mode,
                                     const QString &targetPath,
                                     bool cropEnabled = false,
                                     const QRect &cropRect = QRect());
    QVariantMap captureViaKWinScreenShot2(const QString &mode,
                                          const QString &targetPath,
                                          const QRect &captureRect);
    QVariantMap captureViaGrim(const QString &mode,
                               const QString &targetPath,
                               bool cropEnabled = false,
                               const QRect &cropRect = QRect());
    QVariantMap captureViaOwnWindows(const QString &mode,
                                     const QString &targetPath,
                                     bool cropEnabled = false,
                                     const QRect &cropRect = QRect());
    QVariantMap captureViaCompositor(const QString &command,
                                     const QString &mode,
                                     const QString &targetPath);
    QVariantMap runCapture(const QStringList &args, const QString &mode, const QString &targetPath);
    static QString defaultOutputPath();
    static QString normalizedOutputPath(const QString &path);

    bool isSessionLocked() const;
    static QVariantMap lockedResult(const QString &mode);

    QEventLoop *m_portalLoop = nullptr;
    bool m_portalGotResponse = false;
    uint m_portalResponseCode = 2;
    QVariantMap m_portalPayload;
    std::function<bool()> m_lockedProvider;
};
