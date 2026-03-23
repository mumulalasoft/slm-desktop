#include "mediasessionmanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QVariantMap>

namespace {
const QString kMprisPrefix = QStringLiteral("org.mpris.MediaPlayer2.");
const QString kMprisPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kPlayerIface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString kRootIface = QStringLiteral("org.mpris.MediaPlayer2");
const QString kPropsIface = QStringLiteral("org.freedesktop.DBus.Properties");
}

MediaSessionManager::MediaSessionManager(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1500);
    connect(m_timer, &QTimer::timeout, this, &MediaSessionManager::refresh);
    m_timer->start();
    refresh();
}

bool MediaSessionManager::hasActiveSession() const
{
    return m_hasActiveSession;
}

QString MediaSessionManager::playerName() const
{
    return m_playerName;
}

QString MediaSessionManager::title() const
{
    return m_title;
}

QString MediaSessionManager::artist() const
{
    return m_artist;
}

QString MediaSessionManager::artworkUrl() const
{
    return m_artworkUrl;
}

QString MediaSessionManager::playbackStatus() const
{
    return m_playbackStatus;
}

QString MediaSessionManager::activeService() const
{
    return m_activeService;
}

QVariantList MediaSessionManager::players() const
{
    return m_players;
}

bool MediaSessionManager::canGoNext() const
{
    return m_canGoNext;
}

bool MediaSessionManager::canGoPrevious() const
{
    return m_canGoPrevious;
}

bool MediaSessionManager::canPlayPause() const
{
    return m_canPlayPause;
}

void MediaSessionManager::refresh()
{
    const bool oldHas = m_hasActiveSession;
    const QString oldService = m_activeService;
    const QString oldPlayer = m_playerName;
    const QString oldTitle = m_title;
    const QString oldArtist = m_artist;
    const QString oldArtwork = m_artworkUrl;
    const QString oldStatus = m_playbackStatus;
    const QVariantList oldPlayers = m_players;
    const bool oldCanNext = m_canGoNext;
    const bool oldCanPrev = m_canGoPrevious;
    const bool oldCanToggle = m_canPlayPause;

    const QStringList services = mprisServices();
    m_players.clear();
    for (const QString &service : services) {
        QVariantMap item;
        item.insert(QStringLiteral("service"), service);
        item.insert(QStringLiteral("name"), normalizePlayerName(service));
        item.insert(QStringLiteral("status"), servicePlaybackStatus(service));
        m_players << item;
    }

    QString chosen;
    if (!m_preferredService.isEmpty() && services.contains(m_preferredService)) {
        chosen = m_preferredService;
    }
    for (const QString &service : services) {
        if (!chosen.isEmpty()) {
            break;
        }
        QDBusInterface props(service, kMprisPath, kPropsIface, QDBusConnection::sessionBus());
        if (!props.isValid()) {
            continue;
        }
        QDBusReply<QDBusVariant> statusReply = props.call(QStringLiteral("Get"), kPlayerIface, QStringLiteral("PlaybackStatus"));
        const QString status = statusReply.isValid() ? statusReply.value().variant().toString() : QString();
        if (status.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0) {
            chosen = service;
            break;
        }
    }
    if (chosen.isEmpty() && !services.isEmpty()) {
        chosen = services.first();
    }

    if (chosen.isEmpty()) {
        m_hasActiveSession = false;
        m_activeService.clear();
        m_playerName.clear();
        m_title.clear();
        m_artist.clear();
        m_artworkUrl.clear();
        m_playbackStatus = QStringLiteral("Stopped");
        m_canGoNext = false;
        m_canGoPrevious = false;
        m_canPlayPause = false;
    } else {
        m_hasActiveSession = readPropertiesForService(chosen);
    }

    if (oldHas != m_hasActiveSession ||
        oldService != m_activeService ||
        oldPlayer != m_playerName ||
        oldTitle != m_title ||
        oldArtist != m_artist ||
        oldArtwork != m_artworkUrl ||
        oldStatus != m_playbackStatus ||
        oldPlayers != m_players ||
        oldCanNext != m_canGoNext ||
        oldCanPrev != m_canGoPrevious ||
        oldCanToggle != m_canPlayPause) {
        emit changed();
    }
}

bool MediaSessionManager::playPause()
{
    return callPlayerMethod(QStringLiteral("PlayPause"));
}

bool MediaSessionManager::next()
{
    return callPlayerMethod(QStringLiteral("Next"));
}

