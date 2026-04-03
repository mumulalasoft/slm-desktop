#include "PluginDescriptorValidator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPluginValidator, "slm.print.plugin.validator")

namespace Slm::Print {

namespace {
constexpr int kMaxFields      = 32;
constexpr int kMaxEnumValues  = 64;
constexpr int kMaxTokenLength = 64;

static const QRegularExpression kSafeTokenRe(
    QStringLiteral("^[a-zA-Z0-9._\\-]+$"));
} // anonymous namespace

bool PluginDescriptorValidator::isAllowedType(const QString &type)
{
    return type == QLatin1String(PluginFieldType::Bool)
        || type == QLatin1String(PluginFieldType::Enum)
        || type == QLatin1String(PluginFieldType::Segmented)
        || type == QLatin1String(PluginFieldType::Range);
}

bool PluginDescriptorValidator::isSafeToken(const QString &token)
{
    if (token.isEmpty() || token.length() > kMaxTokenLength)
        return false;
    return kSafeTokenRe.match(token).hasMatch();
}

PluginDescriptor PluginDescriptorValidator::parse(const QString &jsonText)
{
    if (jsonText.trimmed().isEmpty()) {
        qCWarning(lcPluginValidator) << "parse: empty JSON text";
        return {};
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        qCWarning(lcPluginValidator) << "parse: JSON parse error:" << err.errorString();
        return {};
    }
    if (!doc.isObject()) {
        qCWarning(lcPluginValidator) << "parse: top-level JSON value is not an object";
        return {};
    }
    return parse(doc.object());
}

PluginDescriptor PluginDescriptorValidator::parse(const QJsonObject &obj)
{
    PluginDescriptor descriptor;

    // Required: id
    const QString id = obj.value(QStringLiteral("id")).toString();
    if (!isSafeToken(id)) {
        qCWarning(lcPluginValidator) << "parse: missing or unsafe 'id' field:" << id;
        return descriptor;
    }
    descriptor.id = id;

    // Required: title (non-empty string, no length restriction for display)
    const QString title = obj.value(QStringLiteral("title")).toString();
    if (title.trimmed().isEmpty()) {
        qCWarning(lcPluginValidator) << "parse: missing 'title' field";
        return descriptor;
    }
    descriptor.title = title.trimmed();

    // Required: fields array
    if (!obj.contains(QStringLiteral("fields")) || !obj.value(QStringLiteral("fields")).isArray()) {
        qCWarning(lcPluginValidator) << "parse: missing or non-array 'fields'";
        return descriptor;
    }
    const QJsonArray fieldsArray = obj.value(QStringLiteral("fields")).toArray();
    if (fieldsArray.size() > kMaxFields) {
        qCWarning(lcPluginValidator) << "parse: too many fields (" << fieldsArray.size()
                                     << "), maximum is" << kMaxFields;
        return descriptor;
    }

    for (const QJsonValue &fv : fieldsArray) {
        if (!fv.isObject()) {
            qCWarning(lcPluginValidator) << "parse: field entry is not an object, skipping descriptor";
            return descriptor;
        }
        bool fieldOk = false;
        const PluginFieldDescriptor field = parseField(fv.toObject(), fieldOk);
        if (!fieldOk) {
            qCWarning(lcPluginValidator) << "parse: invalid field, rejecting descriptor";
            return descriptor;
        }
        descriptor.fields.append(field);
    }

    descriptor.valid = true;
    return descriptor;
}

PluginFieldDescriptor PluginDescriptorValidator::parseField(const QJsonObject &obj, bool &ok)
{
    ok = false;
    PluginFieldDescriptor field;

    // id
    const QString id = obj.value(QStringLiteral("id")).toString();
    if (!isSafeToken(id)) {
        qCWarning(lcPluginValidator) << "parseField: unsafe field id:" << id;
        return field;
    }
    field.id = id;

    // type
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (!isAllowedType(type)) {
        qCWarning(lcPluginValidator) << "parseField: disallowed field type:" << type
                                     << "for field" << id;
        return field;
    }
    field.type = type;

    // label (display string — allow spaces/unicode, just must be non-empty)
    const QString label = obj.value(QStringLiteral("label")).toString().trimmed();
    if (label.isEmpty()) {
        qCWarning(lcPluginValidator) << "parseField: empty label for field" << id;
        return field;
    }
    field.label = label;

    // mapToIpp (optional, must be safe token if present)
    const QString mapToIpp = obj.value(QStringLiteral("map_to_ipp")).toString();
    if (!mapToIpp.isEmpty()) {
        if (!isSafeToken(mapToIpp)) {
            qCWarning(lcPluginValidator) << "parseField: unsafe map_to_ipp:" << mapToIpp
                                         << "for field" << id;
            return field;
        }
        field.mapToIpp = mapToIpp;
    }

    // Type-specific validation
    if (type == QLatin1String(PluginFieldType::Enum)
        || type == QLatin1String(PluginFieldType::Segmented))
    {
        if (!obj.contains(QStringLiteral("values")) || !obj.value(QStringLiteral("values")).isArray()) {
            qCWarning(lcPluginValidator) << "parseField: enum/segmented field" << id
                                         << "missing 'values' array";
            return field;
        }
        const QJsonArray vals = obj.value(QStringLiteral("values")).toArray();
        if (vals.isEmpty()) {
            qCWarning(lcPluginValidator) << "parseField: empty values for field" << id;
            return field;
        }
        if (vals.size() > kMaxEnumValues) {
            qCWarning(lcPluginValidator) << "parseField: too many enum values for field" << id;
            return field;
        }
        for (const QJsonValue &v : vals) {
            const QString sv = v.toString();
            if (!isSafeToken(sv)) {
                qCWarning(lcPluginValidator) << "parseField: unsafe enum value" << sv
                                             << "in field" << id;
                return field;
            }
            field.values.append(sv);
        }
        field.defaultValue = field.values.first();
    }
    else if (type == QLatin1String(PluginFieldType::Bool))
    {
        field.defaultValue = obj.value(QStringLiteral("default")).toBool(false);
    }
    else if (type == QLatin1String(PluginFieldType::Range))
    {
        field.rangeMin     = obj.value(QStringLiteral("min")).toInt(0);
        field.rangeMax     = obj.value(QStringLiteral("max")).toInt(100);
        if (field.rangeMin >= field.rangeMax) {
            qCWarning(lcPluginValidator) << "parseField: range min >= max for field" << id;
            return field;
        }
        field.defaultValue = obj.value(QStringLiteral("default")).toInt(field.rangeMin);
    }

    ok = true;
    return field;
}

} // namespace Slm::Print
