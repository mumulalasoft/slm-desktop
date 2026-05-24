/*
 * Calamares entry QML for the slm-disk-select viewmodule.
 *
 * The plugin's QRC ships this file at the same path prefix as the six
 * SlmInstaller components (InstallerTheme, InstallerCard,
 * InstallerPrimaryButton, InstallerDiskCard, PartitionDiagram,
 * DiskSelectionScreen), so the directory-import below resolves the
 * qmldir-declared singleton and components without a system QML path.
 *
 * The `config` context property is set by SlmDiskSelectViewStep::widget();
 * it owns the disk model and the commit/refresh/quit slots.
 */
import QtQuick
import "."

DiskSelectionScreen {
    id: screen
    anchors.fill: parent

    model: config.diskModel
    isRefreshing: config.isRefreshing

    // Unidirectional state flow: the screen is the source of truth for
    // selectedPath; we forward changes to the Config so the ViewStep can
    // update `next`-button enablement. We never bind selectedPath FROM
    // config — that would establish a binding that user clicks would
    // silently break.
    onSelectedPathChanged: config.selectedPath = selectedPath
    onContinueRequested: function (path) { config.commit(path) }
    onRefreshRequested: config.refresh()
    onQuitRequested: config.quit()
}
