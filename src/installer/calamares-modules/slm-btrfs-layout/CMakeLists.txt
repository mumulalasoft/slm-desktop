calamares_add_plugin(slm-btrfs-layout
    TYPE job
    EXPORT_MACRO DLLEXPORT_PRO
    SOURCES SlmBtrfsLayoutJob.cpp
    LINK_LIBRARIES
        slm_installer_shared
        Calamares::calamares
        Qt6::Core
)