bool MediaSessionManager::previous()
{
    return callPlayerMethod(QStringLiteral("Previous"));
}

bool MediaSessionManager::setActivePlayer(const QString &service)
{
    const QString s = service.trimmed();
    if (s.isEmpty()) {
        return false;
    }
    m_preferredService = s;
    refresh();
    return !m_activeService.isEmpty();
}

QStringList MediaSessionManager::mprisServices() const
{
    QStringList out;
    auto iface = QDBusConnection::sessionBus().interface();
    if (!iface) {
        return out;
    }
    QDBusReply<QStringList> reply = iface->registeredServiceNames();
    if (!reply.isValid()) {
        return out;
    }
    for (const QString &service : reply.value()) {
        if (service.startsWith(kMprisPrefix)) {
            out << service;
        }
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

bool MediaSessionManager::callPlayerMethod(const QString &method)
{
    if (m_activeService.isEmpty()) {
        return false;
    }
    QDBusInterface player(m_activeService, kMprisPath, kPlayerIface, QDBusConnection::sessionBus());
    if (!player.isValid()) {
        return false;
    }
    QDBusReply<void> reply = player.call(method);
    if (!reply.isValid()) {
        return false;
    }
    refresh();
    return true;
}

bool MediaSessionManager::readPropertiesForService(const QString &service)
{
    QDBusInterface props(service, kMprisPath, kPropsIface, QDBusConnection::sessionBus());
    if (!props.isValid()) {
        return false;
    }

    auto getProp = [&](const QString &iface, const QString &name) -> QVariant {
        QDBusReply<QDBusVariant> reply = props.call(QStringLiteral("Get"), iface, name);
        if (!reply.isValid()) {
            return QVariant();
        }
        return reply.value().variant();
    };

    m_activeService = service;
    m_playerName = normalizePlayerName(service);
    m_playbackStatus = getProp(kPlayerIface, QStringLiteral("PlaybackStatus")).toString();
    m_canGoNext = getProp(kPlayerIface, QStringLiteral("CanGoNext")).toBool();
    m_canGoPrevious = getProp(kPlayerIface, QStringLiteral("CanGoPrevious")).toBool();
    m_canPlayPause = getProp(kPlayerIface, QStringLiteral("CanPlay")).toBool() ||
                     getProp(kPlayerIface, QStringLiteral("CanPause")).toBool();

    const QVariant metadataVar = getProp(kPlayerIface, QStringLiteral("Metadata"));
    const QVariantMap metadata = metadataVar.toMap();
    m_title = metadata.value(QStringLiteral("xesam:title")).toString().trimmed();

    QString artist;
    const QVariant artistVar = metadata.value(QStringLiteral("xesam:artist"));
    if (artistVar.typeId() == QMetaType::QStringList) {
        artist = artistVar.toStringList().join(QStringLiteral(", "));
    } else if (artistVar.typeId() == QMetaType::QVariantList) {
        QStringList parts;
        for (const QVariant &v : artistVar.toList()) {
            const QString s = v.toString().trimmed();
            if (!s.isEmpty()) {
                parts << s;
            }
        }
        artist = parts.join(QStringLiteral(", "));
    } else {
        artist = artistVar.toString().trimmed();
    }
    m_artist = artist;
    m_artworkUrl = metadata.value(QStringLiteral("mpris:artUrl")).toString().trimmed();

    if (m_title.isEmpty()) {
        m_title = QStringLiteral("No media playing");
    }
    return true;
}

QString MediaSessionManager::normalizePlayerName(const QString &service) const
{
    QString name = service;
    if (name.startsWith(kMprisPrefix)) {
        name.remove(0, kMprisPrefix.size());
    }
    const int dot = name.indexOf('.');
    if (dot > 0) {
        name = name.left(dot);
    }
    if (!name.isEmpty()) {
        name[0] = name.at(0).toUpper();
    }
    return name;
}

QString MediaSessionManager::servicePlaybackStatus(const QString &service) const
{
    QDBusInterface props(service, kMprisPath, kPropsIface, QDBusConnection::sessionBus());
    if (!props.isValid()) {
        return QStringLiteral("Unknown");
    }
    QDBusReply<QDBusVariant> reply = props.call(QStringLiteral("Get"), kPlayerIface, QStringLiteral("PlaybackStatus"));
    if (!reply.isValid()) {
        return QStringLiteral("Unknown");
    }
    const QString status = reply.value().variant().toString().trimmed();
    return status.isEmpty() ? QStringLiteral("Unknown") : status;
}
