find_package(Qt6 REQUIRED COMPONENTS Quick QuickWidgets Qml)

calamares_add_plugin(slm-disk-select
    TYPE viewmodule
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES
        SlmDiskSelectViewStep.cpp
        SlmDiskSelectViewStep.h
        Config.cpp
        Config.h
    RESOURCES
        resources.qrc
    LINK_LIBRARIES
        slm_installer_viewmodels
        Calamares::calamares
        Calamares::calamaresui
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickWidgets
)
