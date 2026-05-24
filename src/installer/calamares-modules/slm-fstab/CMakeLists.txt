calamares_add_plugin(slm-fstab
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmFstabJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
