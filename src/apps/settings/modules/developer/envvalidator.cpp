#include "envvalidator.h"

#include <QMap>
#include <QRegularExpression>

const QStringList &EnvValidator::criticalKeys()
{
    static const QStringList k = {
        QStringLiteral("PATH"),
        QStringLiteral("LD_LIBRARY_PATH"),
        QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
    };
    return k;
}

const QStringList &EnvValidator::highKeys()
{
    static const QStringList k = {
        QStringLiteral("WAYLAND_DISPLAY"),
        QStringLiteral("DISPLAY"),
        QStringLiteral("QT_PLUGIN_PATH"),
    };
    return k;
}

const QStringList &EnvValidator::mediumKeys()
{
    static const QStringList k = {
        QStringLiteral("LD_PRELOAD"),
        QStringLiteral("PYTHONPATH"),
        QStringLiteral("PERLLIB"),
        QStringLiteral("RUBYLIB"),
    };
    return k;
}

static const QMap<QString, QString> &descriptions()
{
    static const QMap<QString, QString> d = {
        {QStringLiteral("PATH"),
         QStringLiteral("Controls where the system finds executables. "
                        "Incorrect values may prevent applications from launching.")},
        {QStringLiteral("LD_LIBRARY_PATH"),
         QStringLiteral("Controls shared library lookup. "
                        "Incorrect values can cause application crashes or security risks.")},
        {QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
         QStringLiteral("Changing this may break inter-process communication "
                        "for your entire desktop session.")},
        {QStringLiteral("WAYLAND_DISPLAY"),
         QStringLiteral("Identifies the Wayland display socket. "
                        "Changing this will break Wayland applications.")},
        {QStringLiteral("DISPLAY"),
         QStringLiteral("Identifies the X11 display. "
                        "Changing this will break X11 applications.")},
        {QStringLiteral("QT_PLUGIN_PATH"),
         QStringLiteral("Controls where Qt looks for plugins. "
                        "Incorrect values may break Qt applications.")},
        {QStringLiteral("LD_PRELOAD"),
         QStringLiteral("Injects shared libraries into every process. "
                        "Can cause instability or security risks.")},
    };
    return d;
}

ValidationResult EnvValidator::validateKey(const QString &key)
{
    if (key.trimmed().isEmpty())
        return {false, QStringLiteral("none"), QStringLiteral("Key cannot be empty.")};

    static const QRegularExpression validPattern(QStringLiteral("^[A-Z_][A-Z0-9_]*$"));
    if (!validPattern.match(key).hasMatch()) {
        return {false, QStringLiteral("none"),
                QStringLiteral("Key must contain only uppercase letters (A–Z), "
                               "digits (0–9), and underscores. "
                               "It must not start with a digit.")};
    }

    if (criticalKeys().contains(key))
        return {true, QStringLiteral("critical"), sensitiveKeyDescription(key)};
    if (highKeys().contains(key))
        return {true, QStringLiteral("high"), sensitiveKeyDescription(key)};
    if (mediumKeys().contains(key))
        return {true, QStringLiteral("medium"), sensitiveKeyDescription(key)};

    return {true, QStringLiteral("none"), {}};
}

ValidationResult EnvValidator::validateValue(const QString &key, const QString &value)
{
    if (criticalKeys().contains(key) && value.trimmed().isEmpty()) {
        return {false, QStringLiteral("critical"),
                QStringLiteral("Value for %1 cannot be empty.").arg(key)};
    }
    return {true, QStringLiteral("none"), {}};
}

bool EnvValidator::isSensitiveKey(const QString &key)
{
    return criticalKeys().contains(key)
        || highKeys().contains(key)
        || mediumKeys().contains(key);
}

QString EnvValidator::sensitiveKeyDescription(const QString &key)
{
    return descriptions().value(
        key,
        QStringLiteral("Modifying %1 may affect system stability.").arg(key));
}
