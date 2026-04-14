#include "contextmenuservice.h"

#include "../../core/actions/slmactiontypes.h"

#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardPaths>

namespace Slm::ContextMenu {

namespace {

// ─── mime helpers ────────────────────────────────────────────────────────────

bool isArchive(const QString &mime)
{
    static const QStringList archives = {
        QStringLiteral("application/zip"),
        QStringLiteral("application/x-tar"),
        QStringLiteral("application/x-bzip2"),
        QStringLiteral("application/gzip"),
        QStringLiteral("application/x-xz"),
        QStringLiteral("application/x-7z-compressed"),
        QStringLiteral("application/x-rar-compressed"),
        QStringLiteral("application/x-rar"),
        QStringLiteral("application/zstd"),
    };
    return archives.contains(mime);
}

bool isImage(const QString &mime)
{
    return mime.startsWith(QStringLiteral("image/"));
}

bool isVideo(const QString &mime)
{
    return mime.startsWith(QStringLiteral("video/"));
}

bool isAudio(const QString &mime)
{
    return mime.startsWith(QStringLiteral("audio/"));
}

bool isText(const QString &mime)
{
    return mime.startsWith(QStringLiteral("text/"));
}

bool isPdf(const QString &mime)
{
    return mime == QStringLiteral("application/pdf");
}

bool isIso(const QString &mime)
{
    return mime == QStringLiteral("application/x-iso9660-image")
           || mime == QStringLiteral("application/x-cd-image");
}

// ─── item helpers ────────────────────────────────────────────────────────────

QString ctxStr(const QVariantMap &ctx, const char *key)
{
    return ctx.value(QLatin1String(key)).toString();
}

bool ctxBool(const QVariantMap &ctx, const char *key)
{
    return ctx.value(QLatin1String(key)).toBool();
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

ContextMenuService::ContextMenuService(QObject *parent)
    : QObject(parent)
{
    reloadExtensions();
}

void ContextMenuService::reloadExtensions()
{
    // Scan standard .desktop directories for X-SLM-ContextMenu extensions.
    QStringList dirs;
    dirs << QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    dirs << QStringLiteral("/usr/share/slm/actions");
    dirs << QStringLiteral("/usr/local/share/slm/actions");
    const QString homeData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!homeData.isEmpty())
        dirs << homeData + QStringLiteral("/slm/actions");

    m_registry.setScanDirectories(dirs);
    m_registry.reload();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

QVariantList ContextMenuService::buildMenu(const QVariantMap &context) const
{
    const QString type = ctxStr(context, "type");

    QVariantList items;
    if (type == QStringLiteral("file")) {
        items = buildFileMenu(context);
    } else if (type == QStringLiteral("folder")) {
        items = buildFolderMenu(context);
    } else if (type == QStringLiteral("desktop")) {
        items = buildDesktopMenu(context);
    } else if (type == QStringLiteral("window")) {
        items = buildWindowMenu(context);
    } else if (type == QStringLiteral("notification")) {
        items = buildNotificationMenu(context);
    } else if (type == QStringLiteral("dock")) {
        items = buildDockMenu(context);
    }

    return applyMaxTopLevel(items);
}

QVariantList ContextMenuService::getSuggestedActions(const QVariantMap &context) const
{
    const QString mime = ctxStr(context, "mimeType");
    QVariantList suggestions;

    if (isPdf(mime)) {
        suggestions << item(QStringLiteral("open_in_editor"),
                            QStringLiteral("Open in Editor"),
                            QStringLiteral("document-edit"));
    }
    if (isVideo(mime)) {
        suggestions << item(QStringLiteral("convert_video"),
                            QStringLiteral("Convert"),
                            QStringLiteral("video-x-generic"));
    }
    if (isAudio(mime)) {
        suggestions << item(QStringLiteral("open_in_player"),
                            QStringLiteral("Open in Player"),
                            QStringLiteral("audio-x-generic"));
    }
    if (isImage(mime)) {
        suggestions << item(QStringLiteral("set_wallpaper"),
                            QStringLiteral("Set as Wallpaper"),
                            QStringLiteral("preferences-desktop-wallpaper"));
    }
    if (isText(mime)) {
        suggestions << item(QStringLiteral("open_in_ide"),
                            QStringLiteral("Open in IDE"),
                            QStringLiteral("utilities-terminal"));
    }
    if (isArchive(mime)) {
        suggestions << item(QStringLiteral("extract_here"),
                            QStringLiteral("Extract Here"),
                            QStringLiteral("archive-extract"));
    }

    return suggestions;
}

QVariantMap ContextMenuService::invokeBuiltinAction(const QString &actionId,
                                                    const QVariantMap &context) const
{
    // Built-in action dispatch is intentionally thin — the QML layer that owns
    // the surface-specific APIs (FileManagerApi, WorkspaceManager, etc.) handles
    // the actual operation.  This method is reserved for actions that don't
    // have a natural QML owner.
    Q_UNUSED(actionId)
    Q_UNUSED(context)
    return {{QStringLiteral("ok"), false},
            {QStringLiteral("error"),
             QStringLiteral("Use surface-specific QML API for built-in actions")}};
}

// ─────────────────────────────────────────────────────────────────────────────
// Surface builders
// ─────────────────────────────────────────────────────────────────────────────

QVariantList ContextMenuService::buildFileMenu(const QVariantMap &ctx) const
{
    const QString mime = ctxStr(ctx, "mimeType");
    const bool isExec = ctxBool(ctx, "isExecutable");
    const bool isMounted = ctxBool(ctx, "isMounted");

    QVariantList top;

    // 1. Open / Run
    if (isExec) {
        top << item(QStringLiteral("run"),
                    QStringLiteral("Run"),
                    QStringLiteral("system-run"), 10);
    }
    top << item(QStringLiteral("open"),
                QStringLiteral("Open"),
                QStringLiteral("document-open"), 20);

    // 2. Open With
    top << item(QStringLiteral("open_with"),
                QStringLiteral("Open With\u2026"),
                QStringLiteral("document-open"), 30);

    // 3. Preview (for media/documents)
    if (isImage(mime) || isVideo(mime) || isPdf(mime)) {
        top << item(QStringLiteral("preview"),
                    QStringLiteral("Preview"),
                    QStringLiteral("document-preview"), 35);
    }

    top << separator();

    // 4. Cut / Copy / Paste
    top << item(QStringLiteral("cut"),
                QStringLiteral("Cut"),
                QStringLiteral("edit-cut"), 40);
    top << item(QStringLiteral("copy"),
                QStringLiteral("Copy"),
                QStringLiteral("edit-copy"), 41);

    // 5. Rename
    top << item(QStringLiteral("rename"),
                QStringLiteral("Rename"),
                QStringLiteral("document-edit"), 50);

    // 6. Delete / Eject
    if (isMounted) {
        top << item(QStringLiteral("eject"),
                    QStringLiteral("Eject"),
                    QStringLiteral("media-eject"), 55);
    } else {
        top << item(QStringLiteral("trash"),
                    QStringLiteral("Move to Trash"),
                    QStringLiteral("edit-delete"), 55);
    }

    top << separator();

    // 7+: More submenu
    QVariantList more;
    more << item(QStringLiteral("duplicate"),
                 QStringLiteral("Duplicate"),
                 QStringLiteral("edit-copy"), 60);
    more << item(QStringLiteral("move_to"),
                 QStringLiteral("Move to\u2026"),
                 QStringLiteral("folder-move"), 62);
    more << item(QStringLiteral("copy_to"),
                 QStringLiteral("Copy to\u2026"),
                 QStringLiteral("folder-copy"), 63);
    more << separator();
    more << item(QStringLiteral("compress"),
                 QStringLiteral("Compress\u2026"),
                 QStringLiteral("archive-insert"), 65);

    // MIME-specific extras
    if (isArchive(mime)) {
        more << item(QStringLiteral("extract_here"),
                     QStringLiteral("Extract Here"),
                     QStringLiteral("archive-extract"), 66);
        more << item(QStringLiteral("extract_to"),
                     QStringLiteral("Extract to\u2026"),
                     QStringLiteral("archive-extract"), 67);
    }
    if (isImage(mime)) {
        more << item(QStringLiteral("set_wallpaper"),
                     QStringLiteral("Set as Wallpaper"),
                     QStringLiteral("preferences-desktop-wallpaper"), 68);
    }
    if (isMounted || isIso(mime)) {
        more << item(QStringLiteral("mount"),
                     QStringLiteral("Mount"),
                     QStringLiteral("drive-optical"), 70);
    }

    more << separator();
    more << item(QStringLiteral("properties"),
                 QStringLiteral("Properties"),
                 QStringLiteral("document-properties"), 80);
    more << item(QStringLiteral("permissions"),
                 QStringLiteral("Permissions"),
                 QStringLiteral("system-users"), 81);
    more << item(QStringLiteral("open_as_admin"),
                 QStringLiteral("Open as Administrator"),
                 QStringLiteral("security-high"), 82);

    // Inject .desktop extension actions into More.
    const QVariantList ext = fetchExtensionActions(ctx);
    if (!ext.isEmpty()) {
        more << separator();
        more << ext;
    }

    top << submenu(QStringLiteral("more"),
                   QStringLiteral("More"),
                   QStringLiteral("view-more-symbolic"),
                   more, 90);

    return top;
}

QVariantList ContextMenuService::buildFolderMenu(const QVariantMap &ctx) const
{
    const bool isNetwork = ctxBool(ctx, "isNetwork");
    const bool canPaste = ctxBool(ctx, "canPaste");

    QVariantList top;

    top << item(QStringLiteral("open"),
                QStringLiteral("Open"),
                QStringLiteral("folder-open"), 10);
    top << item(QStringLiteral("open_terminal"),
                QStringLiteral("Open Terminal Here"),
                QStringLiteral("utilities-terminal"), 15);

    top << separator();

    top << item(QStringLiteral("cut"),
                QStringLiteral("Cut"),
                QStringLiteral("edit-cut"), 20);
    top << item(QStringLiteral("copy"),
                QStringLiteral("Copy"),
                QStringLiteral("edit-copy"), 21);
    if (canPaste) {
        top << item(QStringLiteral("paste"),
                    QStringLiteral("Paste"),
                    QStringLiteral("edit-paste"), 22);
    }
    top << item(QStringLiteral("rename"),
                QStringLiteral("Rename"),
                QStringLiteral("document-edit"), 25);
    top << item(QStringLiteral("trash"),
                QStringLiteral("Move to Trash"),
                QStringLiteral("edit-delete"), 26);

    top << separator();

    QVariantList more;
    more << item(QStringLiteral("duplicate"),
                 QStringLiteral("Duplicate"),
                 QStringLiteral("edit-copy"), 30);
    more << item(QStringLiteral("move_to"),
                 QStringLiteral("Move to\u2026"),
                 QStringLiteral("folder-move"), 32);
    more << item(QStringLiteral("copy_to"),
                 QStringLiteral("Copy to\u2026"),
                 QStringLiteral("folder-copy"), 33);
    if (isNetwork) {
        more << item(QStringLiteral("mount_network"),
                     QStringLiteral("Mount Network Drive"),
                     QStringLiteral("network-server"), 38);
    }
    more << separator();
    more << item(QStringLiteral("properties"),
                 QStringLiteral("Properties"),
                 QStringLiteral("document-properties"), 40);
    more << item(QStringLiteral("permissions"),
                 QStringLiteral("Permissions"),
                 QStringLiteral("system-users"), 41);
    more << item(QStringLiteral("open_as_admin"),
                 QStringLiteral("Open as Administrator"),
                 QStringLiteral("security-high"), 42);

    const QVariantList ext = fetchExtensionActions(ctx);
    if (!ext.isEmpty()) {
        more << separator();
        more << ext;
    }

    top << submenu(QStringLiteral("more"),
                   QStringLiteral("More"),
                   QStringLiteral("view-more-symbolic"),
                   more, 90);

    return top;
}

QVariantList ContextMenuService::buildDesktopMenu(const QVariantMap &ctx) const
{
    const bool canPaste = ctxBool(ctx, "canPaste");

    QVariantList top;

    // New file / folder
    QVariantList newItems;
    newItems << item(QStringLiteral("new_folder"),
                     QStringLiteral("Folder"),
                     QStringLiteral("folder-new"), 10);
    newItems << item(QStringLiteral("new_text_file"),
                     QStringLiteral("Text File"),
                     QStringLiteral("document-new"), 11);
    top << submenu(QStringLiteral("new"),
                   QStringLiteral("New"),
                   QStringLiteral("document-new"),
                   newItems, 10);

    if (canPaste) {
        top << item(QStringLiteral("paste"),
                    QStringLiteral("Paste"),
                    QStringLiteral("edit-paste"), 20);
    }

    top << separator();

    top << item(QStringLiteral("change_wallpaper"),
                QStringLiteral("Change Wallpaper\u2026"),
                QStringLiteral("preferences-desktop-wallpaper"), 30);
    top << item(QStringLiteral("appearance"),
                QStringLiteral("Appearance\u2026"),
                QStringLiteral("preferences-desktop-theme"), 31);

    top << separator();

    top << item(QStringLiteral("settings"),
                QStringLiteral("Settings"),
                QStringLiteral("preferences-system"), 40);
    top << item(QStringLiteral("open_terminal"),
                QStringLiteral("Open Terminal Here"),
                QStringLiteral("utilities-terminal"), 41);

    return top;
}

QVariantList ContextMenuService::buildWindowMenu(const QVariantMap &ctx) const
{
    const QString appId = ctxStr(ctx, "appId");
    const bool hasAppId = !appId.isEmpty();

    QVariantList top;

    top << item(QStringLiteral("minimize"),
                QStringLiteral("Minimize"),
                QStringLiteral("window-minimize"), 10);
    top << item(QStringLiteral("maximize"),
                QStringLiteral("Maximize"),
                QStringLiteral("window-maximize"), 11);
    top << item(QStringLiteral("fullscreen"),
                QStringLiteral("Fullscreen"),
                QStringLiteral("view-fullscreen"), 12);

    top << separator();

    top << item(QStringLiteral("move_to_workspace"),
                QStringLiteral("Move to Workspace"),
                QStringLiteral("view-grid-symbolic"), 20);
    top << item(QStringLiteral("move_to_display"),
                QStringLiteral("Move to Display"),
                QStringLiteral("display"), 21);

    top << separator();

    top << item(QStringLiteral("always_on_top"),
                QStringLiteral("Always on Top"),
                QStringLiteral("go-top"), 30);

    if (hasAppId) {
        top << item(QStringLiteral("focus_app"),
                    QStringLiteral("Focus App"),
                    QStringLiteral("window-restore"), 31);
        top << item(QStringLiteral("reveal_in_dock"),
                    QStringLiteral("Reveal in Dock"),
                    QStringLiteral("go-bottom"), 32);
        top << item(QStringLiteral("pin_to_dock"),
                    QStringLiteral("Pin to Dock"),
                    QStringLiteral("view-pin"), 33);
    }

    top << separator();

    top << item(QStringLiteral("close"),
                QStringLiteral("Close"),
                QStringLiteral("window-close"), 99);

    return top;
}

QVariantList ContextMenuService::buildNotificationMenu(const QVariantMap &ctx) const
{
    const bool canReply = ctxBool(ctx, "canReply");
    const QString appId = ctxStr(ctx, "appId");

    QVariantList top;

    if (canReply) {
        top << item(QStringLiteral("reply"),
                    QStringLiteral("Reply"),
                    QStringLiteral("mail-reply-sender"), 10);
        top << separator();
    }

    top << item(QStringLiteral("mute_app"),
                QStringLiteral("Mute App"),
                QStringLiteral("audio-volume-muted"), 20);
    top << item(QStringLiteral("turn_off_notifications"),
                QStringLiteral("Turn Off Notifications"),
                QStringLiteral("notification-disabled-symbolic"), 21);

    top << separator();

    if (!appId.isEmpty()) {
        top << item(QStringLiteral("open_app"),
                    QStringLiteral("Open App"),
                    QStringLiteral("application-x-executable"), 30);
    }

    return top;
}

QVariantList ContextMenuService::buildDockMenu(const QVariantMap &ctx) const
{
    const QString appId = ctxStr(ctx, "appId");
    const bool isRunning = ctxBool(ctx, "isRunning");
    const bool isPinned = ctxBool(ctx, "isPinned");

    QVariantList top;

    if (isRunning) {
        top << item(QStringLiteral("focus_app"),
                    QStringLiteral("Focus"),
                    QStringLiteral("window-restore"), 10);
        top << item(QStringLiteral("move_to_workspace"),
                    QStringLiteral("Move to Workspace"),
                    QStringLiteral("view-grid-symbolic"), 11);
    } else {
        top << item(QStringLiteral("launch"),
                    QStringLiteral("Launch"),
                    QStringLiteral("system-run"), 10);
    }

    top << separator();

    if (isPinned) {
        top << item(QStringLiteral("unpin_from_dock"),
                    QStringLiteral("Unpin"),
                    QStringLiteral("view-unpin"), 20);
    } else {
        top << item(QStringLiteral("pin_to_dock"),
                    QStringLiteral("Pin to Dock"),
                    QStringLiteral("view-pin"), 20);
    }

    if (isRunning && !appId.isEmpty()) {
        top << item(QStringLiteral("quit_app"),
                    QStringLiteral("Quit"),
                    QStringLiteral("application-exit"), 30);
    }

    return top;
}

// ─────────────────────────────────────────────────────────────────────────────
// Extension actions
// ─────────────────────────────────────────────────────────────────────────────

QVariantList ContextMenuService::fetchExtensionActions(const QVariantMap &ctx) const
{
    const QStringList uris = [&]() {
        QStringList result;
        const QString uri = ctx.value(QStringLiteral("uri")).toString();
        if (!uri.isEmpty())
            result << uri;
        const QVariantList sel = ctx.value(QStringLiteral("selection")).toList();
        for (const QVariant &s : sel) {
            const QString u = s.toMap().value(QStringLiteral("uri")).toString();
            if (!u.isEmpty())
                result << u;
        }
        return result;
    }();

    if (uris.isEmpty())
        return {};

    const QVariantMap actionCtx = {
        {QStringLiteral("uris"), uris},
        {QStringLiteral("mime_types"),
         QStringList{ctx.value(QStringLiteral("mimeType")).toString()}},
        {QStringLiteral("target"),
         ctx.value(QStringLiteral("isDirectory")).toBool()
             ? QStringLiteral("directory")
             : QStringLiteral("file")},
    };

    return m_registry.listActionsWithContext(QStringLiteral("ContextMenu"), actionCtx);
}

// ─────────────────────────────────────────────────────────────────────────────
// Max-7 enforcement
// ─────────────────────────────────────────────────────────────────────────────

QVariantList ContextMenuService::applyMaxTopLevel(QVariantList items)
{
    // Count non-separator top-level items.
    int visibleCount = 0;
    for (const QVariant &v : items) {
        const QVariantMap m = v.toMap();
        if (!m.value(QStringLiteral("isSeparator")).toBool())
            ++visibleCount;
    }

    if (visibleCount <= kMaxTopLevel)
        return items;

    // Move items beyond the 7th visible item into the overflow "More..." submenu.
    // If there's already a "more" submenu, inject into its children.
    QVariantList kept;
    QVariantList overflow;
    int counted = 0;

    for (const QVariant &v : items) {
        const QVariantMap m = v.toMap();
        const bool isSep = m.value(QStringLiteral("isSeparator")).toBool();
        const QString id = m.value(QStringLiteral("id")).toString();

        if (id == QStringLiteral("more")) {
            // Existing More submenu — append overflow into its children later.
            kept << v;
            continue;
        }
        if (!isSep && counted < kMaxTopLevel) {
            kept << v;
            ++counted;
        } else if (!isSep) {
            overflow << v;
        } else if (counted < kMaxTopLevel) {
            kept << v;
        }
    }

    if (overflow.isEmpty())
        return kept;

    // Find existing More submenu and inject overflow into it.
    for (int i = 0; i < kept.size(); ++i) {
        QVariantMap m = kept[i].toMap();
        if (m.value(QStringLiteral("id")).toString() == QStringLiteral("more")) {
            QVariantList children = m.value(QStringLiteral("children")).toList();
            children << separator();
            children << overflow;
            m[QStringLiteral("children")] = children;
            kept[i] = m;
            return kept;
        }
    }

    // No More submenu present — append one.
    kept << submenu(QStringLiteral("more"),
                    QStringLiteral("More"),
                    QStringLiteral("view-more-symbolic"),
                    overflow, 90);
    return kept;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory helpers
// ─────────────────────────────────────────────────────────────────────────────

QVariantMap ContextMenuService::item(const QString &id,
                                     const QString &label,
                                     const QString &icon,
                                     int priority,
                                     bool enabled)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("label"), label},
        {QStringLiteral("icon"), icon},
        {QStringLiteral("priority"), priority},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("visible"), true},
        {QStringLiteral("isSeparator"), false},
        {QStringLiteral("children"), QVariantList{}},
    };
}

QVariantMap ContextMenuService::submenu(const QString &id,
                                        const QString &label,
                                        const QString &icon,
                                        const QVariantList &children,
                                        int priority)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("label"), label},
        {QStringLiteral("icon"), icon},
        {QStringLiteral("priority"), priority},
        {QStringLiteral("enabled"), !children.isEmpty()},
        {QStringLiteral("visible"), true},
        {QStringLiteral("isSeparator"), false},
        {QStringLiteral("children"), children},
    };
}

QVariantMap ContextMenuService::separator()
{
    return {{QStringLiteral("isSeparator"), true},
            {QStringLiteral("visible"), true}};
}

} // namespace Slm::ContextMenu
