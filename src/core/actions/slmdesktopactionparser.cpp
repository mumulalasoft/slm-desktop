#include "slmdesktopactionparser.h"

#include <QFileInfo>

#include <glib.h>

namespace Slm::Actions {
namespace {

QString fromUtf8(const gchar *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

bool keyAsBool(GKeyFile *keyFile, const gchar *group, const gchar *key)
{
    GError *error = nullptr;
    const gboolean value = g_key_file_get_boolean(keyFile, group, key, &error);
    if (error) {
        g_error_free(error);
        error = nullptr;
        const QString raw = fromUtf8(g_key_file_get_string(keyFile, group, key, nullptr)).trimmed().toLower();
        return raw == QStringLiteral("true") || raw == QStringLiteral("1") || raw == QStringLiteral("yes");
    }
    return value != 0;
}

QString keyAsString(GKeyFile *keyFile, const gchar *group, const gchar *key)
{
    return fromUtf8(g_key_file_get_string(keyFile, group, key, nullptr)).trimmed();
}

QStringList keyAsList(GKeyFile *keyFile, const gchar *group, const gchar *key)
{
    const QString raw = keyAsString(keyFile, group, key);
    if (raw.isEmpty()) {
        return {};
    }
    QStringList out;
    const QStringList parts = raw.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    out.reserve(parts.size());
    for (const QString &part : parts) {
        const QString t = part.trimmed();
        if (!t.isEmpty()) {
            out.push_back(t);
        }
    }
    return out;
}

QString buildActionId(const QString &desktopId, const QString &actionName)
{
    return desktopId + QStringLiteral("::") + actionName;
}

} // namespace

DesktopParseResult DesktopActionParser::parseFile(const QString &desktopFilePath) const
{
    DesktopParseResult out;
    const QFileInfo fi(desktopFilePath);
    if (!fi.exists() || !fi.isFile()) {
        out.error = QStringLiteral("Desktop file not found: %1").arg(desktopFilePath);
        return out;
    }

    GError *error = nullptr;
    GKeyFile *keyFile = g_key_file_new();
    if (!g_key_file_load_from_file(keyFile,
                                   desktopFilePath.toUtf8().constData(),
                                   G_KEY_FILE_NONE,
                                   &error)) {
        out.error = error ? QString::fromUtf8(error->message) : QStringLiteral("Failed to parse desktop file");
        if (error) {
            g_error_free(error);
        }
        g_key_file_free(keyFile);
        return out;
    }

    const QString desktopId = fi.fileName();
    const QStringList mimeTypes = keyAsList(keyFile, "Desktop Entry", "MimeType");
    const QStringList actionNames = keyAsList(keyFile, "Desktop Entry", "Actions");
    for (const QString &actionName : actionNames) {
        const QString group = QStringLiteral("Desktop Action %1").arg(actionName);
        if (!g_key_file_has_group(keyFile, group.toUtf8().constData())) {
            continue;
        }

        FileAction action;
        action.id = buildActionId(desktopId, actionName);
        action.desktopId = desktopId;
        action.desktopFilePath = fi.absoluteFilePath();
        action.desktopActionName = actionName;
        action.name = keyAsString(keyFile, group.toUtf8().constData(), "Name");
        action.exec = keyAsString(keyFile, group.toUtf8().constData(), "Exec");
        action.icon = keyAsString(keyFile, group.toUtf8().constData(), "Icon");
        action.mimeTypes = mimeTypes;
        action.mtimeUtc = fi.lastModified().toUTC();

        action.capabilities = parseCapabilities(
            keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-Capabilities"));
        action.contextMenu = keyAsBool(keyFile, group.toUtf8().constData(), "X-SLM-ContextMenu");
        if (action.contextMenu) {
            action.capabilities.insert(Capability::ContextMenu);
        }
        action.targets = parseTargets(
            keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-FileAction-Targets"));
        action.group = keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-Group");
        action.priority = keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-Priority").toInt();
        action.keywords = keyAsList(keyFile, group.toUtf8().constData(), "X-SLM-Keywords");
        action.conditions = keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-FileAction-Conditions");

        if (action.capabilities.contains(Capability::Share)) {
            action.share.enabled = true;
            action.share.targets = parseTargets(
                keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-Share-Targets"));
            action.share.modes = parseShareModes(
                keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-Share-Modes"));
        }

        if (action.capabilities.contains(Capability::QuickAction)) {
            action.quickAction.enabled = true;
            action.quickAction.scopes = parseQuickActionScopes(
                keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-QuickAction-Scopes"));
            const QString argsValue = keyAsString(keyFile,
                                                  group.toUtf8().constData(),
                                                  "X-SLM-QuickAction-Args");
            if (!argsValue.isEmpty()) {
                const QuickActionArgs parsed = quickActionArgsFromString(argsValue);
                if (parsed != QuickActionArgs::Unknown) {
                    action.quickAction.args = parsed;
                }
            }
            const QString visibilityValue = keyAsString(keyFile,
                                                        group.toUtf8().constData(),
                                                        "X-SLM-QuickAction-Visibility");
            if (!visibilityValue.isEmpty()) {
                const QuickActionVisibility parsed = quickActionVisibilityFromString(visibilityValue);
                if (parsed != QuickActionVisibility::Unknown) {
                    action.quickAction.visibility = parsed;
                }
            }
        }

        // SLM OpenWith capability is intentionally disabled.
        // Native filemanager Open With remains the single source of truth.

        if (action.capabilities.contains(Capability::DragDrop)) {
            action.dragDrop.enabled = true;
            action.dragDrop.accepts = parseTargets(
                keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-DragDrop-Accepts"));
            const QString opValue = keyAsString(keyFile,
                                                group.toUtf8().constData(),
                                                "X-SLM-DragDrop-Operation");
            if (!opValue.isEmpty()) {
                const DragDropOperation op = dragDropOperationFromString(opValue);
                if (op != DragDropOperation::Unknown) {
                    action.dragDrop.operation = op;
                }
            }
            action.dragDrop.destinations = parseDragDropDestinations(
                keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-DragDrop-Destination"));
            const QString behaviorValue = keyAsString(keyFile,
                                                      group.toUtf8().constData(),
                                                      "X-SLM-DragDrop-Behavior");
            if (!behaviorValue.isEmpty()) {
                const DragDropBehavior behavior = dragDropBehaviorFromString(behaviorValue);
                if (behavior != DragDropBehavior::Unknown) {
                    action.dragDrop.behavior = behavior;
                }
            }
        }

        if (action.capabilities.contains(Capability::SearchAction)) {
            action.searchAction.enabled = true;
            action.searchAction.scopes = parseSearchActionScopes(
                keyAsString(keyFile, group.toUtf8().constData(), "X-SLM-SearchAction-Scopes"));
            action.searchAction.intent = keyAsString(
                keyFile, group.toUtf8().constData(), "X-SLM-SearchAction-Intent");
        }

        if (action.priority == 0 && !g_key_file_has_key(keyFile,
                                                        group.toUtf8().constData(),
                                                        "X-SLM-Priority",
                                                        nullptr)) {
            action.priority = 100;
        }

        if (action.name.isEmpty() || action.exec.isEmpty()) {
            continue;
        }
        if (action.capabilities.isEmpty()) {
            continue;
        }
        out.actions.push_back(action);
    }

    g_key_file_free(keyFile);
    out.ok = true;
    return out;
}

} // namespace Slm::Actions
