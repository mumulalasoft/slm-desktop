#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>

class MediaSessionManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasActiveSession READ hasActiveSession NOTIFY changed)
    Q_PROPERTY(QString playerName READ playerName NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString artist READ artist NOTIFY changed)
    Q_PROPERTY(QString artworkUrl READ artworkUrl NOTIFY changed)
    Q_PROPERTY(QString playbackStatus READ playbackStatus NOTIFY changed)
    Q_PROPERTY(QString activeService READ activeService NOTIFY changed)
    Q_PROPERTY(QVariantList players READ players NOTIFY changed)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY changed)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY changed)
    Q_PROPERTY(bool canPlayPause READ canPlayPause NOTIFY changed)

public:
    explicit MediaSessionManager(QObject *parent = nullptr);

    bool hasActiveSession() const;
    QString playerName() const;
    QString title() const;
    QString artist() const;
    QString artworkUrl() const;
    QString playbackStatus() const;
    QString activeService() const;
    QVariantList players() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPlayPause() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool playPause();
    Q_INVOKABLE bool next();
    Q_INVOKABLE bool previous();
    Q_INVOKABLE bool setActivePlayer(const QString &service);

signals:
    void changed();

private:
    QStringList mprisServices() const;
    bool callPlayerMethod(const QString &method);
    bool readPropertiesForService(const QString &service);
    QString normalizePlayerName(const QString &service) const;
    QString servicePlaybackStatus(const QString &service) const;

    bool m_hasActiveSession = false;
    QString m_activeService;
    QString m_playerName;
    QString m_title;
    QString m_artist;
    QString m_artworkUrl;
    QString m_playbackStatus = QStringLiteral("Stopped");
    QString m_preferredService;
    QVariantList m_players;
    bool m_canGoNext = false;
    bool m_canGoPrevious = false;
    bool m_canPlayPause = false;
    QTimer *m_timer = nullptr;
};
