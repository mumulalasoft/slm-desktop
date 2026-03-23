#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariantList>
#include <QSet>
#include <cstdint>
#include <memory>

class ScreencastStreamBackend : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        PortalMirror,
        PipeWire,
    };

    explicit ScreencastStreamBackend(Mode mode, QObject *parent = nullptr);
    ~ScreencastStreamBackend() override;

    static std::unique_ptr<ScreencastStreamBackend> createFromEnvironment(QObject *parent = nullptr,
                                                                          QString *selectedMode = nullptr);
    static bool isPipeWireBuildAvailable();

    bool start();
    Mode mode() const { return m_mode; }
    QString modeName() const;
    QString lastError() const { return m_lastError; }
#ifdef SLM_HAVE_PIPEWIRE
    // Internal hooks used by PipeWire C callbacks in this translation unit.
    void upsertPipeWireNode(uint32_t id, const QVariantMap &node);
    void removePipeWireNode(uint32_t id);
#endif

signals:
    void sessionStreamsChanged(const QString &sessionPath, const QVariantList &streams);
    void sessionEnded(const QString &sessionPath);
    void sessionRevoked(const QString &sessionPath, const QString &reason);

private slots:
    void onPortalSessionStateChanged(const QString &sessionHandle,
                                     const QString &appId,
                                     bool active,
                                     int activeCount);
    void onPortalSessionRevoked(const QString &sessionHandle,
                                const QString &appId,
                                const QString &reason,
                                int activeCount);

private:
    struct SessionHints {
        QSet<uint32_t> nodeIds;
        QSet<QString> nodeNames;
        QSet<QString> appNames;
        QSet<QString> sourceTypes;
        bool isEmpty() const
        {
            return nodeIds.isEmpty() && nodeNames.isEmpty() && appNames.isEmpty()
                && sourceTypes.isEmpty();
        }
    };

    static QVariantList normalizeStreams(const QVariantList &streams);
    static QVariantList queryPortalSessionStreams(const QString &sessionPath);
    QVariantList currentPipeWireStreams() const;
    static SessionHints extractSessionHints(const QVariantList &streams);
    static QString normalizedHintText(const QVariant &value);
    static int hintMatchScore(const QVariantMap &stream, const SessionHints &hints);
    QVariantList correlateSessionPipeWireStreams(const QString &sessionPath,
                                                 const QVariantList &portalStreams) const;
#ifdef SLM_HAVE_PIPEWIRE
    bool initPipeWire();
    void shutdownPipeWire();
#endif

    Mode m_mode = Mode::PortalMirror;
    QString m_lastError;
    bool m_pipewireInitialized = false;
    QHash<QString, SessionHints> m_sessionNodeHints;
#ifdef SLM_HAVE_PIPEWIRE
    struct PipeWireState;
    std::unique_ptr<PipeWireState> m_pw;
#endif
};
