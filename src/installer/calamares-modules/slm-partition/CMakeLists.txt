calamares_add_plugin(slm-partition
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmPartitionJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
