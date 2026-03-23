#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class ScreencastPrivacyModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
    Q_PROPERTY(QStringList activeApps READ activeApps NOTIFY activeAppsChanged)
    Q_PROPERTY(QVariantList activeSessionItems READ activeSessionItems NOTIFY activeSessionItemsChanged)
    Q_PROPERTY(QString statusTitle READ statusTitle NOTIFY presentationChanged)
    Q_PROPERTY(QString statusSubtitle READ statusSubtitle NOTIFY presentationChanged)
    Q_PROPERTY(QString tooltipText READ tooltipText NOTIFY presentationChanged)
    Q_PROPERTY(QString lastActionText READ lastActionText NOTIFY presentationChanged)

public:
    explicit ScreencastPrivacyModel(QObject *parent = nullptr);

    bool active() const;
    int activeCount() const;
    QStringList activeApps() const;
    QVariantList activeSessionItems() const;
    QString statusTitle() const;
    QString statusSubtitle() const;
    QString tooltipText() const;
    QString lastActionText() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool stopSession(const QString &sessionHandle);
    Q_INVOKABLE bool revokeSession(const QString &sessionHandle,
                                   const QString &reason = QStringLiteral("revoked-by-user"));
    Q_INVOKABLE int stopAllSessions();

signals:
    void activeChanged();
    void activeCountChanged();
    void activeAppsChanged();
    void activeSessionItemsChanged();
    void presentationChanged();

private:
    void setState(int activeCount, const QStringList &activeApps, const QVariantList &sessionItems);
    void setLastActionText(const QString &text);
    static QString normalizeAppLabel(const QString &appId);

private slots:
    void onSessionStateChanged(const QString &sessionHandle,
                               const QString &appId,
                               bool active,
                               int activeCount);
    void onSessionRevoked(const QString &sessionHandle,
                          const QString &appId,
                          const QString &reason,
                          int activeCount);

private:
    int m_activeCount = 0;
    QStringList m_activeApps;
    QVariantList m_activeSessionItems;
    QString m_lastActionText;
};
