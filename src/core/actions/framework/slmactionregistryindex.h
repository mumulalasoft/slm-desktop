#pragma once

#include "../slmactiontypes.h"

#include <QHash>
#include <QMultiHash>
#include <QSet>

namespace Slm::Actions::Framework {

class ActionRegistryIndex
{
public:
    void clear();
    void rebuild(const QVector<ActionDefinition> &actions);

    bool contains(const QString &id) const;
    ActionDefinition byId(const QString &id) const;
    QVector<ActionDefinition> all() const;
    QVector<ActionDefinition> byCapability(Capability capability) const;
    QVector<ActionDefinition> byMime(const QString &mime) const;
    QVector<ActionDefinition> byKeyword(const QString &keyword) const;

private:
    QVector<ActionDefinition> resolveIds(const QList<QString> &ids) const;

    QHash<QString, ActionDefinition> m_byId;
    QMultiHash<int, QString> m_byCapability;
    QMultiHash<QString, QString> m_byMime;
    QMultiHash<QString, QString> m_byKeyword;
};

} // namespace Slm::Actions::Framework

