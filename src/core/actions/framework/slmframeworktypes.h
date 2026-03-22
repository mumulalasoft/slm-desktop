#pragma once

#include "../slmactiontypes.h"
#include "../slmactioncondition.h"

#include <QVariantMap>

namespace Slm::Actions::Framework {

using CapabilityType = Slm::Actions::Capability;
using ShareCapability = Slm::Actions::ShareCapability;
using QuickActionCapability = Slm::Actions::QuickActionCapability;
using DragDropCapability = Slm::Actions::DragDropCapability;
using SearchActionCapability = Slm::Actions::SearchActionCapability;

struct ActionDefinition {
    QString id;
    QString sourceDesktopFile;
    QString desktopActionName;
    QString name;
    QString exec;
    QString icon;
    QStringList mimeTypes;
    QSet<CapabilityType> capabilities;
    int priority = 100;
    QStringList keywords;
    QStringList groupPath;
    QSharedPointer<ConditionAstNode> conditionsAst;
    QVariantMap capabilityMetadata;
};

struct ActionContext {
    QStringList uris;
    QString text;
    QString scope;
    QStringList mimeTypes;
    int selectionCount = 0;
    QString target;
    bool writable = false;
    bool executable = false;
    QString sourceApp;
};

struct ResolvedAction {
    QString actionId;
    QString name;
    QString icon;
    QString exec;
    double score = 0.0;
    QStringList groupPath;
    QVariantMap metadata;
};

inline ActionDefinition fromCoreAction(const Slm::Actions::ActionDefinition &in)
{
    ActionDefinition out;
    out.id = in.id;
    out.sourceDesktopFile = in.desktopFilePath;
    out.desktopActionName = in.desktopActionName;
    out.name = in.name;
    out.exec = in.exec;
    out.icon = in.icon;
    out.mimeTypes = in.mimeTypes;
    out.capabilities = in.capabilities;
    out.priority = in.priority;
    out.keywords = in.keywords;
    out.groupPath = in.group.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return out;
}

inline ResolvedAction toResolvedAction(const QVariantMap &row)
{
    ResolvedAction out;
    out.actionId = row.value(QStringLiteral("id")).toString();
    out.name = row.value(QStringLiteral("name")).toString();
    out.icon = row.value(QStringLiteral("icon")).toString();
    out.exec = row.value(QStringLiteral("exec")).toString();
    out.score = row.value(QStringLiteral("score")).toDouble();
    out.groupPath = row.value(QStringLiteral("group")).toString().split(QLatin1Char('/'),
                                                                        Qt::SkipEmptyParts);
    out.metadata = row;
    return out;
}

} // namespace Slm::Actions::Framework
