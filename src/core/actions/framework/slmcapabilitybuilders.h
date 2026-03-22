#pragma once

#include "../slmactionregistry.h"

#include <QVariantList>

namespace Slm::Actions::Framework {

class ContextMenuBuilder
{
public:
    explicit ContextMenuBuilder(ActionRegistry *registry = nullptr);
    QVariantList build(const QVariantList &flatActions) const;

private:
    ActionRegistry *m_registry = nullptr;
};

class ShareSheetBuilder
{
public:
    explicit ShareSheetBuilder(ActionRegistry *registry = nullptr);
    QVariantList build(const QVariantList &flatActions) const;

private:
    ActionRegistry *m_registry = nullptr;
};

class QuickActionBuilder
{
public:
    QVariantList build(const QVariantList &flatActions) const;
};

class SearchActionRanker
{
public:
    explicit SearchActionRanker(ActionRegistry *registry = nullptr);
    QVariantList rank(const QVariantList &flatActions) const;

private:
    ActionRegistry *m_registry = nullptr;
};

class DragDropResolver
{
public:
    explicit DragDropResolver(ActionRegistry *registry = nullptr);
    QVariantMap resolveDefault(const QVariantMap &context) const;

private:
    ActionRegistry *m_registry = nullptr;
};

} // namespace Slm::Actions::Framework
