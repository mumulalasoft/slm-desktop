# Each per-module CMakeLists.txt invokes Calamares's own calamares_add_plugin()
# helper (from /usr/lib/.../cmake/Calamares/CalamaresAddPlugin.cmake), which
# handles target naming, module.desc installation, and library output dir.
#
# Order is alphabetical for readability — runtime order is settings.conf.

add_subdirectory(slm-bootloader)
add_subdirectory(slm-btrfs-layout)
add_subdirectory(slm-deploy)
add_subdirectory(slm-firstboot)
add_subdirectory(slm-fstab)
add_subdirectory(slm-hardware-probe)
add_subdirectory(slm-partition)
add_subdirectory(slm-protected-packages)
add_subdirectory(slm-recovery)
add_subdirectory(slm-snapshot)
add_subdirectory(slm-uefi-check)
