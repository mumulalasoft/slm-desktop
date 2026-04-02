#pragma once
#include "settingbinding.h"
#include <QtDBus/QDBusInterface>
#include <QVariant>

/**
 * @brief Binding for NetworkManager via D-Bus.
 */
class NetworkManagerBinding : public SettingBinding {
    Q_OBJECT
public:
    explicit NetworkManagerBinding(QObject *parent = nullptr);
    ~NetworkManagerBinding();

    QVariant value() const override;
    void setValue(const QVariant &newValue) override;

private slots:
    void onStateChanged(uint state);

private:
    QDBusInterface *m_nmInterface;
};
