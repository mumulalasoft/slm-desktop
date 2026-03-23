#pragma once

#include <QObject>
#include <QString>

class TothespotContextMenuHelper : public QObject
{
    Q_OBJECT

public:
    explicit TothespotContextMenuHelper(QObject *parent = nullptr);

    Q_INVOKABLE bool hasContainingAction(bool isApp, bool isDir, bool hasPath) const;
    Q_INVOKABLE bool hasCopyPathAction(bool isApp, bool isDir, bool hasPath) const;
    Q_INVOKABLE int visibleActionCount(bool isApp, bool isDir, bool hasPath) const;
    Q_INVOKABLE QString actionForSlot(int slot, bool isApp, bool isDir, bool hasPath) const;
    Q_INVOKABLE int normalizeSelectionIndex(int currentIndex, bool isApp, bool isDir, bool hasPath) const;
    Q_INVOKABLE int moveSelection(int currentIndex, int delta, bool isApp, bool isDir, bool hasPath) const;
};
