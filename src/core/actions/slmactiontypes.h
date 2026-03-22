#pragma once

#include <QDateTime>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace Slm::Actions {

enum class Capability {
    Unknown = 0,
    ContextMenu,
    Share,
    QuickAction,
    // Deprecated in SLM Action Framework.
    // Keep only for API compatibility; FileManager Open With is native-only.
    OpenWith,
    DragDrop,
    SearchAction,
};

enum class Target {
    Unknown = 0,
    File,
    Directory,
    Selection,
    Background,
    Device,
    Archive,
    Link,
    Text,
    Url,
    UriList,
};

enum class ShareMode {
    Unknown = 0,
    Send,
    Link,
    Embed,
};

enum class QuickActionScope {
    Unknown = 0,
    Launcher,
    Dock,
    AppMenu,
};

enum class QuickActionArgs {
    Unknown = 0,
    None,
    Selection,
    Clipboard,
    Text,
    Files,
};

enum class QuickActionVisibility {
    Unknown = 0,
    Always,
    Contextual,
};

enum class DragDropOperation {
    Unknown = 0,
    Copy,
    Move,
    Link,
    Ask,
};

enum class DragDropDestination {
    Unknown = 0,
    App,
    Widget,
    Window,
    Desktop,
};

enum class DragDropBehavior {
    Unknown = 0,
    Implicit,
    Chooser,
};

enum class SearchActionScope {
    Unknown = 0,
    Launcher,
    Tothespot,
    CommandPalette,
    FileSearch,
};

QString capabilityToString(Capability capability);
Capability capabilityFromString(const QString &value);
QSet<Capability> parseCapabilities(const QString &value);

QString targetToString(Target target);
Target targetFromString(const QString &value);
QSet<Target> parseTargets(const QString &value);

QString shareModeToString(ShareMode mode);
ShareMode shareModeFromString(const QString &value);
QSet<ShareMode> parseShareModes(const QString &value);

QString quickActionScopeToString(QuickActionScope scope);
QuickActionScope quickActionScopeFromString(const QString &value);
QSet<QuickActionScope> parseQuickActionScopes(const QString &value);

QString quickActionArgsToString(QuickActionArgs args);
QuickActionArgs quickActionArgsFromString(const QString &value);

QString quickActionVisibilityToString(QuickActionVisibility visibility);
QuickActionVisibility quickActionVisibilityFromString(const QString &value);

QString dragDropOperationToString(DragDropOperation op);
DragDropOperation dragDropOperationFromString(const QString &value);

QString dragDropDestinationToString(DragDropDestination destination);
QSet<DragDropDestination> parseDragDropDestinations(const QString &value);

QString dragDropBehaviorToString(DragDropBehavior behavior);
DragDropBehavior dragDropBehaviorFromString(const QString &value);

QString searchActionScopeToString(SearchActionScope scope);
SearchActionScope searchActionScopeFromString(const QString &value);
QSet<SearchActionScope> parseSearchActionScopes(const QString &value);

struct ActionItemMeta {
    QString uri;
    QString path;
    QString mime;
    QString ext;
    QString scheme;
    QString target;
    qlonglong size = 0;
    bool writable = false;
    bool executable = false;
};

struct ActionEvalContext {
    QVector<ActionItemMeta> items;
    int count = 0;
    QString target;
};

struct ShareCapability {
    bool enabled = false;
    QSet<Target> targets;
    QSet<ShareMode> modes;
};

struct QuickActionCapability {
    bool enabled = false;
    QSet<QuickActionScope> scopes;
    QuickActionArgs args = QuickActionArgs::None;
    QuickActionVisibility visibility = QuickActionVisibility::Always;
};

struct DragDropCapability {
    bool enabled = false;
    QSet<Target> accepts;
    DragDropOperation operation = DragDropOperation::Ask;
    QSet<DragDropDestination> destinations;
    DragDropBehavior behavior = DragDropBehavior::Implicit;
};

struct SearchActionCapability {
    bool enabled = false;
    QSet<SearchActionScope> scopes;
    QString intent;
};

struct ActionContext {
    QStringList uris;
    QString text;
    QString scope;
    QStringList mimeTypes;
    int selectionCount = 0;
    QString sourceApp;
    QVariantMap extras;
};

struct ActionDefinition {
    QString id;
    QString desktopId;
    QString desktopFilePath;
    QString desktopActionName;
    QString name;
    QString exec;
    QString icon;
    QStringList mimeTypes;
    QSet<Capability> capabilities;
    QSet<Target> targets;
    QString group;
    int priority = 100;
    QStringList keywords;
    QString conditions;
    bool contextMenu = false;
    QDateTime mtimeUtc;

    ShareCapability share;
    QuickActionCapability quickAction;
    DragDropCapability dragDrop;
    SearchActionCapability searchAction;
};

using FileAction = ActionDefinition;

QVariantMap toVariantMap(const FileAction &action);

} // namespace Slm::Actions
