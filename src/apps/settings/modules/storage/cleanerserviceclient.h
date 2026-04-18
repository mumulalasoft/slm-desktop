#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class QDBusInterface;

class CleanerServiceClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool cleaning READ cleaning NOTIFY cleaningChanged)
    Q_PROPERTY(int progressPercent READ progressPercent NOTIFY progressChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressChanged)
    Q_PROPERTY(QVariantMap scanResult READ scanResult NOTIFY scanResultChanged)
    Q_PROPERTY(QVariantMap lastResult READ lastResult NOTIFY lastResultChanged)
    Q_PROPERTY(QVariantList appCaches READ appCaches NOTIFY scanResultChanged)
    Q_PROPERTY(QVariantList smartSuggestions READ smartSuggestions NOTIFY scanResultChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool autoClean READ autoClean NOTIFY policyChanged)
    Q_PROPERTY(int maxCacheSizeMb READ maxCacheSizeMb NOTIFY policyChanged)
    Q_PROPERTY(int deleteAfterDays READ deleteAfterDays NOTIFY policyChanged)

public:
    explicit CleanerServiceClient(QObject *parent = nullptr);
    ~CleanerServiceClient() override;

    bool available() const;
    bool scanning() const;
    bool cleaning() const;
    int progressPercent() const;
    QString progressMessage() const;
    QVariantMap scanResult() const;
    QVariantMap lastResult() const;
    QVariantList appCaches() const;
    QVariantList smartSuggestions() const;
    QString lastError() const;
    bool autoClean() const;
    int maxCacheSizeMb() const;
    int deleteAfterDays() const;

    Q_INVOKABLE void requestScan();
    Q_INVOKABLE void requestPreview(const QVariantMap &options);
    Q_INVOKABLE void requestClean(const QVariantMap &options);
    Q_INVOKABLE void refreshAvailability();
    Q_INVOKABLE void requestPolicy();
    Q_INVOKABLE bool updatePolicy(const QVariantMap &policyPatch);

signals:
    void availableChanged();
    void scanningChanged();
    void cleaningChanged();
    void progressChanged();
    void scanResultChanged();
    void lastResultChanged();
    void lastErrorChanged();
    void policyChanged();

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onDaemonProgressChanged(int percent, const QString &message);

private:
    bool ensureIface();
    QVariantMap callMapMethod(const QString &method, const QVariantList &args = {});
    void setLastError(const QString &error);

    QDBusInterface *m_iface = nullptr;
    bool m_available = false;
    bool m_scanning = false;
    bool m_cleaning = false;
    int m_progressPercent = 0;
    QString m_progressMessage;
    QVariantMap m_scanResult;
    QVariantMap m_lastResult;
    QString m_lastError;
    bool m_autoClean = false;
    int m_maxCacheSizeMb = 1024;
    int m_deleteAfterDays = 30;
};
