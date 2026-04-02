cmake_minimum_required(VERSION 3.21)

project(slm-filemanager LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(SLM_FM_MONOREPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/Sources.cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui DBus Sql Concurrent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-unix-2.0)

add_library(slm-filemanager-core STATIC
    ${SLM_FILEMANAGER_CORE_SOURCES}
    ${SLM_FILEMANAGER_CORE_HEADERS}
)

add_library(SlmFileManager::Core ALIAS slm-filemanager-core)

target_include_directories(slm-filemanager-core
    PUBLIC
        $<BUILD_INTERFACE:${SLM_FM_MONOREPO_ROOT}/src/apps/filemanager/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/slm-filemanager>
    PRIVATE
        ${SLM_FM_MONOREPO_ROOT}
        ${SLM_FM_MONOREPO_ROOT}/src/apps/filemanager/include
)

target_link_libraries(slm-filemanager-core
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Sql
        Qt6::Concurrent
        PkgConfig::GIO
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SlmFileManagerConfigVersion.cmake"
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/SlmFileManagerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SlmFileManagerConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SlmFileManager"
)

install(TARGETS slm-filemanager-core
    EXPORT SlmFileManagerTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES
    "${SLM_FM_MONOREPO_ROOT}/src/apps/filemanager/include/filemanagerapi.h"
    "${SLM_FM_MONOREPO_ROOT}/src/apps/filemanager/include/filemanagermodel.h"
    "${SLM_FM_MONOREPO_ROOT}/src/apps/filemanager/include/filemanagermodelfactory.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/slm-filemanager"
)

install(EXPORT SlmFileManagerTargets
    FILE SlmFileManagerTargets.cmake
    NAMESPACE SlmFileManager::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SlmFileManager"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SlmFileManagerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SlmFileManagerConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SlmFileManager"
)

option(SLM_FILEMANAGER_BUILD_TESTS "Build slm-filemanager standalone tests" OFF)
if(SLM_FILEMANAGER_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
