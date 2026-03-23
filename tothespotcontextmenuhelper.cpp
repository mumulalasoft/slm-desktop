#include "tothespotcontextmenuhelper.h"

#include <QtGlobal>

TothespotContextMenuHelper::TothespotContextMenuHelper(QObject *parent)
    : QObject(parent)
{
}

bool TothespotContextMenuHelper::hasContainingAction(bool isApp, bool isDir, bool hasPath) const
{
    return !isApp && !isDir && hasPath;
}

bool TothespotContextMenuHelper::hasCopyPathAction(bool isApp, bool isDir, bool hasPath) const
{
    Q_UNUSED(isDir)
    return !isApp && hasPath;
}

int TothespotContextMenuHelper::visibleActionCount(bool isApp, bool isDir, bool hasPath) const
{
    if (hasContainingAction(isApp, isDir, hasPath)) {
        return 6;
    }
    if (hasCopyPathAction(isApp, isDir, hasPath)) {
        return 5;
    }
    return 2;
}

QString TothespotContextMenuHelper::actionForSlot(int slot, bool isApp, bool isDir, bool hasPath) const
{
    if (slot <= 0) {
        return isApp ? QStringLiteral("launch") : QStringLiteral("open");
    }
    if (hasContainingAction(isApp, isDir, hasPath)) {
        if (slot == 1) {
            return QStringLiteral("openContainingFolder");
        }
        if (slot == 2) {
            return QStringLiteral("copyPath");
        }
        if (slot == 3) {
            return QStringLiteral("copyName");
        }
        if (slot == 4) {
            return QStringLiteral("copyUri");
        }
        return QStringLiteral("properties");
    }
    if (hasCopyPathAction(isApp, isDir, hasPath)) {
        if (slot == 1) {
            return QStringLiteral("copyPath");
        }
        if (slot == 2) {
            return QStringLiteral("copyName");
        }
        if (slot == 3) {
            return QStringLiteral("copyUri");
        }
        return QStringLiteral("properties");
    }
    return QStringLiteral("properties");
}

int TothespotContextMenuHelper::normalizeSelectionIndex(int currentIndex, bool isApp, bool isDir, bool hasPath) const
{
    const int count = visibleActionCount(isApp, isDir, hasPath);
    if (count <= 0) {
        return -1;
    }
    if (currentIndex < 0 || currentIndex >= count) {
        return 0;
    }
    return currentIndex;
}

int TothespotContextMenuHelper::moveSelection(int currentIndex, int delta, bool isApp, bool isDir, bool hasPath) const
{
    const int count = visibleActionCount(isApp, isDir, hasPath);
    if (count <= 0) {
        return -1;
    }
    int next = normalizeSelectionIndex(currentIndex, isApp, isDir, hasPath);
    if (next < 0) {
        next = 0;
    }
    const int shift = delta % count;
    next += shift;
    while (next < 0) {
        next += count;
    }
    while (next >= count) {
        next -= count;
    }
    return next;
}
