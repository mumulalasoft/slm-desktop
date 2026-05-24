calamares_add_plugin(slm-recovery
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmRecoveryJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
