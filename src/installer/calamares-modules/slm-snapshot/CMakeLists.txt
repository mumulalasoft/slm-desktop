calamares_add_plugin(slm-snapshot
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmSnapshotJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
