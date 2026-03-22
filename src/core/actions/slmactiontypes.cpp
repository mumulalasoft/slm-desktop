#include "slmactiontypes.h"

#include <QStringView>

namespace Slm::Actions {
namespace {

QString normalize(const QString &v)
{
    return v.trimmed().toLower();
}

QStringList splitList(const QString &value)
{
    QStringList out;
    const QStringList tokens = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    out.reserve(tokens.size());
    for (const QString &token : tokens) {
        const QString trimmed = token.trimmed();
        if (!trimmed.isEmpty()) {
            out.push_back(trimmed);
        }
    }
    return out;
}

} // namespace

QString capabilityToString(Capability capability)
{
    switch (capability) {
    case Capability::ContextMenu: return QStringLiteral("ContextMenu");
    case Capability::Share: return QStringLiteral("Share");
    case Capability::QuickAction: return QStringLiteral("QuickAction");
    // Kept for compatibility; parsing OpenWith from desktop metadata is disabled.
    case Capability::OpenWith: return QStringLiteral("OpenWith");
    case Capability::DragDrop: return QStringLiteral("DragDrop");
    case Capability::SearchAction: return QStringLiteral("SearchAction");
    default: break;
    }
    return QStringLiteral("Unknown");
}

Capability capabilityFromString(const QString &value)
{
    const QString n = normalize(value);
    if (n == QStringLiteral("contextmenu")) return Capability::ContextMenu;
    if (n == QStringLiteral("share")) return Capability::Share;
    if (n == QStringLiteral("quickaction")) return Capability::QuickAction;
    // OpenWith is handled by the native filemanager Open With pipeline.
    // Keep enum/type for ABI compatibility but disable it in SLM framework.
    if (n == QStringLiteral("openwith")) return Capability::Unknown;
    if (n == QStringLiteral("dragdrop")) return Capability::DragDrop;
    if (n == QStringLiteral("searchaction")) return Capability::SearchAction;
    return Capability::Unknown;
}

QSet<Capability> parseCapabilities(const QString &value)
{
    QSet<Capability> out;
    const QStringList tokens = splitList(value);
    for (const QString &token : tokens) {
        const Capability c = capabilityFromString(token);
        if (c != Capability::Unknown) {
            out.insert(c);
        }
    }
    return out;
}

QString targetToString(Target target)
{
    switch (target) {
    case Target::File: return QStringLiteral("file");
    case Target::Directory: return QStringLiteral("directory");
    case Target::Selection: return QStringLiteral("selection");
    case Target::Background: return QStringLiteral("background");
    case Target::Device: return QStringLiteral("device");
    case Target::Archive: return QStringLiteral("archive");
    case Target::Link: return QStringLiteral("link");
    case Target::Text: return QStringLiteral("text");
    case Target::Url: return QStringLiteral("url");
    case Target::UriList: return QStringLiteral("uri-list");
    default: break;
    }
    return QStringLiteral("unknown");
}

Target targetFromString(const QString &value)
{
    const QString n = normalize(value);
    if (n == QStringLiteral("file")) return Target::File;
    if (n == QStringLiteral("directory")) return Target::Directory;
    if (n == QStringLiteral("selection")) return Target::Selection;
    if (n == QStringLiteral("background")) return Target::Background;
    if (n == QStringLiteral("device")) return Target::Device;
    if (n == QStringLiteral("archive")) return Target::Archive;
    if (n == QStringLiteral("link")) return Target::Link;
    if (n == QStringLiteral("text")) return Target::Text;
    if (n == QStringLiteral("url")) return Target::Url;
    if (n == QStringLiteral("uri-list")) return Target::UriList;
    return Target::Unknown;
}

QSet<Target> parseTargets(const QString &value)
{
    QSet<Target> out;
    const QStringList tokens = splitList(value);
    for (const QString &token : tokens) {
        const Target t = targetFromString(token);
        if (t != Target::Unknown) {
            out.insert(t);
        }
    }
    return out;
}

QString shareModeToString(ShareMode mode)
{
    switch (mode) {
    case ShareMode::Send: return QStringLiteral("send");
    case ShareMode::Link: return QStringLiteral("link");
    case ShareMode::Embed: return QStringLiteral("embed");
    default: break;
    }
    return QStringLiteral("unknown");
}

ShareMode shareModeFromString(const QString &value)
{
    const QString n = normalize(value);
    if (n == QStringLiteral("send")) return ShareMode::Send;
    if (n == QStringLiteral("link")) return ShareMode::Link;
    if (n == QStringLiteral("embed")) return ShareMode::Embed;
    return ShareMode::Unknown;
}

QSet<ShareMode> parseShareModes(const QString &value)
{
    QSet<ShareMode> out;
    const QStringList tokens = splitList(value);
    for (const QString &token : tokens) {
        const ShareMode mode = shareModeFromString(token);
        if (mode != ShareMode::Unknown) {
            out.insert(mode);
        }
    }
    return out;
}

QString quickActionScopeToString(QuickActionScope scope)
{
    switch (scope) {
    case QuickActionScope::Launcher: return QStringLiteral("launcher");
    case QuickActionScope::Dock: return QStringLiteral("dock");
    case QuickActionScope::AppMenu: return QStringLiteral("appmenu");
    default: break;
    }
    return QStringLiteral("unknown");
}

QuickActionScope quickActionScopeFromString(const QString &value)
{
    const QString n = normalize(value);
    if (n == QStringLiteral("launcher")) return QuickActionScope::Launcher;
    if (n == QStringLiteral("dock")) return QuickActionScope::Dock;
    if (n == QStringLiteral("appmenu")) return QuickActionScope::AppMenu;
    return QuickActionScope::Unknown;
}

QSet<QuickActionScope> parseQuickActionScopes(const QString &value)
{
    QSet<QuickActionScope> out;
    const QStringList tokens = splitList(value);
    for (const QString &token : tokens) {
        const QuickActionScope scope = quickActionScopeFromString(token);
        if (scope != QuickActionScope::Unknown) {
            out.insert(scope);
        }
    }
    return out;
}

QString quickActionArgsToString(QuickActionArgs args)
{
    switch (args) {
    case QuickActionArgs::None: return QStringLiteral("none");
    case QuickActionArgs::Selection: return QStringLiteral("selection");
    case QuickActionArgs::Clipboard: return QStringLiteral("clipboard");
    case QuickActionArgs::Text: return QStringLiteral("text");
    case QuickActionArgs::Files: return QStringLiteral("files");
    default: break;
    }
    return QStringLiteral("unknown");
}

QuickActionArgs quickActionArgsFromString(const QString &value)
{
    const QString n = normalize(value);
    if (n == QStringLiteral("none")) return QuickActionArgs::None;
    if (n == QStringLiteral("selection")) return QuickActionArgs::Selection;
    if (n == QStringLiteral("clipboard")) return QuickActionArgs::Clipboard;
    if (n == QStringLiteral("text")) return QuickActionArgs::Text;
    if (n == QStringLiteral("files")) return QuickActionArgs::Files;
    return QuickActionArgs::Unknown;
}

QString quickActionVisibilityToString(QuickActionVisibility visibility)
{
    switch (visibility) {
    case QuickActionVisibility::Always: return QStringLiteral("always");
    case QuickActionVisibility::Contextual: return QStringLiteral("contextual");
    default: break;
    }
    return QStringLiteral("unknown");
}

QuickActionVisibility quickActionVisibilityFromString(const QString &value)
{
    const QString n = normalize(value);
    if (n == QStringLiteral("always")) return QuickActionVisibility::Always;
    if (n == QStringLiteral("contextual")) return QuickActionVisibility::Contextual;
    return QuickActionVisibility::Unknown;
}

QString dragDropOperationToString(DragDropOperation op)
{
    switch (op) {
    case DragDropOperation::Copy: return QStringLiteral("copy");
    case DragDropOperation::Move: return QStringLiteral("move");
    case DragDropOperation::Link: return QStringLiteral("link");
    case DragDropOperation::Ask: return QStringLiteral("ask");
    default: break;
    }
    return QStringLiteral("unknown");
}

DragDropOperation dragDropOperationFromString(const QString &value)
{
    const QString n = normalize(value);
    if (n == QStringLiteral("copy")) return DragDropOperation::Copy;
    if (n == QStringLiteral("move")) return DragDropOperation::Move;
    if (n == QStringLiteral("link")) return DragDropOperation::Link;
    if (n == QStringLiteral("ask")) return DragDropOperation::Ask;
    return DragDropOperation::Unknown;
}

QString dragDropDestinationToString(DragDropDestination destination)
{
    switch (destination) {
    case DragDropDestination::App: return QStringLiteral("app");
    case DragDropDestination::Widget: return QStringLiteral("widget");
    case DragDropDestination::Window: return QStringLiteral("window");
    case DragDropDestination::Desktop: return QStringLiteral("desktop");
    default: break;
    }
    return QStringLiteral("unknown");
}

QSet<DragDropDestination> parseDragDropDestinations(const QString &value)
{
    QSet<DragDropDestination> out;
    const QStringList tokens = splitList(value);
    for (const QString &token : tokens) {
        const QString n = normalize(token);
        DragDropDestination destination = DragDropDestination::Unknown;
        if (n == QStringLiteral("app")) {
            destination = DragDropDestination::App;
        } else if (n == QStringLiteral("widget")) {
            destination = DragDropDestination::Widget;
        } else if (n == QStringLiteral("window")) {
            destination = DragDropDestination::Window;
        } else if (n == QStringLiteral("desktop")) {
            destination = DragDropDestination::Desktop;
        }
        if (destination != DragDropDestination::Unknown) {
            out.insert(destination);
        }
    }
    return out;
}

QString dragDropBehaviorToString(DragDropBehavior behavior)
{
    switch (behavior) {
    case DragDropBehavior::Implicit: return QStringLiteral("implicit");
    case DragDropBehavior::Chooser: return QStringLiteral("chooser");
    default: break;
    }
    return QStringLiteral("unknown");
}

DragDropBehavior dragDropBehaviorFromString(const QString &value)
{
    const QString n = normalize(value);
    if (n == QStringLiteral("implicit")) return DragDropBehavior::Implicit;
    if (n == QStringLiteral("chooser")) return DragDropBehavior::Chooser;
    return DragDropBehavior::Unknown;
}

QString searchActionScopeToString(SearchActionScope scope)
{
    switch (scope) {
    case SearchActionScope::Launcher: return QStringLiteral("launcher");
    case SearchActionScope::Tothespot: return QStringLiteral("tothespot");
    case SearchActionScope::CommandPalette: return QStringLiteral("commandpalette");
    case SearchActionScope::FileSearch: return QStringLiteral("filesearch");
    default: break;
    }
    return QStringLiteral("unknown");
}

SearchActionScope searchActionScopeFromString(const QString &value)
{
    const QString n = normalize(value);
    if (n == QStringLiteral("launcher")) return SearchActionScope::Launcher;
    if (n == QStringLiteral("tothespot") || n == QStringLiteral("spotlight")) {
        return SearchActionScope::Tothespot;
    }
    if (n == QStringLiteral("commandpalette")) return SearchActionScope::CommandPalette;
    if (n == QStringLiteral("filesearch")) return SearchActionScope::FileSearch;
    return SearchActionScope::Unknown;
}

QSet<SearchActionScope> parseSearchActionScopes(const QString &value)
{
    QSet<SearchActionScope> out;
    const QStringList tokens = splitList(value);
    for (const QString &token : tokens) {
        const SearchActionScope scope = searchActionScopeFromString(token);
        if (scope != SearchActionScope::Unknown) {
            out.insert(scope);
        }
    }
    return out;
}

QVariantMap toVariantMap(const FileAction &action)
{
    QStringList caps;
    caps.reserve(action.capabilities.size());
    for (const Capability c : action.capabilities) {
        caps.push_back(capabilityToString(c));
    }
    caps.sort();

    QStringList targets;
    targets.reserve(action.targets.size());
    for (const Target t : action.targets) {
        targets.push_back(targetToString(t));
    }
    targets.sort();

    auto setToSortedStrings = [](const auto &setValue, auto toStringFn) {
        QStringList out;
        out.reserve(setValue.size());
        for (const auto &entry : setValue) {
            out.push_back(toStringFn(entry));
        }
        out.sort();
        return out;
    };

    QVariantMap map;
    map.insert(QStringLiteral("id"), action.id);
    map.insert(QStringLiteral("desktopId"), action.desktopId);
    map.insert(QStringLiteral("desktopFilePath"), action.desktopFilePath);
    map.insert(QStringLiteral("desktopActionName"), action.desktopActionName);
    map.insert(QStringLiteral("name"), action.name);
    map.insert(QStringLiteral("exec"), action.exec);
    map.insert(QStringLiteral("icon"), action.icon);
    map.insert(QStringLiteral("mimeTypes"), action.mimeTypes);
    map.insert(QStringLiteral("capabilities"), caps);
    map.insert(QStringLiteral("targets"), targets);
    map.insert(QStringLiteral("group"), action.group);
    map.insert(QStringLiteral("priority"), action.priority);
    map.insert(QStringLiteral("keywords"), action.keywords);
    map.insert(QStringLiteral("conditions"), action.conditions);
    map.insert(QStringLiteral("contextMenu"), action.contextMenu);
    map.insert(QStringLiteral("mtimeUtc"), action.mtimeUtc.toString(Qt::ISODateWithMs));

    QVariantMap share;
    share.insert(QStringLiteral("enabled"), action.share.enabled);
    share.insert(QStringLiteral("targets"), setToSortedStrings(action.share.targets, targetToString));
    share.insert(QStringLiteral("modes"), setToSortedStrings(action.share.modes, shareModeToString));
    map.insert(QStringLiteral("share"), share);

    QVariantMap quickAction;
    quickAction.insert(QStringLiteral("enabled"), action.quickAction.enabled);
    quickAction.insert(QStringLiteral("scopes"), setToSortedStrings(action.quickAction.scopes, quickActionScopeToString));
    quickAction.insert(QStringLiteral("args"), quickActionArgsToString(action.quickAction.args));
    quickAction.insert(QStringLiteral("visibility"), quickActionVisibilityToString(action.quickAction.visibility));
    map.insert(QStringLiteral("quickAction"), quickAction);

    QVariantMap dragDrop;
    dragDrop.insert(QStringLiteral("enabled"), action.dragDrop.enabled);
    dragDrop.insert(QStringLiteral("accepts"), setToSortedStrings(action.dragDrop.accepts, targetToString));
    dragDrop.insert(QStringLiteral("operation"), dragDropOperationToString(action.dragDrop.operation));
    dragDrop.insert(QStringLiteral("destinations"),
                    setToSortedStrings(action.dragDrop.destinations, dragDropDestinationToString));
    dragDrop.insert(QStringLiteral("behavior"), dragDropBehaviorToString(action.dragDrop.behavior));
    map.insert(QStringLiteral("dragDrop"), dragDrop);

    QVariantMap searchAction;
    searchAction.insert(QStringLiteral("enabled"), action.searchAction.enabled);
    searchAction.insert(QStringLiteral("scopes"), setToSortedStrings(action.searchAction.scopes, searchActionScopeToString));
    searchAction.insert(QStringLiteral("intent"), action.searchAction.intent);
    map.insert(QStringLiteral("searchAction"), searchAction);
    return map;
}

} // namespace Slm::Actions
