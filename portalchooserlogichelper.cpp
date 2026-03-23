#include "portalchooserlogichelper.h"

PortalChooserLogicHelper::PortalChooserLogicHelper(QObject *parent)
    : QObject(parent)
{
}

bool PortalChooserLogicHelper::shouldGoUpOnBackspace(bool dirFieldFocused, bool saveNameFieldFocused) const
{
    return !dirFieldFocused && !saveNameFieldFocused;
}

bool PortalChooserLogicHelper::shouldGoUpOnAltUp(bool dirFieldFocused, bool saveNameFieldFocused) const
{
    Q_UNUSED(dirFieldFocused)
    Q_UNUSED(saveNameFieldFocused)
    return true;
}

bool PortalChooserLogicHelper::shouldLoadDirectoryOnEnter(bool dirFieldFocused) const
{
    return dirFieldFocused;
}

bool PortalChooserLogicHelper::shouldTriggerPrimaryOnEnter(bool dirFieldFocused) const
{
    return !shouldLoadDirectoryOnEnter(dirFieldFocused);
}

bool PortalChooserLogicHelper::shouldCancelOnEscape() const
{
    return true;
}

bool PortalChooserLogicHelper::canPrimaryAction(const QString &mode,
                                                bool selectFolders,
                                                const QString &name,
                                                const QString &currentDir,
                                                int selectedCount) const
{
    const QString m = mode.trimmed().toLower();
    if (m == QStringLiteral("save")) {
        return !name.trimmed().isEmpty();
    }
    if (selectFolders) {
        return !currentDir.trimmed().isEmpty();
    }
    return selectedCount > 0;
}
