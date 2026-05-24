calamares_add_plugin(slm-deploy
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmDeployJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
