calamares_add_plugin(slm-protected-packages
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmProtectedPackagesJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
