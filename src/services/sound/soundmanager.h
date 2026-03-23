#pragma once

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QVariantList>

class SoundManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(int volume READ volume NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString currentSink READ currentSink NOTIFY changed)
    Q_PROPERTY(QVariantList sinks READ sinks NOTIFY changed)
    Q_PROPERTY(QVariantList streams READ streams NOTIFY changed)

public:
    explicit SoundManager(QObject *parent = nullptr);

    bool available() const;
    bool muted() const;
    int volume() const;
    QString iconName() const;
    QString statusText() const;
    QString currentSink() const;
    QVariantList sinks() const;
    QVariantList streams() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setMuted(bool muted);
    Q_INVOKABLE bool setVolume(int volume);
    Q_INVOKABLE bool setDefaultSink(const QString &sinkName);
    Q_INVOKABLE bool setStreamVolume(uint streamId, int volume);
    Q_INVOKABLE bool setStreamMuted(uint streamId, bool muted);
    Q_INVOKABLE bool openSoundSettings();

signals:
    void changed();

private:
    bool hasWpctl() const;
    QString runWpctl(const QStringList &args, int timeoutMs = 1200) const;
    QString detectDefaultSink() const;
    QVariantMap inspectNode(const QString &id) const;
    int parseVolumePercent(const QString &text) const;
    bool parseMuted(const QString &text) const;
    QString computeIconName() const;
    QVariantList querySinks() const;
    QVariantList queryStreams();

    bool m_available = false;
    bool m_muted = false;
    int m_volume = 0;
    QString m_iconName = QStringLiteral("audio-volume-muted-symbolic");
    QString m_statusText = QStringLiteral("Sound unavailable");
    QString m_currentSink;
    QVariantList m_sinks;
    QVariantList m_streams;
    QHash<uint, qint64> m_streamLastActiveMs;
    QTimer *m_timer = nullptr;
    bool m_hasWpctl = false;
    bool m_hasPactl = false;
};
