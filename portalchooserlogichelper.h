#pragma once

#include <QObject>
#include <QString>

class PortalChooserLogicHelper : public QObject
{
    Q_OBJECT

public:
    explicit PortalChooserLogicHelper(QObject *parent = nullptr);

    Q_INVOKABLE bool shouldGoUpOnBackspace(bool dirFieldFocused, bool saveNameFieldFocused) const;
    Q_INVOKABLE bool shouldGoUpOnAltUp(bool dirFieldFocused, bool saveNameFieldFocused) const;
    Q_INVOKABLE bool shouldLoadDirectoryOnEnter(bool dirFieldFocused) const;
    Q_INVOKABLE bool shouldTriggerPrimaryOnEnter(bool dirFieldFocused) const;
    Q_INVOKABLE bool shouldCancelOnEscape() const;
    Q_INVOKABLE bool canPrimaryAction(const QString &mode,
                                      bool selectFolders,
                                      const QString &name,
                                      const QString &currentDir,
                                      int selectedCount) const;
};
