#include "PluginFeatureResolver.h"
#include "PluginDescriptorValidator.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPluginResolver, "slm.print.plugin.resolver")

namespace Slm::Print {

PluginFeatureResolver::PluginFeatureResolver(QObject *parent)
    : QObject(parent)
{}

void PluginFeatureResolver::loadDescriptor(const QString &jsonText)
{
    const PluginDescriptor descriptor = PluginDescriptorValidator::parse(jsonText);
    applyDescriptor(descriptor);
}

void PluginFeatureResolver::loadFromVendorExtensions(const QVariantMap &vendorExtensions)
{
    const QString key = QStringLiteral("plugin_descriptor");
    if (!vendorExtensions.contains(key)) {
        qCDebug(lcPluginResolver) << "loadFromVendorExtensions: no plugin_descriptor key";
        clear();
        return;
    }
    const QString jsonText = vendorExtensions.value(key).toString();
    loadDescriptor(jsonText);
}

void PluginFeatureResolver::clear()
{
    m_descriptor = PluginDescriptor{};
    m_values.clear();
    m_featureModel.clear();
    emit descriptorChanged();
    emit featureModelChanged();
}

void PluginFeatureResolver::setFieldValue(const QString &fieldId, const QVariant &value)
{
    if (!m_descriptor.valid)
        return;

    // Verify fieldId exists in descriptor
    bool found = false;
    for (const PluginFieldDescriptor &f : m_descriptor.fields) {
        if (f.id == fieldId) {
            found = true;
            break;
        }
    }
    if (!found) {
        qCWarning(lcPluginResolver) << "setFieldValue: unknown field id:" << fieldId;
        return;
    }

    m_values[fieldId] = value;
    rebuildFeatureModel();
    emit featureModelChanged();
}

QVariantMap PluginFeatureResolver::buildIppAttributes() const
{
    QVariantMap result;
    if (!m_descriptor.valid)
        return result;

    for (const PluginFieldDescriptor &f : m_descriptor.fields) {
        if (f.mapToIpp.isEmpty())
            continue;
        const QVariant currentValue = m_values.value(f.id, f.defaultValue);
        result[f.mapToIpp] = currentValue;
    }
    return result;
}

void PluginFeatureResolver::resetValues()
{
    m_values.clear();
    rebuildFeatureModel();
    emit featureModelChanged();
}

void PluginFeatureResolver::applyDescriptor(const PluginDescriptor &descriptor)
{
    m_descriptor = descriptor;
    m_values.clear();
    rebuildFeatureModel();
    emit descriptorChanged();
    emit featureModelChanged();

    if (!descriptor.valid) {
        qCWarning(lcPluginResolver) << "applyDescriptor: invalid descriptor, feature section hidden";
    } else {
        qCDebug(lcPluginResolver) << "applyDescriptor: loaded descriptor"
                                  << descriptor.id
                                  << "with" << descriptor.fields.size() << "fields";
    }
}

void PluginFeatureResolver::rebuildFeatureModel()
{
    m_featureModel.clear();
    if (!m_descriptor.valid)
        return;

    for (const PluginFieldDescriptor &f : m_descriptor.fields) {
        QVariantMap entry;
        entry[QStringLiteral("id")]           = f.id;
        entry[QStringLiteral("type")]         = f.type;
        entry[QStringLiteral("label")]        = f.label;
        entry[QStringLiteral("values")]       = f.values;
        entry[QStringLiteral("currentValue")] = m_values.value(f.id, f.defaultValue);
        entry[QStringLiteral("rangeMin")]     = f.rangeMin;
        entry[QStringLiteral("rangeMax")]     = f.rangeMax;
        entry[QStringLiteral("mapToIpp")]     = f.mapToIpp;
        m_featureModel.append(entry);
    }
}

} // namespace Slm::Print
