#include "settingbindingresolver.h"

#include <QRegularExpression>

namespace {

QString normalizeDbusPathFromInterface(const QString &iface)
{
    QString out = iface.trimmed();
    out.replace(QLatin1Char('.'), QLatin1Char('/'));
    if (!out.startsWith(QLatin1Char('/'))) {
        out.prepend(QLatin1Char('/'));
    }
    return out;
}

BindingDescriptor unsupportedDescriptor(const QString &spec,
                                       const QString &scheme,
                                       const QVariant &defaultValue)
{
    BindingDescriptor out;
    out.kind = BindingDescriptor::Kind::Unsupported;
    out.original = spec;
    out.scheme = scheme;
    out.defaultValue = defaultValue;
    out.error = QStringLiteral("unsupported-binding-scheme");
    return out;
}

} // namespace

BindingDescriptor SettingBindingResolver::parse(const QString &bindingSpec,
                                                const QVariant &defaultValue)
{
    BindingDescriptor out;
    out.original = bindingSpec;
    out.defaultValue = defaultValue;

    const QString spec = bindingSpec.trimmed();
    if (spec.isEmpty()) {
        out.error = QStringLiteral("empty-binding-spec");
        return out;
    }

    const int sep = spec.indexOf(QLatin1Char(':'));
    if (sep <= 0) {
        out.error = QStringLiteral("invalid-binding-spec");
        return out;
    }

    const QString scheme = spec.left(sep).trimmed().toLower();
    const QString payload = spec.mid(sep + 1).trimmed();
    out.scheme = scheme;

    if (scheme == QStringLiteral("gsettings")) {
        const int slash = payload.indexOf(QLatin1Char('/'));
        if (slash <= 0 || slash + 1 >= payload.size()) {
            out.error = QStringLiteral("invalid-gsettings-spec");
            return out;
        }
        out.kind = BindingDescriptor::Kind::GSettings;
        out.schema = payload.left(slash).trimmed();
        out.key = payload.mid(slash + 1).trimmed();
        if (out.schema.isEmpty() || out.key.isEmpty()) {
            out.kind = BindingDescriptor::Kind::Invalid;
            out.error = QStringLiteral("invalid-gsettings-spec");
        }
        return out;
    }

    if (scheme == QStringLiteral("dbus")) {
        if (payload.compare(QStringLiteral("org.freedesktop.NetworkManager"), Qt::CaseInsensitive) == 0) {
            out.kind = BindingDescriptor::Kind::NetworkManagerState;
            out.service = QStringLiteral("org.freedesktop.NetworkManager");
            out.path = QStringLiteral("/org/freedesktop/NetworkManager");
            out.interfaceName = QStringLiteral("org.freedesktop.NetworkManager");
            return out;
        }

        // Canonical form must be parsed first:
        // dbus:service:path:interface:member[:method]
        // It may contain '/' in path, so parsing compact legacy form first can
        // misclassify canonical specs as DBusProperty.
        const QStringList parts = payload.split(QLatin1Char(':'), Qt::KeepEmptyParts);
        if (parts.size() >= 4) {
            out.service = parts.value(0).trimmed();
            out.path = parts.value(1).trimmed();
            out.interfaceName = parts.value(2).trimmed();
            out.member = parts.value(3).trimmed();
            const QString mode = parts.value(4).trimmed().toLower();
            if (out.service.isEmpty() || out.path.isEmpty()
                || out.interfaceName.isEmpty() || out.member.isEmpty()) {
                out.error = QStringLiteral("invalid-dbus-spec");
                return out;
            }
            out.kind = (mode == QStringLiteral("method"))
                           ? BindingDescriptor::Kind::DBusMethod
                           : BindingDescriptor::Kind::DBusProperty;
            return out;
        }

        // Compact legacy form: dbus:<iface-or-service>/<member>
        const int slash = payload.indexOf(QLatin1Char('/'));
        if (slash > 0) {
            const QString left = payload.left(slash).trimmed();
            const QString right = payload.mid(slash + 1).trimmed();
            if (!left.isEmpty() && !right.isEmpty()) {
                if (left.compare(QStringLiteral("org.freedesktop.NetworkManager"), Qt::CaseInsensitive) == 0) {
                    out.kind = BindingDescriptor::Kind::NetworkManagerProperty;
                    out.service = QStringLiteral("org.freedesktop.NetworkManager");
                    out.path = QStringLiteral("/org/freedesktop/NetworkManager");
                    out.interfaceName = QStringLiteral("org.freedesktop.NetworkManager");
                    out.member = right;
                    return out;
                }

                out.kind = BindingDescriptor::Kind::DBusProperty;
                out.service = left;
                out.interfaceName = left;
                out.path = normalizeDbusPathFromInterface(left);
                out.member = right;
                return out;
            }
        }

        out.error = QStringLiteral("invalid-dbus-spec");
        return out;
    }

    if (scheme == QStringLiteral("settings")
        || scheme == QStringLiteral("system")
        || scheme == QStringLiteral("xdg")
        || scheme == QStringLiteral("portal")) {
        out.kind = BindingDescriptor::Kind::LocalSettings;
        out.localKey = QStringLiteral("%1/%2").arg(scheme, payload);
        return out;
    }

    return unsupportedDescriptor(spec, scheme, defaultValue);
}
