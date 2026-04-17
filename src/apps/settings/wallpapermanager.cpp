#include "wallpapermanager.h"
#include "desktopsettingsclient.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QRandomGenerator>
#include <QDebug>

WallpaperManager::WallpaperManager(DesktopSettingsClient *desktopSettings,
                                   QObject *parent)
    : QObject(parent)
    , m_desktopSettings(desktopSettings)
{
    if (m_desktopSettings) {
        connect(m_desktopSettings, &DesktopSettingsClient::wallpaperUriChanged,
                this, &WallpaperManager::wallpaperChanged);
    }
}

QString WallpaperManager::currentWallpaperUri() const
{
    return m_desktopSettings ? m_desktopSettings->wallpaperUri() : QString();
}

void WallpaperManager::openFilePicker()
{
    // Build a unique request token and object path per the XDG portal spec.
    const QString token = QStringLiteral("slmwp%1")
        .arg(QRandomGenerator::global()->generate(), 8, 16, QLatin1Char('0'));
    const QString sender = QDBusConnection::sessionBus().baseService()
        .mid(1)
        .replace(QLatin1Char('.'), QLatin1Char('_'))
        .replace(QLatin1Char(':'), QLatin1Char('_'));
    m_activeRequestPath = QStringLiteral(
        "/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

    const bool connected = QDBusConnection::sessionBus().connect(
        {}, m_activeRequestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(onFileChooserResponse(uint, QVariantMap)));

    if (!connected) {
        qWarning() << "[WallpaperManager] Could not subscribe to portal Request.Response";
    }

    setLoading(true);

    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.FileChooser"),
        QStringLiteral("OpenFile"));

    QVariantMap options;
    options[QStringLiteral("handle_token")] = token;
    options[QStringLiteral("accept_label")] = tr("Set as Wallpaper");

    call << QString() << tr("Choose Wallpaper") << options;
    // NoBlock = fire-and-forget; the response arrives via the connected signal.
    QDBusConnection::sessionBus().call(call, QDBus::NoBlock);
}

void WallpaperManager::setWallpaperUri(const QString &fileUri)
{
    if (!m_desktopSettings) {
        emit errorOccurred(tr("Wallpaper settings unavailable"));
        return;
    }
    if (!m_desktopSettings->setWallpaperUri(fileUri)) {
        emit errorOccurred(tr("Failed to update wallpaper setting"));
    }
}

void WallpaperManager::onFileChooserResponse(uint response, const QVariantMap &results)
{
    setLoading(false);

    if (!m_activeRequestPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            {}, m_activeRequestPath,
            QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"),
            this,
            SLOT(onFileChooserResponse(uint, QVariantMap)));
        m_activeRequestPath.clear();
    }

    if (response != 0)
        return; // User cancelled (1) or other error (2).

    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (!uris.isEmpty())
        setWallpaperUri(uris.first());
}

void WallpaperManager::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        emit loadingChanged();
    }
}
