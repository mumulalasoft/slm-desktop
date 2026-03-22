#pragma once

#include "slmframeworktypes.h"

namespace Slm::Actions::Framework {

struct GroupTreeNode {
    QString label;
    QString path;
    QList<GroupTreeNode> children;
    QList<ResolvedAction> actions;
};

class GroupTreeBuilder
{
public:
    GroupTreeNode build(const QList<ResolvedAction> &actions) const;

private:
    static void sortTree(GroupTreeNode *node);
};

} // namespace Slm::Actions::Framework

