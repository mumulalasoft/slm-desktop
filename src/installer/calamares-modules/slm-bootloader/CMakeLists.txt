calamares_add_plugin(slm-bootloader
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmBootloaderJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
