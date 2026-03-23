#pragma once

#include <QObject>
#include <QVariantList>

class InputCapturePrivacyModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString activeSession READ activeSession NOTIFY activeSessionChanged)
    Q_PROPERTY(int enabledCount READ enabledCount NOTIFY enabledCountChanged)
    Q_PROPERTY(QVariantList sessionItems READ sessionItems NOTIFY sessionItemsChanged)
    Q_PROPERTY(QString statusTitle READ statusTitle NOTIFY presentationChanged)
    Q_PROPERTY(QString statusSubtitle READ statusSubtitle NOTIFY presentationChanged)
    Q_PROPERTY(QString tooltipText READ tooltipText NOTIFY presentationChanged)
    Q_PROPERTY(QString lastActionText READ lastActionText NOTIFY presentationChanged)

public:
    explicit InputCapturePrivacyModel(QObject *parent = nullptr);

    bool active() const;
    QString activeSession() const;
    int enabledCount() const;
    QVariantList sessionItems() const;
    QString statusTitle() const;
    QString statusSubtitle() const;
    QString tooltipText() const;
    QString lastActionText() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool disableSession(const QString &sessionHandle);
    Q_INVOKABLE bool releaseSession(const QString &sessionHandle,
                                    const QString &reason = QStringLiteral("released-by-user"));

signals:
    void activeChanged();
    void activeSessionChanged();
    void enabledCountChanged();
    void sessionItemsChanged();
    void presentationChanged();

private slots:
    void onCaptureActiveChanged(bool active, const QString &sessionHandle);
    void onSessionStateChanged(const QString &sessionHandle, bool enabled, int barrierCount);

private:
    void setState(bool active,
                  const QString &activeSession,
                  int enabledCount,
                  const QVariantList &sessionItems);
    void setLastActionText(const QString &text);

    bool m_active = false;
    QString m_activeSession;
    int m_enabledCount = 0;
    QVariantList m_sessionItems;
    QString m_lastActionText;
};

