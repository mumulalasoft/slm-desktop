#include "slmgrouptreebuilder.h"

#include <algorithm>

namespace Slm::Actions::Framework {
namespace {

GroupTreeNode *ensureChild(GroupTreeNode *parent, const QString &label, const QString &path)
{
    for (GroupTreeNode &child : parent->children) {
        if (child.label == label && child.path == path) {
            return &child;
        }
    }
    parent->children.push_back(GroupTreeNode{label, path, {}, {}});
    return &parent->children.last();
}

} // namespace

GroupTreeNode GroupTreeBuilder::build(const QList<ResolvedAction> &actions) const
{
    GroupTreeNode root;
    root.label = QStringLiteral("root");
    root.path = QString();

    for (const ResolvedAction &action : actions) {
        GroupTreeNode *cursor = &root;
        QString path;
        for (const QString &segment : action.groupPath) {
            path = path.isEmpty() ? segment : (path + QLatin1Char('/') + segment);
            cursor = ensureChild(cursor, segment, path);
        }
        cursor->actions.push_back(action);
    }

    sortTree(&root);
    return root;
}

void GroupTreeBuilder::sortTree(GroupTreeNode *node)
{
    if (!node) {
        return;
    }
    std::sort(node->children.begin(), node->children.end(),
              [](const GroupTreeNode &a, const GroupTreeNode &b) {
                  return QString::localeAwareCompare(a.label, b.label) < 0;
              });
    std::sort(node->actions.begin(), node->actions.end(),
              [](const ResolvedAction &a, const ResolvedAction &b) {
                  if (a.score != b.score) {
                      return a.score > b.score;
                  }
                  return QString::localeAwareCompare(a.name, b.name) < 0;
              });
    for (GroupTreeNode &child : node->children) {
        sortTree(&child);
    }
}

} // namespace Slm::Actions::Framework

