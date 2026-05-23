cmake_minimum_required(VERSION 3.16)

# SLM Installer — Calamares-based system installer for SLM Desktop.
#
# This subtree is gated behind SLM_BUILD_INSTALLER because it depends on
# Calamares development headers (calamares-dev), which are not required to
# build the desktop shell itself. Enable with:
#
#   cmake -B build -DSLM_BUILD_INSTALLER=ON
#
# Design docs:
#   docs/SLM_INSTALLER.md           — binding spec
#   docs/SLM_INSTALLER_BACKEND.md   — backend architecture
#   docs/SLM_INSTALLER_DESIGN.md    — UX/UI plan

option(SLM_BUILD_INSTALLER "Build SLM Installer (Calamares modules)" OFF)

if(NOT SLM_BUILD_INSTALLER)
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

# Calamares 3.3 ships a CMake package config whose imported targets reference
# KF6::CoreAddons in their link interface but do not import it themselves (it
# relies on ECM, which isn't always present). Pre-load KF6CoreAddons so the
# target exists before Calamares's config file gets evaluated.
find_package(KF6CoreAddons QUIET)

find_package(Calamares REQUIRED)

include(CalamaresAddPlugin)

add_library(slm_installer_shared STATIC
    shared/SlmErrorTranslator.cpp
    shared/SlmErrorTranslator.h
)

target_include_directories(slm_installer_shared
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared
)

target_link_libraries(slm_installer_shared
    PUBLIC Qt6::Core
)

set_target_properties(slm_installer_shared PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

add_subdirectory(viewmodels)
add_subdirectory(calamares-modules)

install(FILES settings.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/calamares
)

install(DIRECTORY branding/slm
    DESTINATION ${CMAKE_INSTALL_DATADIR}/calamares/branding
)
