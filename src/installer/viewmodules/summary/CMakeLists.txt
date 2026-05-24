find_package(Qt6 REQUIRED COMPONENTS Quick QuickWidgets Qml)

calamares_add_plugin(slm-summary
    TYPE viewmodule
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES
        SlmSummaryViewStep.cpp
        SlmSummaryViewStep.h
        Config.cpp
        Config.h
    RESOURCES
        resources.qrc
    LINK_LIBRARIES
        Calamares::calamares
        Calamares::calamaresui
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickWidgets
)
