#pragma once

#include <QObject>
#include <QVariantMap>

class PortalUiBridge : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.PortalUI")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit PortalUiBridge(QObject *parent = nullptr);
    ~PortalUiBridge() override;

    bool serviceRegistered() const;

    Q_INVOKABLE void submitFileChooserResult(const QString &requestId,
                                             const QVariantMap &result);
    Q_INVOKABLE void submitConsentResult(const QString &requestPath,
                                         const QVariantMap &result);

public slots:
    QVariantMap Ping() const;
    QVariantMap FileChooser(const QVariantMap &options);
    QVariantMap ConsentRequest(const QVariantMap &payload);

signals:
    void serviceRegisteredChanged();
    void fileChooserRequested(const QString &requestId, const QVariantMap &options);
    void fileChooserResultSubmitted(const QString &requestId);
    void consentRequested(const QString &requestPath, const QVariantMap &payload);
    void consentResultSubmitted(const QString &requestPath);

private:
    void registerDbusService();

    bool m_serviceRegistered = false;
    QVariantMap m_results;
    QVariantMap m_consentResults;
};
