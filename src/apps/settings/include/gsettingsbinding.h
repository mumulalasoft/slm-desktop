#pragma once
#include "settingbinding.h"

#ifdef signals
#  define SLM_RESTORE_QT_SIGNALS_MACRO
#  undef signals
#endif
#include <gio/gio.h>
#ifdef SLM_RESTORE_QT_SIGNALS_MACRO
#  define signals Q_SIGNALS
#  undef SLM_RESTORE_QT_SIGNALS_MACRO
#endif

#include <QVariant>

/**
 * @brief Binding for GNOME GSettings (dconf).
 */
class GSettingsBinding : public SettingBinding {
    Q_OBJECT
public:
    explicit GSettingsBinding(const QString &schemaId, const QString &key, QObject *parent = nullptr);
    ~GSettingsBinding();

    QVariant value() const override;
    void setValue(const QVariant &newValue) override;

private:
    static void on_changed(GSettings *settings, const gchar *key, gpointer user_data);

    GSettings *m_settings = nullptr;
    QString m_key;
};
