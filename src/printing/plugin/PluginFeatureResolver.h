#pragma once

#include "PluginFieldDescriptor.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>

namespace Slm::Print {

// Parses, validates, and exposes a printer plugin feature descriptor to QML.
//
// Usage from QML:
//
//   PluginFeatureResolver {
//       id: pluginResolver
//   }
//
//   Component.onCompleted: {
//       pluginResolver.loadDescriptor(someJsonString)
//   }
//
//   PrinterFeatureSection {
//       resolver: pluginResolver
//       settingsModel: dialog.printSession.settingsModel
//   }
//
// The featureModel property is a QVariantList of QVariantMaps.
// Each map has: id, type, label, values (list), currentValue, rangeMin, rangeMax.
//
// Alternatively, load directly from PrinterCapability.vendorExtensions via
// loadFromVendorExtensions(), which looks for a "plugin_descriptor" key.
class PluginFeatureResolver : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool descriptorValid  READ descriptorValid  NOTIFY descriptorChanged)
    Q_PROPERTY(QString descriptorId  READ descriptorId    NOTIFY descriptorChanged)
    Q_PROPERTY(QString descriptorTitle READ descriptorTitle NOTIFY descriptorChanged)
    Q_PROPERTY(QVariantList featureModel READ featureModel NOTIFY featureModelChanged)

public:
    explicit PluginFeatureResolver(QObject *parent = nullptr);

    bool        descriptorValid() const  { return m_descriptor.valid; }
    QString     descriptorId()    const  { return m_descriptor.id; }
    QString     descriptorTitle() const  { return m_descriptor.title; }
    QVariantList featureModel()   const  { return m_featureModel; }

    // Load and validate from a JSON text string.
    // If invalid, descriptorValid becomes false and featureModel is cleared.
    Q_INVOKABLE void loadDescriptor(const QString &jsonText);

    // Load from a PrinterCapability.vendorExtensions QVariantMap.
    // Looks for the "plugin_descriptor" key (JSON string value).
    Q_INVOKABLE void loadFromVendorExtensions(const QVariantMap &vendorExtensions);

    // Clear the current descriptor and reset all values.
    Q_INVOKABLE void clear();

    // Set the current value for a field by id.
    // Ignored if the field id is unknown.
    Q_INVOKABLE void setFieldValue(const QString &fieldId, const QVariant &value);

    // Build a QVariantMap of IPP attribute name → value for all fields
    // that have a non-empty mapToIpp. Ready to merge into PrintTicket.pluginFeatures.
    Q_INVOKABLE QVariantMap buildIppAttributes() const;

    // Reset all field values to their descriptor defaults.
    Q_INVOKABLE void resetValues();

signals:
    void descriptorChanged();
    void featureModelChanged();

private:
    void applyDescriptor(const PluginDescriptor &descriptor);
    void rebuildFeatureModel();

    PluginDescriptor m_descriptor;
    QVariantList     m_featureModel;   // cache for QML binding
    QVariantMap      m_values;         // fieldId → current value
};

} // namespace Slm::Print
