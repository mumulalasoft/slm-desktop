#include "localsettingsbinding.h"

#include <QSettings>

namespace {
constexpr const char kOrg[] = "SLM";
constexpr const char kApp[] = "slm-settings";
}

LocalSettingsBinding::LocalSettingsBinding(const QString &key,
                                           const QVariant &defaultValue,
                                           QObject *parent)
    : SettingBinding(parent)
    , m_key(key.trimmed())
    , m_defaultValue(defaultValue)
{
}

QVariant LocalSettingsBinding::value() const
{
    if (m_key.isEmpty()) {
        return m_defaultValue;
    }
    QSettings settings(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
    return settings.value(m_key, m_defaultValue);
}

void LocalSettingsBinding::setValue(const QVariant &newValue)
{
    if (m_key.isEmpty()) {
        endOperation(QStringLiteral("write"), false, QStringLiteral("invalid-local-setting-key"));
        return;
    }
    beginOperation();
    QSettings settings(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
    const QVariant oldValue = settings.value(m_key, m_defaultValue);
    if (oldValue == newValue) {
        endOperation(QStringLiteral("write"), true);
        return;
    }
    settings.setValue(m_key, newValue);
    settings.sync();
    emit valueChanged();
    endOperation(QStringLiteral("write"), true);
}
