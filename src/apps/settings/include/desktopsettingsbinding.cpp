#include "desktopsettingsbinding.h"

DesktopSettingsBinding::DesktopSettingsBinding(DesktopSettingsClient *client,
                                               const QString &path,
                                               const QVariant &defaultValue,
                                               QObject *parent)
    : SettingBinding(parent)
    , m_client(client)
    , m_path(path)
    , m_defaultValue(defaultValue)
{
    if (m_client) {
        connect(m_client, &DesktopSettingsClient::settingChanged, this, [this](const QString &p) {
            if (p == m_path) {
                emit valueChanged();
            }
        });
    }
}

QVariant DesktopSettingsBinding::value() const
{
    if (!m_client) {
        return m_defaultValue;
    }
    return m_client->settingValue(m_path, m_defaultValue);
}

void DesktopSettingsBinding::setValue(const QVariant &newValue)
{
    if (!m_client) {
        endOperation(QStringLiteral("write"), false, QStringLiteral("client-unavailable"));
        return;
    }
    beginOperation();
    bool ok = m_client->setSettingValue(m_path, newValue);
    endOperation(QStringLiteral("write"), ok, ok ? QString() : QStringLiteral("write-failed"));
    if (ok) {
        emit valueChanged();
    }
}
