/*
 * Calamares entry QML for the slm-summary viewmodule. Same QRC strategy
 * as disk-select.qml: all SlmInstaller components plus this file are
 * bundled at qrc:/qt/qml/SlmInstaller/ so sibling-resolution and qmldir
 * singleton lookup just work without a system QML path.
 *
 * The `config` context property is provided by SlmSummaryViewStep::widget().
 */
import QtQuick
import "."

SummaryScreen {
    id: screen
    anchors.fill: parent

    diskName:               config.diskName
    diskPath:               config.diskPath
    espSizeMb:              config.espSizeMb
    recoverySizeMb:         config.recoverySizeMb
    subvolumes:             config.subvolumes
    factorySnapshotEnabled: config.factorySnapshotEnabled
    warnings:               config.warnings
    language:               config.language
    timezone:               config.timezone

    onInstallRequested: config.commit()
    onBackRequested: config.back()
}
