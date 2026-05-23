calamares_add_plugin(slm-firstboot
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmFirstbootJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
