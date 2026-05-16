#pragma once

#include <QObject>
#include <QVariantList>

class NotificationSettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList appRows READ appRows NOTIFY appRowsChanged)
    Q_PROPERTY(bool notifyServiceAvailable READ notifyServiceAvailable NOTIFY notifyServiceAvailableChanged)

public:
    explicit NotificationSettingsController(QObject *parent = nullptr);

    QVariantList appRows() const;
    bool notifyServiceAvailable() const;

public slots:
    void refresh();

public:
    Q_INVOKABLE bool sendPreviewNotification(const QString &appId,
                                             const QString &title,
                                             const QString &body,
                                             const QString &priority,
                                             bool grouped = true);
    Q_INVOKABLE bool clearHistory();

    Q_INVOKABLE QVariantMap appPreferences(const QString &appId) const;
    Q_INVOKABLE QVariant appPreference(const QString &appId,
                                       const QString &name,
                                       const QVariant &fallback = QVariant()) const;
    Q_INVOKABLE bool setAppPreference(const QString &appId,
                                      const QString &name,
                                      const QVariant &value);

signals:
    void appRowsChanged();
    void notifyServiceAvailableChanged();

private:
    QString normalizedAppId(const QString &raw) const;
    QString prefKey(const QString &appId, const QString &name) const;
    void recomputeRows(const QVariantList &items);
    void setNotifyServiceAvailable(bool available);

    QVariantList m_appRows;
    bool m_notifyServiceAvailable = false;
};
