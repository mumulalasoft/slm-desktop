#include "gsettingsbinding.h"
#include <QDebug>

GSettingsBinding::GSettingsBinding(const QString &schemaId, const QString &key, QObject *parent)
    : SettingBinding(parent)
    , m_key(key)
{
    const QByteArray schemaUtf8 = schemaId.toUtf8();
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    if (!source) {
        qWarning() << "GSettingsBinding: default schema source unavailable";
        return;
    }

    GSettingsSchema *schema = g_settings_schema_source_lookup(source, schemaUtf8.constData(), TRUE);
    if (!schema) {
        qWarning() << "GSettingsBinding: schema is not installed" << schemaId;
        return;
    }
    g_settings_schema_unref(schema);

    m_settings = g_settings_new(schemaUtf8.constData());
    if (m_settings) {
        g_signal_connect(m_settings, "changed", G_CALLBACK(on_changed), this);
    } else {
        qWarning() << "GSettingsBinding: Failed to open schema" << schemaId;
    }
}

GSettingsBinding::~GSettingsBinding()
{
    if (m_settings) {
        g_object_unref(m_settings);
    }
}

QVariant GSettingsBinding::value() const
{
    if (!m_settings) return QVariant();

    GVariant *gv = g_settings_get_value(m_settings, m_key.toUtf8().constData());
    if (!gv) return QVariant();

    QVariant res;
    if (g_variant_is_of_type(gv, G_VARIANT_TYPE_BOOLEAN)) {
        res = (bool)g_variant_get_boolean(gv);
    } else if (g_variant_is_of_type(gv, G_VARIANT_TYPE_INT32)) {
        res = (int)g_variant_get_int32(gv);
    } else if (g_variant_is_of_type(gv, G_VARIANT_TYPE_STRING)) {
        res = QString::fromUtf8(g_variant_get_string(gv, nullptr));
    } else if (g_variant_is_of_type(gv, G_VARIANT_TYPE_DOUBLE)) {
        res = g_variant_get_double(gv);
    } else {
        // Fallback or complex type handled by user
        gchar *str = g_variant_print(gv, FALSE);
        res = QString::fromUtf8(str);
        g_free(str);
    }

    g_variant_unref(gv);
    return res;
}

void GSettingsBinding::setValue(const QVariant &newValue)
{
    if (!m_settings) {
        endOperation(QStringLiteral("write"), false, QStringLiteral("gsettings-unavailable"));
        return;
    }
    beginOperation();

    GVariant *current = g_settings_get_value(m_settings, m_key.toUtf8().constData());
    if (!current) {
        endOperation(QStringLiteral("write"), false, QStringLiteral("gsettings-read-failed"));
        return;
    }

    GVariant *gv = nullptr;
    if (g_variant_is_of_type(current, G_VARIANT_TYPE_BOOLEAN)) {
        gv = g_variant_new_boolean(newValue.toBool());
    } else if (g_variant_is_of_type(current, G_VARIANT_TYPE_INT32)) {
        gv = g_variant_new_int32(newValue.toInt());
    } else if (g_variant_is_of_type(current, G_VARIANT_TYPE_STRING)) {
        gv = g_variant_new_string(newValue.toString().toUtf8().constData());
    } else if (g_variant_is_of_type(current, G_VARIANT_TYPE_DOUBLE)) {
        gv = g_variant_new_double(newValue.toDouble());
    }

    if (gv) {
        g_settings_set_value(m_settings, m_key.toUtf8().constData(), gv);
        // Signal will be emitted by GSettings itself, which we catch in on_changed
        endOperation(QStringLiteral("write"), true);
    } else {
        endOperation(QStringLiteral("write"), false, QStringLiteral("unsupported-gsettings-type"));
    }

    g_variant_unref(current);
}

void GSettingsBinding::on_changed(GSettings *settings, const gchar *key, gpointer user_data)
{
    GSettingsBinding *self = static_cast<GSettingsBinding*>(user_data);
    if (self->m_key == QString::fromUtf8(key)) {
        emit self->valueChanged();
    }
}
