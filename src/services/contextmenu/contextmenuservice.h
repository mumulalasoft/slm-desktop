#pragma once

#include "../../core/actions/slmactionregistry.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace Slm::ContextMenu {

// Maximum number of top-level items before overflow goes to "More..." submenu.
inline constexpr int kMaxTopLevel = 7;

class ContextMenuService : public QObject
{
    Q_OBJECT

public:
    explicit ContextMenuService(QObject *parent = nullptr);

    // Reload .desktop action definitions from the standard system directories.
    // Called automatically on construction; call again after .desktop changes.
    Q_INVOKABLE void reloadExtensions();

    // Build the menu model for a given surface context.
    // context keys:
    //   type        — "file" | "folder" | "desktop" | "window" | "notification" | "dock"
    //   mimeType    — MIME type of the target (for file/folder)
    //   filePath    — absolute path (for file/folder)
    //   uri         — URI of the target
    //   isExecutable, isDirectory, isMounted, isNetwork — bool flags
    //   appId       — active/target application ID
    //   workspaceId — current workspace
    //   windowId    — target window ID (for window surface)
    //   notificationId — notification ID (for notification surface)
    //   canPaste    — bool, clipboard has pasteable content (for desktop/folder)
    //   selection   — QVariantList of {uri, mime, path} for multi-selection
    Q_INVOKABLE QVariantList buildMenu(const QVariantMap &context) const;

    // Smart suggestions for a given context (top-N most relevant actions).
    Q_INVOKABLE QVariantList getSuggestedActions(const QVariantMap &context) const;

    // Invoke a built-in action. Returns {ok, error}.
    // Extension actions are dispatched through ActionRegistry directly from QML.
    Q_INVOKABLE QVariantMap invokeBuiltinAction(const QString &actionId,
                                                const QVariantMap &context) const;

private:
    QVariantList buildFileMenu(const QVariantMap &ctx) const;
    QVariantList buildFolderMenu(const QVariantMap &ctx) const;
    QVariantList buildDesktopMenu(const QVariantMap &ctx) const;
    QVariantList buildWindowMenu(const QVariantMap &ctx) const;
    QVariantList buildNotificationMenu(const QVariantMap &ctx) const;
    QVariantList buildDockMenu(const QVariantMap &ctx) const;

    // Fetch .desktop extension actions for the given context from ActionRegistry.
    QVariantList fetchExtensionActions(const QVariantMap &ctx) const;

    // Enforce max-7 top-level rule: items beyond kMaxTopLevel go into "More..." submenu.
    static QVariantList applyMaxTopLevel(QVariantList items);

    // Factory helpers.
    static QVariantMap item(const QString &id,
                            const QString &label,
                            const QString &icon = {},
                            int priority = 100,
                            bool enabled = true);
    static QVariantMap submenu(const QString &id,
                               const QString &label,
                               const QString &icon,
                               const QVariantList &children,
                               int priority = 100);
    static QVariantMap separator();

    mutable Slm::Actions::ActionRegistry m_registry;
};

} // namespace Slm::ContextMenu
