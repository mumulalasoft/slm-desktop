calamares_add_plugin(slm-hardware-probe
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmHardwareProbeJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
