cmake_minimum_required(VERSION 3.16)

project(Slm_Desktop VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Gui Quick QuickControls2 QuickLayouts Network DBus Test Sql Concurrent ShaderTools)
find_package(Qt6 QUIET OPTIONAL_COMPONENTS WaylandClient GuiPrivate)
find_package(PkgConfig REQUIRED)
find_package(Poppler QUIET OPTIONAL_COMPONENTS Qt6)
pkg_check_modules(POPPLER_QT6 QUIET IMPORTED_TARGET poppler-qt6)
if(NOT TARGET Poppler::Qt6 AND POPPLER_QT6_FOUND)
    add_library(Poppler::Qt6 INTERFACE IMPORTED)
    target_link_libraries(Poppler::Qt6 INTERFACE PkgConfig::POPPLER_QT6)
endif()
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-unix-2.0)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_check_modules(WAYLAND_SCANNER REQUIRED wayland-scanner)

pkg_get_variable(WAYLAND_SCANNER_EXECUTABLE wayland-scanner wayland_scanner)
pkg_get_variable(WAYLAND_PROTOCOLS_PKGDATADIR wayland-protocols pkgdatadir)

message(STATUS "Wayland scanner: ${WAYLAND_SCANNER_EXECUTABLE}")
message(STATUS "Wayland protocols dir: ${WAYLAND_PROTOCOLS_PKGDATADIR}")

macro(wayland_generate_protocol XML OUTPUT_BASE)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_BASE}-protocol.c"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} public-code "${XML}" "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_BASE}-protocol.c"
        DEPENDS "${XML}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_BASE}-client-protocol.h"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header "${XML}" "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_BASE}-client-protocol.h"
        DEPENDS "${XML}"
        VERBATIM
    )
endmacro()

wayland_generate_protocol("/usr/share/wayland/wayland.xml" "wayland")
wayland_generate_protocol("${WAYLAND_PROTOCOLS_PKGDATADIR}/staging/ext-data-control/ext-data-control-v1.xml" "ext-data-control-v1")

pkg_check_modules(PIPEWIRE IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(SPA IMPORTED_TARGET libspa-0.2)
pkg_check_modules(PAM QUIET IMPORTED_TARGET pam)
pkg_check_modules(POLKIT_AGENT QUIET IMPORTED_TARGET polkit-agent-1)
pkg_check_modules(POLKIT_GOBJECT QUIET IMPORTED_TARGET polkit-gobject-1)
pkg_check_modules(LIBARCHIVE QUIET IMPORTED_TARGET libarchive)
find_path(SLM_PAM_INCLUDE_DIR security/pam_appl.h)
find_library(SLM_PAM_LIBRARY pam)

set(SLM_HAVE_PIPEWIRE FALSE)
if(PIPEWIRE_FOUND AND SPA_FOUND)
    set(SLM_HAVE_PIPEWIRE TRUE)
endif()
message(STATUS "SLM PipeWire backend support: ${SLM_HAVE_PIPEWIRE}")
if(PAM_FOUND OR (SLM_PAM_INCLUDE_DIR AND SLM_PAM_LIBRARY))
    message(STATUS "SLM desktopd PAM auth support: TRUE")
else()
    message(STATUS "SLM desktopd PAM auth support: FALSE")
endif()
if(POLKIT_AGENT_FOUND AND POLKIT_GOBJECT_FOUND)
    message(STATUS "SLM polkit agent support: TRUE")
else()
    message(STATUS "SLM polkit agent support: FALSE")
endif()
if(TARGET Poppler::Qt6)
    message(STATUS "SLM Poppler Qt6 support: TRUE")
else()
    message(STATUS "SLM Poppler Qt6 support: FALSE (using fallback PDF renderer)")
endif()
if(LIBARCHIVE_FOUND)
    message(STATUS "SLM libarchive support: TRUE")
else()
    message(STATUS "SLM libarchive support: FALSE (archive backend disabled)")
endif()

qt_standard_project_setup(REQUIRES 6.5)
if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

include(cmake/Sources.cmake)

# FileManager integration mode:
# - default (OFF/OFF): build in-tree core target dan link langsung.
# - external package (ON/OFF): find_package() dari system/installed prefix.
# - local dev (ON/ON): find_package() dari local workspace checkout.
#
# Cara pakai dev mode:
#   cmake -DSLM_USE_EXTERNAL_FILEMANAGER_PACKAGE=ON \
#         -DSLM_USE_LOCAL_FILEMANAGER=ON \
#         -DSLM_FILEMANAGER_SOURCE_DIR=/workspace/filemanager
#
# Atau pakai CMake preset: cmake --preset dev-local-fm
option(SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE "Use external SlmFileManager package instead of in-tree core sources" OFF)
option(SLM_BUILD_FILEMANAGER_STANDALONE_TARGET "Build standalone FileManager core library target" ON)

# DEV MODE: SLM_USE_LOCAL_FILEMANAGER hanya berlaku bila SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE=ON.
# Lihat cmake/FindSlmFileManager.cmake untuk detail.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SLM_FILEMANAGER_CORE_TARGET "")
if(SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE)
    include(FindSlmFileManager)
    set(SLM_FILEMANAGER_CORE_TARGET SlmFileManager::Core)
else()
    if(NOT SLM_BUILD_FILEMANAGER_STANDALONE_TARGET)
        message(FATAL_ERROR "SLM_BUILD_FILEMANAGER_STANDALONE_TARGET must be ON when not using external SlmFileManager package.")
    endif()

    add_library(slm-filemanager-core STATIC
        ${FILEMANAGER_APP_SOURCES}
        ${FILEMANAGER_OPS_SERVICE_SOURCES}
        ${FILEMANAGER_PROGRESS_SOURCES}
    )
    add_library(SlmFileManager::Core ALIAS slm-filemanager-core)
    set(SLM_FILEMANAGER_CORE_TARGET SlmFileManager::Core)
    target_include_directories(slm-filemanager-core
        PUBLIC
            ${CMAKE_SOURCE_DIR}/src/apps/filemanager/include
        PRIVATE
            ${CMAKE_SOURCE_DIR}
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
endif()

# slm-desktop consumes FileManager via library target to ease standalone split.
if(SLM_USE_EXTERNAL_FILEMANAGER_PACKAGE)
    # In external package mode, FileManager units are provided by SlmFileManager::Core.
    # Keep execution gate/runtime registry in slm-desktop (shell-owned dependencies).
    list(REMOVE_ITEM DESKTOP_SHELL_SERVICES
        ${FILEMANAGER_APP_SOURCES}
        ${FILEMANAGER_PROGRESS_SOURCES}
        metadataindexserver.cpp
        metadataindexserver.h
        src/core/actions/slmactiontypes.cpp
        src/core/actions/slmactiontypes.h
        src/core/actions/slmactioncondition.cpp
        src/core/actions/slmactioncondition.h
        src/core/actions/slmdesktopactionparser.cpp
        src/core/actions/slmdesktopactionparser.h
        src/core/actions/slmactiondiscovery.cpp
        src/core/actions/slmactiondiscovery.h
        src/core/actions/slmactionregistry.cpp
        src/core/actions/slmactionregistry.h
        src/core/actions/framework/slmdesktopentryscanner.cpp
        src/core/actions/framework/slmdesktopentryscanner.h
        src/core/actions/framework/slmframeworktypes.h
        src/core/actions/framework/acl/slmacllexer.cpp
        src/core/actions/framework/acl/slmacllexer.h
        src/core/actions/framework/acl/slmaclparser.cpp
        src/core/actions/framework/acl/slmaclparser.h
        src/core/actions/framework/slmdesktopentryparsermodule.cpp
        src/core/actions/framework/slmdesktopentryparsermodule.h
        src/core/actions/framework/slmmetadataparser.cpp
        src/core/actions/framework/slmmetadataparser.h
        src/core/actions/framework/slmactionregistryindex.cpp
        src/core/actions/framework/slmactionregistryindex.h
        src/core/actions/framework/slmactioncachelayer.cpp
        src/core/actions/framework/slmactioncachelayer.h
        src/core/actions/framework/slmcachemanager.cpp
        src/core/actions/framework/slmcachemanager.h
        src/core/actions/framework/slmcapabilityresolverlayer.cpp
        src/core/actions/framework/slmcapabilityresolverlayer.h
        src/core/actions/framework/slmcapabilitybuilders.cpp
        src/core/actions/framework/slmcapabilitybuilders.h
        src/core/actions/framework/slmgrouptreebuilder.cpp
        src/core/actions/framework/slmgrouptreebuilder.h
        src/core/actions/framework/slmactioninvoker.cpp
        src/core/actions/framework/slmactioninvoker.h
        src/core/actions/framework/slmactionframework.cpp
        src/core/actions/framework/slmactionframework.h
        src/core/permissions/Capability.cpp
        src/core/permissions/Capability.h
        src/core/permissions/CallerIdentity.cpp
        src/core/permissions/CallerIdentity.h
        src/core/permissions/AccessContext.cpp
        src/core/permissions/AccessContext.h
        src/core/permissions/PolicyDecision.cpp
        src/core/permissions/PolicyDecision.h
        src/core/permissions/PermissionStore.cpp
        src/core/permissions/PermissionStore.h
        src/core/permissions/AuditLogger.cpp
        src/core/permissions/AuditLogger.h
        src/core/permissions/TrustResolver.cpp
        src/core/permissions/TrustResolver.h
        src/core/permissions/SensitiveContentPolicy.cpp
        src/core/permissions/SensitiveContentPolicy.h
        src/core/permissions/PolicyEngine.cpp
        src/core/permissions/PolicyEngine.h
        src/core/permissions/PermissionBroker.cpp
        src/core/permissions/PermissionBroker.h
        src/core/permissions/DBusSecurityGuard.cpp
        src/core/permissions/DBusSecurityGuard.h
    )
else()
    list(REMOVE_ITEM DESKTOP_SHELL_SERVICES
        ${FILEMANAGER_APP_SOURCES}
        ${FILEMANAGER_PROGRESS_SOURCES}
    )
endif()

qt_add_executable(slm-desktop
    ${DESKTOP_SHELL_APP_ENTRY}
    ${DESKTOP_SHELL_MODELS_AND_RUNTIME}
    ${DESKTOP_SHELL_SERVICES}
    src/apps/settings/modules/developer/enventry.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
)
target_include_directories(slm-desktop PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/filemanager/include
    ${CMAKE_SOURCE_DIR}/src/printing
    ${CMAKE_BINARY_DIR}
)

# Ensure the runtime linker can resolve the same Qt used at configure time.
# Some local build trees may end up with an empty RUNPATH placeholder list.
if(DEFINED Qt6_DIR AND TARGET slm-desktop)
    get_filename_component(_qt6_cmake_dir "${Qt6_DIR}" ABSOLUTE)
    get_filename_component(_qt6_lib_dir "${_qt6_cmake_dir}/../.." ABSOLUTE)
    if(EXISTS "${_qt6_lib_dir}/libQt6Core.so" OR EXISTS "${_qt6_lib_dir}/libQt6Core.so.6")
        set_target_properties(slm-desktop PROPERTIES
            BUILD_RPATH "${_qt6_lib_dir}"
            INSTALL_RPATH "${_qt6_lib_dir}"
        )
    endif()
endif()

qt_add_executable(indicatorctl
    src/cli/indicatorctl.cpp
)

qt_add_executable(windowingctl
    ${WINDOWINGCTL_SOURCES}
)

qt_add_executable(workspacectl
    src/cli/workspacectl.cpp
)

qt_add_executable(fileopctl
    src/cli/fileopctl.cpp
)

qt_add_executable(devicectl
    src/cli/devicectl.cpp
)

qt_add_executable(globalmenuctl
    src/cli/globalmenuctl.cpp
)

# SlmSettings Application
qt_add_executable(slm-settings
    src/apps/settings/main.cpp
    src/apps/settings/settingsapp.cpp
    src/core/icons/themeiconprovider.cpp
    src/core/icons/themeiconprovider.h
    src/core/icons/themeiconcontroller.cpp
    src/core/icons/themeiconcontroller.h
    src/apps/settings/moduleloader.cpp
    src/apps/settings/searchengine.cpp
    src/apps/settings/globalsearchprovider.cpp
    src/apps/settings/include/settingbinding.cpp
    src/apps/settings/include/mockbinding.cpp
    src/apps/settings/include/gsettingsbinding.cpp
    src/apps/settings/include/networkmanagerbinding.cpp
    src/apps/settings/include/dbusbinding.cpp
    src/apps/settings/include/localsettingsbinding.cpp
    src/apps/settings/include/settingbindingresolver.cpp
    src/apps/settings/include/settingbindingfactory.cpp
    src/apps/settings/include/settingspolkitbridge.cpp
    src/apps/settings/wallpapermanager.cpp
    src/apps/settings/wallpapermanager.h
    src/apps/settings/mimeappsmanager.cpp
    src/apps/settings/mimeappsmanager.h
    src/apps/settings/modules/applications/startupappscontroller.cpp
    src/apps/settings/modules/applications/startupappscontroller.h
    src/apps/settings/thememanager.cpp
    src/apps/settings/thememanager.h
    src/apps/settings/desktopsettingsclient.cpp
    src/apps/settings/desktopsettingsclient.h
    src/apps/settings/fontmanager.cpp
    src/apps/settings/fontmanager.h
    src/apps/settings/modules/useraccounts/useraccountscontroller.cpp
    src/apps/settings/modules/useraccounts/useraccountscontroller.h
    src/printing/core/PrinterCapabilityProvider.cpp
    src/printing/core/PrinterCapabilityProvider.h
    src/printing/core/PrinterManager.cpp
    src/printing/core/PrinterManager.h
    src/printing/core/PrinterAdminService.cpp
    src/printing/core/PrinterAdminService.h
    src/printing/core/PrintPreviewModel.cpp
    src/printing/core/PrintPreviewModel.h
    src/printing/core/PrintTypes.h
    src/printing/preview/FallbackPdfRenderer.cpp
    src/printing/preview/FallbackPdfRenderer.h
    src/printing/preview/PrintPreviewCache.cpp
    src/printing/preview/PrintPreviewCache.h
    src/apps/settings/modules/developer/envvalidator.cpp
    src/apps/settings/modules/developer/envvalidator.h
    src/apps/settings/modules/developer/localenvstore.cpp
    src/apps/settings/modules/developer/localenvstore.h
    src/apps/settings/modules/developer/envvariablemodel.cpp
    src/apps/settings/modules/developer/envvariablemodel.h
    src/apps/settings/modules/developer/envvariablecontroller.cpp
    src/apps/settings/modules/developer/envvariablecontroller.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
    src/apps/settings/modules/developer/effectiveenvpreviewcontroller.cpp
    src/apps/settings/modules/developer/effectiveenvpreviewcontroller.h
    src/services/envd/envtypes.h
    src/services/envd/mergeresolver.cpp
    src/services/envd/mergeresolver.h
    src/services/envd/sessionenvstore.cpp
    src/services/envd/sessionenvstore.h
    src/services/envd/envstore.cpp
    src/services/envd/envstore.h
    src/services/envd/perappenvstore.cpp
    src/services/envd/perappenvstore.h
    src/services/envd/envservice.cpp
    src/services/envd/envservice.h
    src/apps/settings/modules/developer/perappoverridescontroller.cpp
    src/apps/settings/modules/developer/perappoverridescontroller.h
    src/apps/settings/modules/developer/developermode.cpp
    src/apps/settings/modules/developer/developermode.h
    src/apps/settings/modules/developer/buildinfocontroller.cpp
    src/apps/settings/modules/developer/buildinfocontroller.h
    src/apps/settings/modules/developer/logserviceclient.cpp
    src/apps/settings/modules/developer/logserviceclient.h
    src/apps/settings/modules/developer/logscontroller.cpp
    src/apps/settings/modules/developer/logscontroller.h
    src/apps/settings/modules/developer/svcmanagerclient.cpp
    src/apps/settings/modules/developer/svcmanagerclient.h
    src/apps/settings/modules/developer/processservicescontroller.cpp
    src/apps/settings/modules/developer/processservicescontroller.h
    src/apps/settings/modules/developer/featureflagscontroller.cpp
    src/apps/settings/modules/developer/featureflagscontroller.h
    src/apps/settings/modules/developer/dbusinspectorcontroller.cpp
    src/apps/settings/modules/developer/dbusinspectorcontroller.h
    src/apps/settings/modules/developer/appsandboxcontroller.cpp
    src/apps/settings/modules/developer/appsandboxcontroller.h
    src/apps/settings/modules/developer/systemenvcontroller.cpp
    src/apps/settings/modules/developer/systemenvcontroller.h
    src/apps/settings/modules/developer/portalbridgeservice.cpp
    src/apps/settings/modules/developer/portalbridgeservice.h
    src/apps/settings/modules/developer/xdgportalscontroller.cpp
    src/apps/settings/modules/developer/xdgportalscontroller.h
    src/apps/settings/modules/developer/pathmanagercontroller.cpp
    src/apps/settings/modules/developer/pathmanagercontroller.h
    src/services/context/contextclient.cpp
    src/services/context/contextclient.h
    src/apps/settings/modules/developer/componenthealthcontroller.cpp
    src/apps/settings/modules/developer/componenthealthcontroller.h
    src/apps/settings/modules/developer/daemonhealthclient.cpp
    src/apps/settings/modules/developer/daemonhealthclient.h
    src/core/system/componentregistry.cpp
    src/core/system/componentregistry.h
    src/core/system/dependencyguard.cpp
    src/core/system/dependencyguard.h
    src/core/system/missingcomponentcontroller.cpp
    src/core/system/missingcomponentcontroller.h
    src/services/envd/systemenvstore.cpp
    src/services/envd/systemenvstore.h
    src/core/launcher/launchenvresolver.cpp
    src/core/launcher/launchenvresolver.h
)

qt_add_executable(slm-loggerd
    src/daemon/loggerd/loggerd_main.cpp
    src/services/logger/logtypes.h
    src/services/logger/logservice.cpp
    src/services/logger/logservice.h
    src/services/logger/logdbusinterface.cpp
    src/services/logger/logdbusinterface.h
)

qt_add_executable(slm-svcmgrd
    src/daemon/svcmgrd/svcmgrd_main.cpp
    src/services/svcmgr/componentinfo.h
    src/services/svcmgr/svcmanagerservice.cpp
    src/services/svcmgr/svcmanagerservice.h
    src/services/svcmgr/svcmanagerdbusinterface.cpp
    src/services/svcmgr/svcmanagerdbusinterface.h
)

qt_add_executable(slm-package-policy-service
    src/daemon/packagepolicyd/packagepolicyd_main.cpp
    src/services/packagepolicy/packagetransaction.cpp
    src/services/packagepolicy/packagetransaction.h
    src/services/packagepolicy/packagepolicyconfig.cpp
    src/services/packagepolicy/packagepolicyconfig.h
    src/services/packagepolicy/aptsimulator.cpp
    src/services/packagepolicy/aptsimulator.h
    src/services/packagepolicy/packagepolicyengine.cpp
    src/services/packagepolicy/packagepolicyengine.h
    src/services/packagepolicy/packagesourceresolver.cpp
    src/services/packagepolicy/packagesourceresolver.h
    src/services/packagepolicy/packagepolicylogger.cpp
    src/services/packagepolicy/packagepolicylogger.h
    src/services/packagepolicy/packagepolicyservice.cpp
    src/services/packagepolicy/packagepolicyservice.h
)

qt_add_executable(slmcapdemo
    src/cli/slmcapdemo.cpp
    src/core/actions/slmactiontypes.cpp
    src/core/actions/slmactiontypes.h
    src/core/actions/slmactioncondition.cpp
    src/core/actions/slmactioncondition.h
    src/core/actions/slmdesktopactionparser.cpp
    src/core/actions/slmdesktopactionparser.h
    src/core/actions/slmactiondiscovery.cpp
    src/core/actions/slmactiondiscovery.h
    src/core/actions/slmactionregistry.cpp
    src/core/actions/slmactionregistry.h
    src/core/execution/appexecutiongate.cpp
    src/core/execution/appexecutiongate.h
    src/core/execution/appruntimeregistry.cpp
    src/core/execution/appruntimeregistry.h
    src/core/launcher/applauncher.cpp
    src/core/launcher/applauncher.h
    src/core/launcher/launchenvresolver.cpp
    src/core/launcher/launchenvresolver.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
    dockidentity.cpp
    dockidentity.h
    dockmodel.cpp
    dockmodel.h
    shortcutmodel.cpp
    shortcutmodel.h
    src/core/actions/framework/slmdesktopentryscanner.cpp
    src/core/actions/framework/slmdesktopentryscanner.h
    src/core/actions/framework/slmactionframework.cpp
    src/core/actions/framework/slmactionframework.h
    src/core/actions/framework/slmframeworktypes.h
    src/core/actions/framework/slmdesktopentryparsermodule.cpp
    src/core/actions/framework/slmdesktopentryparsermodule.h
    src/core/actions/framework/slmmetadataparser.cpp
    src/core/actions/framework/slmmetadataparser.h
    src/core/actions/framework/slmactionregistryindex.cpp
    src/core/actions/framework/slmactionregistryindex.h
    src/core/actions/framework/slmactioncachelayer.cpp
    src/core/actions/framework/slmactioncachelayer.h
    src/core/actions/framework/slmcapabilityresolverlayer.cpp
    src/core/actions/framework/slmcapabilityresolverlayer.h
    src/core/actions/framework/slmcapabilitybuilders.cpp
    src/core/actions/framework/slmcapabilitybuilders.h
    src/core/actions/framework/slmactioninvoker.cpp
    src/core/actions/framework/slmactioninvoker.h
)

qt_add_executable(desktopd
    ${DESKTOPD_SOURCES}
)

qt_add_executable(slm-fileopsd
    ${SLM_FILEOPSD_SOURCES}
)

qt_add_executable(slm-devicesd
    ${SLM_DEVICESD_SOURCES}
)

qt_add_executable(slm-portald
    ${SLM_PORTALD_SOURCES}
)

qt_add_executable(slm-clipboardd
    ${SLM_CLIPBOARDD_SOURCES}
)

qt_add_executable(slm-envd
    src/daemon/envd/envd_main.cpp
    src/services/envd/envtypes.h
    src/services/envd/mergeresolver.cpp
    src/services/envd/mergeresolver.h
    src/services/envd/sessionenvstore.cpp
    src/services/envd/sessionenvstore.h
    src/services/envd/envstore.cpp
    src/services/envd/envstore.h
    src/services/envd/perappenvstore.cpp
    src/services/envd/perappenvstore.h
    src/services/envd/systemenvstore.cpp
    src/services/envd/systemenvstore.h
    src/services/envd/envservice.cpp
    src/services/envd/envservice.h
    src/services/envd/dbusinterface.cpp
    src/services/envd/dbusinterface.h
    src/apps/settings/modules/developer/enventry.h
    src/apps/settings/modules/developer/envvalidator.cpp
    src/apps/settings/modules/developer/envvalidator.h
    src/apps/settings/modules/developer/localenvstore.cpp
    src/apps/settings/modules/developer/localenvstore.h
)

qt_add_executable(slm-envd-helper
    src/daemon/envd-helper/envd_helper_main.cpp
    src/services/envd-helper/helperservice.cpp
    src/services/envd-helper/helperservice.h
    src/services/envd-helper/helperdbusinterface.cpp
    src/services/envd-helper/helperdbusinterface.h
    src/services/envd-helper/auditlog.cpp
    src/services/envd-helper/auditlog.h
    src/apps/settings/modules/developer/enventry.h
    src/apps/settings/modules/developer/localenvstore.cpp
    src/apps/settings/modules/developer/localenvstore.h
)

qt_add_executable(slm-settingsd
    ${SLM_SETTINGSD_SOURCES}
)

qt_add_executable(desktop-contextd
    ${SLM_CONTEXTD_SOURCES}
)

qt_add_executable(slm-firewalld
    ${SLM_FIREWALLD_SOURCES}
)

qt_add_executable(slm-secretd
    src/daemon/secretd/secretd_main.cpp
    src/services/secret/secretservice.cpp
    src/services/secret/secretservice.h
)

if(LIBARCHIVE_FOUND)
qt_add_executable(slm-archived
    src/daemon/archived/archived_main.cpp
    src/services/archive/archiveservice.cpp
    src/services/archive/archiveservice.h
    src/services/archive/archivebackend.cpp
    src/services/archive/archivebackend.h
)
endif()

target_link_libraries(windowingctl
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::DBus
        Qt6::Concurrent
        PkgConfig::GIO
)

target_link_libraries(workspacectl
    PRIVATE
        Qt6::Core
        Qt6::DBus
)

target_link_libraries(fileopctl
    PRIVATE
        Qt6::Core
        Qt6::DBus
)

target_link_libraries(devicectl
    PRIVATE
        Qt6::Core
        Qt6::DBus
)

target_link_libraries(globalmenuctl
    PRIVATE
        Qt6::Core
        Qt6::DBus
)

target_link_libraries(slmcapdemo
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Concurrent
        PkgConfig::GIO
)

target_link_libraries(desktopd
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Network
        Qt6::Sql
        Qt6::Concurrent
        PkgConfig::GIO
)

if(PAM_FOUND OR (SLM_PAM_INCLUDE_DIR AND SLM_PAM_LIBRARY))
    target_compile_definitions(desktopd PRIVATE SLM_HAVE_PAM=1)
    if(PAM_FOUND)
        target_link_libraries(desktopd PRIVATE PkgConfig::PAM)
    else()
        target_include_directories(desktopd PRIVATE ${SLM_PAM_INCLUDE_DIR})
        target_link_libraries(desktopd PRIVATE ${SLM_PAM_LIBRARY})
    endif()
endif()

target_include_directories(desktopd PRIVATE ${CMAKE_SOURCE_DIR})
if(SLM_HAVE_PIPEWIRE)
    target_compile_definitions(desktopd PRIVATE SLM_HAVE_PIPEWIRE=1)
    target_link_libraries(desktopd PRIVATE PkgConfig::PIPEWIRE PkgConfig::SPA)
endif()

target_link_libraries(slm-fileopsd
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Concurrent
        Qt6::Sql
)
target_include_directories(slm-fileopsd PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(slm-devicesd
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        PkgConfig::GIO
)
target_include_directories(slm-devicesd PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(slm-portald
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        PkgConfig::GIO
)

target_include_directories(slm-portald PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(slm-clipboardd
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Sql
        PkgConfig::WAYLAND_CLIENT
)
target_include_directories(slm-clipboardd PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

target_link_libraries(slm-envd
    PRIVATE
        Qt6::Core
        Qt6::DBus
)
target_include_directories(slm-envd PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(slm-envd-helper
    PRIVATE
        Qt6::Core
        Qt6::DBus
)
target_include_directories(slm-envd-helper PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(slm-settingsd
    PRIVATE
        Qt6::Core
        Qt6::DBus
)
target_include_directories(slm-settingsd PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(slm-firewalld
    PRIVATE
        Qt6::Core
        Qt6::DBus
)
target_include_directories(slm-firewalld PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(slm-secretd
    PRIVATE
        Qt6::Core
        Qt6::DBus
)
target_include_directories(slm-secretd PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(slm-loggerd
    PRIVATE
        Qt6::Core
        Qt6::DBus
)
target_include_directories(slm-loggerd PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(slm-svcmgrd
    PRIVATE
        Qt6::Core
        Qt6::DBus
)
target_include_directories(slm-svcmgrd PRIVATE ${CMAKE_SOURCE_DIR})

if(TARGET slm-archived)
target_link_libraries(slm-archived
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Concurrent
        PkgConfig::LIBARCHIVE
)
target_include_directories(slm-archived PRIVATE ${CMAKE_SOURCE_DIR})
endif()

target_link_libraries(slm-package-policy-service
    PRIVATE
        Qt6::Core
        Qt6::DBus
)
target_include_directories(slm-package-policy-service PRIVATE ${CMAKE_SOURCE_DIR})

add_custom_target(lint_ui_style
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/lint-ui-style.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running SLM Desktop UI style lint"
)

add_custom_target(lint_slm_style_usage
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-slm-style-usage.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking SLM style component usage in QML"
)

add_custom_target(lint_animation_tokens
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-animation-token-usage.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking animation token usage in QML"
)

add_custom_target(lint_capability_matrix
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-capability-matrix.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking SLM capability matrix sync"
)

add_custom_target(lint_topbar_popup_contract
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-topbar-popup-contract.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking topbar popup contract"
)

set_source_files_properties(Qml/IndicatorRegistry.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(
    third_party/slm-style/qml/SlmStyle/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)
set_source_files_properties(Qml/ShellState.qml  PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(Qml/ShellZOrder.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(Qml/DockSystem.qml  PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(Qml/DockController.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(Qml/BadgeService.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(Qml/TopBarPopupController.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(DESKTOP_SHELL_QML_FILES
    Main.qml
    third_party/slm-style/qml/SlmStyle/Theme.qml
    Qml/IndicatorRegistry.qml
    Qml/ShellState.qml
    Qml/ShellZOrder.qml
    Qml/DockSystem.qml
    Qml/DockController.qml
    Qml/BadgeService.qml
    Qml/TopBarPopupController.qml
    Qml/DesktopScene.qml
    Qml/apps/filemanager/FileManagerShareSheet.qml
    Qml/apps/filemanager/RecursiveMenu.qml
    Qml/components/applet/DatetimeApplet.qml
    Qml/components/AppWindow.qml
    Qml/components/AppDialog.qml
    Qml/components/desktop/DesktopBackground.qml
    Qml/components/dock/Dock.qml
    Qml/components/dock/DockItem.qml
    Qml/components/launchpad/Launchpad.qml
    # Qml/components/NetworkIndicator.qml
    # Qml/components/BluetoothIndicator.qml
    # Qml/components/SoundIndicator.qml
    Qml/components/applet/BatchOperationIndicator.qml
    Qml/components/system/MissingComponentsCard.qml
    Qml/components/overlay/GlobalProgressOverlay.qml
    Qml/components/overlay/GlobalProgressWindow.qml
    Qml/components/overlay/ShellServiceBridge.qml
    Qml/components/overlay/ShellEventBridge.qml
    Qml/components/overlay/ShellSystemWindows.qml
    Qml/components/overlay/TopBarWindow.qml
    Qml/components/overlay/PopupHostLayer.qml
    Qml/components/overlay/TopBarPopupStateController.qml
    Qml/components/overlay/DockWindow.qml
    Qml/components/overlay/LaunchpadWindow.qml
    Qml/components/overlay/DetachedFileManagerWindow.qml
    Qml/components/overlay/PortalFileChooserWindow.qml
    Qml/components/overlay/AreaShotSelectorWindow.qml
    Qml/components/overlay/MotionDebugOverlay.qml
    Qml/components/overlay/ClipboardOverlayWindow.qml
    Qml/components/overlay/WorkspaceWindow.qml
    Qml/components/overlay/LockScreenWindow.qml
    Qml/components/overlay/PortalConsentDialogWindow.qml
    Qml/components/overlay/LaunchpadActions.js
    # Qml/components/NotificationIndicator.qml
    Qml/components/applet/ScreencastIndicator.qml
    Qml/components/applet/InputCaptureIndicator.qml
    Qml/components/applet/BatteryApplet.qml
    Qml/components/applet/ClipboardApplet.qml
    Qml/components/applet/ControlCenterApplet.qml
    Qml/components/applet/NetworkApplet.qml
    Qml/components/applet/BluetoothApplet.qml
    Qml/components/applet/NotificationApplet.qml
    Qml/components/applet/SoundApplet.qml
    Qml/components/applet/PrintJobApplet.qml
    Qml/components/overlay/ClipboardOverlay.qml
    Qml/components/notification/NotificationManager.qml
    Qml/components/notification/BannerContainer.qml
    Qml/components/notification/NotificationCard.qml
    Qml/components/notification/NotificationCenter.qml
    Qml/components/notification/NotificationGroup.qml
    Qml/components/notification/NotificationManagerWindow.qml
    Qml/components/notification/NotificationBubble.qml
    # Qml/components/BatteryIndicator.qml
    Qml/components/topbar/TopBar.qml
    Qml/components/indicators/IndicatorManager.qml
    Qml/components/indicators/AppIndicatorItem.qml
    Qml/components/indicators/IndicatorMenu.qml

    Qml/components/style/WindowControlsCapsule.qml
    Qml/components/shell/Shell.qml
    Qml/components/screenshot/ScreenshotSaveDialog.qml
    Qml/components/screenshot/ScreenshotOverwriteDialog.qml
    Qml/components/print/PrintDialog.qml
    Qml/components/print/PrintPreviewPane.qml
    Qml/components/print/PrintAdvancedPanel.qml
    Qml/components/print/PrinterFeatureSection.qml
    Qml/components/portalchooser/PortalChooserActionBar.qml
    Qml/components/portalchooser/PortalChooserFooterPane.qml
    Qml/components/portalchooser/PortalChooserPreviewPanel.qml
    Qml/components/portalchooser/PortalChooserSaveNameRow.qml
    Qml/components/portalchooser/PortalChooserOverwriteDialog.qml
    Qml/components/portalchooser/PortalChooserEmptyState.qml
    Qml/components/portalchooser/PortalChooserContentPane.qml
    Qml/components/portalchooser/PortalChooserHeaderControls.qml
    Qml/components/portalchooser/PortalChooserListHeader.qml
    Qml/components/portalchooser/PortalChooserListControls.qml
    Qml/components/portalchooser/PortalChooserListBackgroundArea.qml
    Qml/components/portalchooser/PortalChooserListPane.qml
    Qml/components/portalchooser/PortalChooserListRow.qml
    Qml/components/portalchooser/PortalChooserPlacesPane.qml
    Qml/components/portalchooser/PortalChooserPathBar.qml
    Qml/components/portalchooser/PortalChooserShortcuts.qml
    Qml/components/notification/NotificationBubbleWindow.qml
    Qml/components/shell/ShellContextWindow.qml
    Qml/components/globalmenu/GlobalMenuActionRouter.qml
    Qml/components/topbar/TopBarBrandSection.qml
    Qml/components/topbar/TopBarAnchoredMenu.qml
    Qml/components/topbar/TopBarAnchoredPopup.qml
    Qml/components/topbar/TopBarMainMenuControl.qml
    Qml/components/topbar/TopBarSearchButton.qml
    Qml/components/topbar/TopBarScreenshotControl.qml
    Qml/components/compositor/CompositorSwitcherOverlay.qml
    Qml/components/workspace/WorkspaceOverlay.qml
    Qml/components/workspace/WorkspaceStrip.qml
    Qml/components/workspace/WorkspaceOverviewScene.qml
    Qml/components/workspace/WindowThumbnail.qml
    Qml/apps/filemanager/FileManagerWindow.qml
    Qml/components/tothespot/TothespotOverlay.qml
    Qml/components/tothespot/TothespotDismissWindow.qml
    Qml/components/tothespot/TothespotWindow.qml
    Qml/components/dock/DockAppDelegate.qml
    Qml/components/dock/DockReorderController.qml
    Qml/components/dock/DockReorderMarker.qml
    Qml/components/dock/DockDropPulse.qml
    Qml/components/shell/ShellShortcutTile.qml
    Qml/components/shell/ShellSelectionRect.qml
    Qml/components/shell/ShellDragGhost.qml
    Qml/components/shell/GlobalShortcutManager.qml
    Qml/apps/filemanager/FileManagerSidebar.qml
    Qml/apps/filemanager/FileManagerSidebarPane.qml
    Qml/apps/filemanager/FileManagerTabBar.qml
    Qml/apps/filemanager/FileManagerHeaderBar.qml
    Qml/apps/filemanager/FileManagerFooterBar.qml
    Qml/apps/filemanager/FileManagerWindowBody.qml
    Qml/apps/filemanager/FileManagerMainPane.qml
    Qml/apps/filemanager/FileManagerToolbar.qml
    Qml/apps/filemanager/FileManagerStatusBar.qml
    Qml/apps/filemanager/FileManagerMenus.qml
    Qml/apps/filemanager/FileManagerApiConnections.qml
    Qml/apps/filemanager/FileManagerConnectServerDialog.qml
    Qml/apps/filemanager/FileManagerBatchOverlayPopup.qml
    Qml/apps/filemanager/FileManagerSidebarContextMenu.qml
    Qml/apps/filemanager/FileManagerClearRecentsDialog.qml
    Qml/apps/filemanager/FileManagerQuickPreviewDialog.qml
    Qml/apps/filemanager/FileManagerRenameDialog.qml
    Qml/apps/filemanager/FileManagerPropertiesDialog.qml
    Qml/apps/filemanager/FileManagerPropertiesDialogContent.qml
    Qml/apps/filemanager/FileManagerCompressDialog.qml
    Qml/apps/filemanager/FileManagerOpenWithDialog.qml
    Qml/apps/filemanager/FileManagerStorageVolumeSelectorDialog.qml
    Qml/apps/filemanager/FileManagerFolderShareDialog.qml
    Qml/apps/filemanager/FileManagerRenameSheet.qml
    Qml/apps/filemanager/FileManagerDragGhost.qml
    Qml/apps/filemanager/FileManagerKeys.js
    Qml/apps/filemanager/FileManagerDnd.js
    Qml/apps/filemanager/FileManagerDialogs.qml
    Qml/apps/filemanager/FileManagerOps.js
    Qml/apps/filemanager/FileManagerSidebarOps.js
    Qml/apps/filemanager/FileManagerSidebarContextOps.js
    Qml/apps/filemanager/FileManagerSelection.js
    Qml/apps/filemanager/FileManagerTabs.js
    Qml/apps/filemanager/FileManagerNavigation.js
    Qml/apps/filemanager/FileManagerOpenWith.js
    Qml/apps/filemanager/FileManagerBatch.js
    Qml/apps/filemanager/FileManagerContentDnd.js
    Qml/apps/filemanager/FileManagerContentHandlers.js
    Qml/apps/filemanager/FileManagerDisplay.js
    Qml/apps/filemanager/FileManagerGlobalMenuController.js
    Qml/apps/filemanager/FileManagerModelConnections.qml
    Qml/apps/filemanager/FileManagerLifecycle.qml
    Qml/apps/filemanager/FileManagerPathOps.js
    Qml/apps/filemanager/FileManagerPathUtils.js
    Qml/apps/filemanager/FileManagerRuntimeConnections.qml
    Qml/apps/filemanager/FileManagerWindowActions.js
    Qml/apps/filemanager/FileManagerUtils.js
    Qml/apps/filemanager/FileManagerLibrary.js
    Qml/components/portalchooser/PortalChooserNav.js
    Qml/components/portalchooser/PortalChooserController.js
    Qml/components/portalchooser/PortalChooserFlow.js
    Qml/components/portalchooser/PortalChooserApi.qml
    Qml/components/screenshot/ScreenshotFlow.js
    Qml/components/screenshot/ScreenshotController.js
    Qml/components/screenshot/ScreenshotSaveController.js
    Qml/components/shell/ShellUtils.js
    Qml/components/shell/TothespotController.js
    Qml/components/dock/DockMath.js
    Qml/apps/filemanager/FileManagerContentView.qml
    Qml/apps/filemanager/FileManagerRecentItem.qml
    Qml/apps/filemanager/FileManagerFavoriteItem.qml
    Qml/apps/filemanager/FileManagerOpenWithSheet.qml
    Qml/apps/filemanager/FileManagerRecentFilesModel.qml
    Qml/apps/filemanager/FileManagerSearchBridge.qml
    Qml/apps/filemanager/FileManagerSidebarIntegration.qml
    Qml/apps/filemanager/FileOperationsProgressOverlay.qml
    Qml/apps/filemanager/TrashBadgeController.qml
)

qt_add_qml_module(slm-desktop
    URI Slm_Desktop
    VERSION 1.0
    QML_FILES
        ${DESKTOP_SHELL_QML_FILES}
)

qt_add_qml_module(slm-settings
    URI SlmSettings
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Qml/apps/settings/Main.qml
        Qml/apps/settings/StylePreviewGallery.qml
        Qml/components/style/WindowControlsCapsule.qml
        Qml/apps/settings/Sidebar.qml
        Qml/apps/settings/ContentPanel.qml
        Qml/apps/settings/components/SettingCard.qml
        Qml/apps/settings/components/SettingToggle.qml
        Qml/apps/settings/components/SettingGroup.qml
        Qml/apps/settings/components/AuthorizationHint.qml
        Qml/apps/settings/components/MissingComponentsCard.qml
)

set(DESKTOP_SHELL_RESOURCE_FILES
    assets/icons/battery_0.svg
    assets/icons/battery_25.svg
    assets/icons/battery_50.svg
    assets/icons/battery_75.svg
    assets/icons/battery_75_white.svg
    assets/icons/battery_100.svg
    assets/icons/battery_missing.svg
    assets/icons/ethernet.svg
    assets/icons/ethernet_white.svg
    assets/icons/logo.svg
    assets/icons/logo_white.svg
    assets/icons/launchpad.svg
    assets/icons/wifi_1_bar.svg
    assets/icons/wifi_1_bar_white.svg
    assets/icons/wifi_2_bar.svg
    assets/icons/wifi_2_bar_white.svg
    assets/icons/wifi_3_bar.svg
    assets/icons/wifi_3_bar_white.svg
    assets/images/wallpaper.jpeg
    assets/icons/titlebuttons/titlebutton-menu-backdrop.svg
    assets/icons/titlebuttons/titlebutton-unmaximize-active.svg
    assets/icons/titlebuttons/titlebutton-maximize.svg
    assets/icons/titlebuttons/titlebutton-minimize.svg
    assets/icons/titlebuttons/titlebutton-close-hover.svg
    assets/icons/titlebuttons/titlebutton-unshade-backdrop.svg
    assets/icons/titlebuttons/titlebutton-minimize-active.svg
    assets/icons/titlebuttons/titlebutton-backdrop.svg
    assets/icons/titlebuttons/titlebutton-minimize-backdrop-active.svg
    assets/icons/titlebuttons/titlebutton-maximize-active.svg
    assets/icons/titlebuttons/titlebutton-close.svg
    assets/icons/titlebuttons/titlebutton-unshade.svg
    assets/icons/titlebuttons/titlebutton-shade-hover.svg
    assets/icons/titlebuttons/titlebutton-shade-backdrop.svg
    assets/icons/titlebuttons/titlebutton-minimize-hover.svg
    assets/icons/titlebuttons/titlebutton-maximize-backdrop-active.svg
    assets/icons/titlebuttons/titlebutton-close-backdrop-active.svg
    assets/icons/titlebuttons/titlebutton-menu-backdrop-active.svg
    assets/icons/titlebuttons/titlebutton-unmaximize-backdrop-active.svg
    assets/icons/titlebuttons/titlebutton-unmaximize-backdrop.svg
    assets/icons/titlebuttons/titlebutton-unmaximize.svg
    assets/icons/titlebuttons/titlebutton-minimize-backdrop.svg
    assets/icons/titlebuttons/titlebutton-unmaximize-hover.svg
    assets/icons/titlebuttons/titlebutton-shade.svg
    assets/icons/titlebuttons/titlebutton-shade-active.svg
    assets/icons/titlebuttons/titlebutton-maximize-hover.svg
    assets/icons/titlebuttons/titlebutton-close-backdrop.svg
    assets/icons/titlebuttons/titlebutton-menu-active.svg
    assets/icons/titlebuttons/titlebutton-unshade-hover.svg
    assets/icons/titlebuttons/titlebutton-maximize-backdrop.svg
    assets/icons/titlebuttons/titlebutton-unshade-active.svg
    assets/icons/titlebuttons/titlebutton-close-active.svg
    assets/icons/titlebuttons/titlebutton-menu.svg
    assets/icons/titlebuttons/titlebutton-menu-hover.svg
    assets/icons/titlebuttons/titlebutton-shade-backdrop-active.svg
    assets/icons/titlebuttons/titlebutton-unshade-backdrop-active.svg


)

qt6_add_resources(slm-desktop resources
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}/assets"
    FILES
        ${DESKTOP_SHELL_RESOURCE_FILES}
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(slm-desktop PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.slm-desktop
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(slm-desktop
    PRIVATE
        Qt6::Quick
        Qt6::Network
        Qt6::DBus
        Qt6::Sql
        Qt6::Concurrent
        PkgConfig::GIO
        PkgConfig::WAYLAND_CLIENT
        ${SLM_FILEMANAGER_CORE_TARGET}
)
if(TARGET Qt6::WaylandClient)
    target_sources(slm-desktop PRIVATE
        src/core/wayland/layershell/dockbootstrapstate.cpp
        src/core/wayland/layershell/dockbootstrapstate.h
        src/core/wayland/layershell/wlrlayershell.cpp
        src/core/wayland/layershell/wlrlayershell.h
        src/core/wayland/layershell/docklayerwindow.cpp
        src/core/wayland/layershell/docklayerwindow.h
        src/core/wayland/layershell/qwayland-wlr-layer-shell-unstable-v1.cpp
        src/core/wayland/layershell/qwayland-wlr-layer-shell-unstable-v1.h
        src/core/wayland/layershell/wlr-layer-shell-protocol.c
    )
    target_include_directories(slm-desktop PRIVATE src/core/wayland/layershell)
    target_link_libraries(slm-desktop PRIVATE Qt6::WaylandClient Qt6::GuiPrivate)
    target_compile_definitions(slm-desktop PRIVATE SLM_HAVE_WAYLANDCLIENT)
endif()
if(TARGET Poppler::Qt6)
    target_link_libraries(slm-desktop PRIVATE Poppler::Qt6)
    target_compile_definitions(slm-desktop PRIVATE SLM_HAVE_POPPLER_QT6)
endif()

add_custom_command(TARGET slm-desktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:slm-desktop>
            $<TARGET_FILE_DIR:slm-desktop>/appSlm_Desktop
)

target_link_libraries(indicatorctl
    PRIVATE
        Qt6::Core
        Qt6::DBus
)

target_link_libraries(slm-settings
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickLayouts
        Qt6::DBus
        PkgConfig::GIO
)
target_link_libraries(desktop-contextd
    PRIVATE
        Qt6::Core
        Qt6::DBus
)
target_compile_definitions(slm-settings PRIVATE
    SLM_SHELL_VERSION="0.1.0-dev"
    SLM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
qt_add_resources(slm-settings "slm_desktop_theme_shim_qmldir"
    PREFIX "/qt/qml/Slm_Desktop"
    BASE "${CMAKE_SOURCE_DIR}/src/apps/settings/resources"
    FILES "${CMAKE_SOURCE_DIR}/src/apps/settings/resources/qmldir"
)
qt_add_resources(slm-settings "slm_desktop_theme_shim_theme"
    PREFIX "/qt/qml/Slm_Desktop"
    BASE "${CMAKE_SOURCE_DIR}/third_party/slm-style/qml/SlmStyle"
    FILES "${CMAKE_SOURCE_DIR}/third_party/slm-style/qml/SlmStyle/Theme.qml"
)
qt_add_resources(slm-settings "slm_settings_titlebuttons"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}/assets"
    FILES
        assets/icons/titlebuttons/titlebutton-close.svg
        assets/icons/titlebuttons/titlebutton-close-hover.svg
        assets/icons/titlebuttons/titlebutton-close-active.svg
        assets/icons/titlebuttons/titlebutton-close-backdrop.svg
        assets/icons/titlebuttons/titlebutton-close-backdrop-active.svg
        assets/icons/titlebuttons/titlebutton-minimize.svg
        assets/icons/titlebuttons/titlebutton-minimize-hover.svg
        assets/icons/titlebuttons/titlebutton-minimize-active.svg
        assets/icons/titlebuttons/titlebutton-minimize-backdrop.svg
        assets/icons/titlebuttons/titlebutton-minimize-backdrop-active.svg
        assets/icons/titlebuttons/titlebutton-maximize.svg
        assets/icons/titlebuttons/titlebutton-maximize-hover.svg
        assets/icons/titlebuttons/titlebutton-maximize-active.svg
        assets/icons/titlebuttons/titlebutton-maximize-backdrop.svg
        assets/icons/titlebuttons/titlebutton-maximize-backdrop-active.svg
        assets/icons/titlebuttons/titlebutton-unmaximize-hover.svg
        assets/icons/titlebuttons/titlebutton-unmaximize-active.svg
        assets/icons/titlebuttons/titlebutton-unmaximize-backdrop.svg
        assets/icons/titlebuttons/titlebutton-unmaximize-backdrop-active.svg
        assets/icons/titlebuttons/titlebutton-unmaximize.svg
)
target_include_directories(slm-settings PRIVATE
    ${CMAKE_SOURCE_DIR}/src/apps/settings
    ${CMAKE_SOURCE_DIR}/src/apps/settings/include
    ${CMAKE_SOURCE_DIR}/src/core/icons
)

include(GNUInstallDirs)
install(TARGETS slm-desktop
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Backward-compatible build alias for legacy scripts.
add_custom_target(appSlm_Desktop DEPENDS slm-desktop)

install(TARGETS indicatorctl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS slm-settings
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY src/apps/settings/modules/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/settings/modules
)

install(TARGETS windowingctl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS workspacectl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS fileopctl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS devicectl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS globalmenuctl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS slmcapdemo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS slm-fileopsd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS slm-devicesd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS slm-portald
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS slm-envd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS slm-settingsd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS slm-firewalld
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS slm-secretd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET slm-archived)
install(TARGETS slm-archived
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
endif()

install(TARGETS slm-package-policy-service
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY config/package-policy/
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/slm/package-policy
)

install(DIRECTORY scripts/package-policy/wrappers/
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/slm-package-policy/wrappers
    USE_SOURCE_PERMISSIONS
)

install(PROGRAMS
    scripts/package-policy/install-wrappers.sh
    scripts/package-policy/pre-transaction-snapshot.sh
    scripts/package-policy/post-transaction-health-check.sh
    scripts/package-policy/recover-last-snapshot.sh
    scripts/package-policy/disable-external-repos.sh
    scripts/package-policy/trigger-safe-mode-recovery.sh
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/slm-package-policy
)

install(PROGRAMS scripts/package-policy/install-wrappers.sh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES scripts/systemd/system/slm-package-policy.service
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system
)

enable_testing()

qt_add_executable(packagepolicy_engine_test
    src/services/packagepolicy/packagetransaction.cpp
    src/services/packagepolicy/packagetransaction.h
    src/services/packagepolicy/aptsimulator.cpp
    src/services/packagepolicy/aptsimulator.h
    src/services/packagepolicy/packagepolicyengine.cpp
    src/services/packagepolicy/packagepolicyengine.h
    tests/packagepolicy_engine_test.cpp
)
target_link_libraries(packagepolicy_engine_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_include_directories(packagepolicy_engine_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME packagepolicy_engine_test COMMAND packagepolicy_engine_test)

qt_add_executable(packagepolicy_oneshot_cli_test
    tests/packagepolicy_oneshot_cli_test.cpp
)
target_link_libraries(packagepolicy_oneshot_cli_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_compile_definitions(packagepolicy_oneshot_cli_test
    PRIVATE
        SLM_PACKAGE_POLICY_SERVICE_BIN_PATH="${CMAKE_BINARY_DIR}/slm-package-policy-service"
)
target_include_directories(packagepolicy_oneshot_cli_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME packagepolicy_oneshot_cli_test COMMAND packagepolicy_oneshot_cli_test)

qt_add_executable(secretservice_contract_test
    src/services/secret/secretservice.cpp
    src/services/secret/secretservice.h
    tests/secretservice_contract_test.cpp
)
target_link_libraries(secretservice_contract_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
target_include_directories(secretservice_contract_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME secretservice_contract_test COMMAND secretservice_contract_test)

qt_add_executable(dock_identity_test
    dockidentity.cpp
    dockidentity.h
    tests/dock_identity_test.cpp
)

target_link_libraries(dock_identity_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME dock_identity_test COMMAND dock_identity_test)

qt_add_executable(slmmotioncore_test
    src/core/motion/slmmotiontypes.cpp
    src/core/motion/slmmotiontypes.h
    src/core/motion/slmspringsolver.cpp
    src/core/motion/slmspringsolver.h
    src/core/motion/slmphysicsintegrator.cpp
    src/core/motion/slmphysicsintegrator.h
    src/core/motion/slmmotionpresetlibrary.cpp
    src/core/motion/slmmotionpresetlibrary.h
    src/core/motion/slmanimationengine.cpp
    src/core/motion/slmanimationengine.h
    src/core/motion/slmanimationscheduler.cpp
    src/core/motion/slmanimationscheduler.h
    src/core/motion/slmgesturebinding.cpp
    src/core/motion/slmgesturebinding.h
    tests/slmmotioncore_test.cpp
)

target_link_libraries(slmmotioncore_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_include_directories(slmmotioncore_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME slmmotioncore_test COMMAND slmmotioncore_test)

qt_add_executable(appcommandrouter_test
    src/core/execution/appcommandrouter.cpp
    src/core/execution/appcommandrouter.h
    screenshotmanager.cpp
    screenshotmanager.h
    tests/appcommandrouter_test.cpp
)

target_link_libraries(appcommandrouter_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Network
        Qt6::Test
)

add_test(NAME appcommandrouter_test COMMAND appcommandrouter_test)

qt_add_executable(windowingbackendmanager_test
    src/core/workspace/windowingbackendmanager.cpp
    src/core/workspace/windowingbackendmanager.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    tests/windowingbackendmanager_test.cpp
)

target_link_libraries(windowingbackendmanager_test
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::DBus
        Qt6::Concurrent
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME windowingbackendmanager_test COMMAND windowingbackendmanager_test)

qt_add_executable(kwinwaylandstatemodel_test
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    tests/kwinwaylandstatemodel_test.cpp
)

target_link_libraries(kwinwaylandstatemodel_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Concurrent
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME kwinwaylandstatemodel_test COMMAND kwinwaylandstatemodel_test)

qt_add_executable(kwinsupportinfoparser_test
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    tests/kwinsupportinfoparser_test.cpp
)

target_link_libraries(kwinsupportinfoparser_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME kwinsupportinfoparser_test COMMAND kwinsupportinfoparser_test)

qt_add_executable(fileoperationsmanager_test
    src/apps/filemanager/ops/fileoperationsmanager.cpp
    src/apps/filemanager/ops/fileoperationsmanager.h
    src/apps/filemanager/ops/fileoperationserrors.h
    tests/filemanager/fileoperationsmanager_test.cpp
)

target_link_libraries(fileoperationsmanager_test
    PRIVATE
        Qt6::Core
        Qt6::Concurrent
        Qt6::Test
)

add_test(NAME fileoperationsmanager_test COMMAND fileoperationsmanager_test)

set(SLM_ACTION_TEST_SOURCES
    src/core/actions/slmactiontypes.cpp
    src/core/actions/slmactiontypes.h
    src/core/actions/slmactioncondition.cpp
    src/core/actions/slmactioncondition.h
    src/core/actions/slmdesktopactionparser.cpp
    src/core/actions/slmdesktopactionparser.h
    src/core/actions/slmactiondiscovery.cpp
    src/core/actions/slmactiondiscovery.h
    src/core/actions/slmactionregistry.cpp
    src/core/actions/slmactionregistry.h
    src/core/execution/appexecutiongate.cpp
    src/core/execution/appexecutiongate.h
    src/core/execution/appruntimeregistry.cpp
    src/core/execution/appruntimeregistry.h
    src/core/launcher/applauncher.cpp
    src/core/launcher/applauncher.h
    src/core/launcher/launchenvresolver.cpp
    src/core/launcher/launchenvresolver.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
    dockidentity.cpp
    dockidentity.h
    dockmodel.cpp
    dockmodel.h
    shortcutmodel.cpp
    shortcutmodel.h
)

set(SLM_ACTION_FRAMEWORK_TEST_SOURCES
    src/core/actions/framework/slmdesktopentryscanner.cpp
    src/core/actions/framework/slmdesktopentryscanner.h
    src/core/actions/framework/slmframeworktypes.h
    src/core/actions/framework/acl/slmacllexer.cpp
    src/core/actions/framework/acl/slmacllexer.h
    src/core/actions/framework/acl/slmaclparser.cpp
    src/core/actions/framework/acl/slmaclparser.h
    src/core/actions/framework/slmdesktopentryparsermodule.cpp
    src/core/actions/framework/slmdesktopentryparsermodule.h
    src/core/actions/framework/slmmetadataparser.cpp
    src/core/actions/framework/slmmetadataparser.h
    src/core/actions/framework/slmactionregistryindex.cpp
    src/core/actions/framework/slmactionregistryindex.h
    src/core/actions/framework/slmactioncachelayer.cpp
    src/core/actions/framework/slmactioncachelayer.h
    src/core/actions/framework/slmcachemanager.cpp
    src/core/actions/framework/slmcachemanager.h
    src/core/actions/framework/slmcapabilityresolverlayer.cpp
    src/core/actions/framework/slmcapabilityresolverlayer.h
    src/core/actions/framework/slmcapabilitybuilders.cpp
    src/core/actions/framework/slmcapabilitybuilders.h
    src/core/actions/framework/slmgrouptreebuilder.cpp
    src/core/actions/framework/slmgrouptreebuilder.h
    src/core/actions/framework/slmactioninvoker.cpp
    src/core/actions/framework/slmactioninvoker.h
    src/core/actions/framework/slmactionframework.cpp
    src/core/actions/framework/slmactionframework.h
)

set(SLM_PERMISSION_TEST_SOURCES
    src/core/permissions/Capability.cpp
    src/core/permissions/Capability.h
    src/core/permissions/CallerIdentity.cpp
    src/core/permissions/CallerIdentity.h
    src/core/permissions/AccessContext.cpp
    src/core/permissions/AccessContext.h
    src/core/permissions/PolicyDecision.cpp
    src/core/permissions/PolicyDecision.h
    src/core/permissions/PermissionStore.cpp
    src/core/permissions/PermissionStore.h
    src/core/permissions/AuditLogger.cpp
    src/core/permissions/AuditLogger.h
    src/core/permissions/TrustResolver.cpp
    src/core/permissions/TrustResolver.h
    src/core/permissions/SensitiveContentPolicy.cpp
    src/core/permissions/SensitiveContentPolicy.h
    src/core/permissions/PolicyEngine.cpp
    src/core/permissions/PolicyEngine.h
    src/core/permissions/PermissionBroker.cpp
    src/core/permissions/PermissionBroker.h
    src/core/permissions/DBusSecurityGuard.cpp
    src/core/permissions/DBusSecurityGuard.h
    src/core/permissions/ConsentMediator.cpp
    src/core/permissions/ConsentMediator.h
)

qt_add_executable(appmodel_xbel_fallback_test
    appmodel.cpp
    appmodel.h
    ${SLM_ACTION_TEST_SOURCES}
    tests/appmodel_xbel_fallback_test.cpp
)

target_link_libraries(appmodel_xbel_fallback_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME appmodel_xbel_fallback_test COMMAND appmodel_xbel_fallback_test)

qt_add_executable(appmodel_scoring_weights_test
    appmodel.cpp
    appmodel.h
    ${SLM_ACTION_TEST_SOURCES}
    tests/appmodel_scoring_weights_test.cpp
)

target_link_libraries(appmodel_scoring_weights_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME appmodel_scoring_weights_test COMMAND appmodel_scoring_weights_test)

qt_add_executable(fileoperationsservice_dbus_test
    src/apps/filemanager/ops/fileoperationsmanager.cpp
    src/apps/filemanager/ops/fileoperationsmanager.h
    src/apps/filemanager/ops/fileoperationserrors.h
    src/apps/filemanager/ops/fileoperationsservice.cpp
    src/apps/filemanager/ops/fileoperationsservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/filemanager/fileoperationsservice_dbus_test.cpp
)

target_include_directories(fileoperationsservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(fileoperationsservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Concurrent
        Qt6::Sql
        Qt6::Test
)

add_test(NAME fileoperationsservice_dbus_test COMMAND fileoperationsservice_dbus_test)

qt_add_executable(fileopctl_smoke_test
    src/apps/filemanager/ops/fileoperationsmanager.cpp
    src/apps/filemanager/ops/fileoperationsmanager.h
    src/apps/filemanager/ops/fileoperationserrors.h
    src/apps/filemanager/ops/fileoperationsservice.cpp
    src/apps/filemanager/ops/fileoperationsservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/filemanager/fileopctl_smoke_test.cpp
)

target_include_directories(fileopctl_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(fileopctl_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Concurrent
        Qt6::Sql
        Qt6::Test
)

add_dependencies(fileopctl_smoke_test fileopctl)
add_test(NAME fileopctl_smoke_test COMMAND fileopctl_smoke_test)

qt_add_executable(workspacectl_smoke_test
    appmodel.cpp
    appmodel.h
    src/daemon/desktopd/daemonhealthmonitor.cpp
    src/daemon/desktopd/daemonhealthmonitor.h
    src/daemon/desktopd/desktopdaemonservice.cpp
    src/daemon/desktopd/desktopdaemonservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    src/core/workspace/spacesmanager.cpp
    src/core/workspace/spacesmanager.h
    src/core/workspace/windowingbackendmanager.cpp
    src/core/workspace/windowingbackendmanager.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    src/core/workspace/workspacemanager.cpp
    src/core/workspace/workspacemanager.h
    tests/workspacectl_smoke_test.cpp
)

target_include_directories(workspacectl_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(workspacectl_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Concurrent
        Qt6::Test
        Qt6::Sql
        PkgConfig::GIO
)

add_dependencies(workspacectl_smoke_test workspacectl)
add_test(NAME workspacectl_smoke_test COMMAND workspacectl_smoke_test)

qt_add_executable(missingcomponentcontroller_test
    src/core/system/missingcomponentcontroller.cpp
    src/core/system/missingcomponentcontroller.h
    src/core/system/componentregistry.cpp
    src/core/system/componentregistry.h
    src/core/system/dependencyguard.cpp
    src/core/system/dependencyguard.h
    tests/missingcomponentcontroller_test.cpp
)

target_include_directories(missingcomponentcontroller_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(missingcomponentcontroller_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME missingcomponentcontroller_test COMMAND missingcomponentcontroller_test)

qt_add_executable(spacesmanager_invariants_test
    src/core/workspace/spacesmanager.cpp
    src/core/workspace/spacesmanager.h
    tests/spacesmanager_invariants_test.cpp
)

target_link_libraries(spacesmanager_invariants_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_include_directories(spacesmanager_invariants_test PRIVATE
    src/core/workspace
)

add_test(NAME spacesmanager_invariants_test COMMAND spacesmanager_invariants_test)

qt_add_executable(workspacemanager_rebranding_test
    src/core/workspace/workspacemanager.cpp
    src/core/workspace/workspacemanager.h
    src/core/workspace/spacesmanager.cpp
    src/core/workspace/spacesmanager.h
    src/core/workspace/windowingbackendmanager.cpp
    src/core/workspace/windowingbackendmanager.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    tests/workspacemanager_rebranding_test.cpp
)

target_link_libraries(workspacemanager_rebranding_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Concurrent
        Qt6::Test
        PkgConfig::GIO
)
target_include_directories(workspacemanager_rebranding_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME workspacemanager_rebranding_test COMMAND workspacemanager_rebranding_test)

qt_add_executable(workspacemanager_workspace_ops_test
    src/core/workspace/spacesmanager.cpp
    src/core/workspace/spacesmanager.h
    src/core/workspace/workspacemanager.cpp
    src/core/workspace/workspacemanager.h
    src/core/workspace/windowingbackendmanager.cpp
    src/core/workspace/windowingbackendmanager.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    tests/workspacemanager_workspace_ops_test.cpp
)

target_link_libraries(workspacemanager_workspace_ops_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Concurrent
        Qt6::Test
        PkgConfig::GIO
)
target_include_directories(workspacemanager_workspace_ops_test PRIVATE
    src/core/workspace
)

add_test(NAME workspacemanager_workspace_ops_test COMMAND workspacemanager_workspace_ops_test)

qt_add_executable(windowthumbnaillayoutengine_test
    src/core/workspace/windowthumbnaillayoutengine.cpp
    src/core/workspace/windowthumbnaillayoutengine.h
    tests/windowthumbnaillayoutengine_test.cpp
)

target_link_libraries(windowthumbnaillayoutengine_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

target_include_directories(windowthumbnaillayoutengine_test PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME windowthumbnaillayoutengine_test COMMAND windowthumbnaillayoutengine_test)

qt_add_executable(workspace_shortcuts_guard_test
    tests/workspace_shortcuts_guard_test.cpp
)

target_compile_definitions(workspace_shortcuts_guard_test
    PRIVATE
        DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_link_libraries(workspace_shortcuts_guard_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME workspace_shortcuts_guard_test COMMAND workspace_shortcuts_guard_test)

qt_add_executable(workspace_overlay_guard_test
    tests/workspace_overlay_guard_test.cpp
)

target_compile_definitions(workspace_overlay_guard_test
    PRIVATE
        DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_link_libraries(workspace_overlay_guard_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME workspace_overlay_guard_test COMMAND workspace_overlay_guard_test)

qt_add_executable(motion_scheduler_ui_guard_test
    tests/motion_scheduler_ui_guard_test.cpp
)

target_compile_definitions(motion_scheduler_ui_guard_test
    PRIVATE
        DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_link_libraries(motion_scheduler_ui_guard_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME motion_scheduler_ui_guard_test COMMAND motion_scheduler_ui_guard_test)

qt_add_executable(topbar_mainmenu_recent_icons_guard_test
    tests/topbar_mainmenu_recent_icons_guard_test.cpp
)

target_compile_definitions(topbar_mainmenu_recent_icons_guard_test
    PRIVATE
        DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_link_libraries(topbar_mainmenu_recent_icons_guard_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME topbar_mainmenu_recent_icons_guard_test COMMAND topbar_mainmenu_recent_icons_guard_test)

qt_add_executable(notification_animation_contract_test
    tests/notification_animation_contract_test.cpp
)

target_compile_definitions(notification_animation_contract_test
    PRIVATE
        DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_link_libraries(notification_animation_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME notification_animation_contract_test COMMAND notification_animation_contract_test)

qt_add_executable(notification_visual_contract_test
    tests/notification_visual_contract_test.cpp
)

target_compile_definitions(notification_visual_contract_test
    PRIVATE
        DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_link_libraries(notification_visual_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME notification_visual_contract_test COMMAND notification_visual_contract_test)

qt_add_executable(notificationmanager_priority_routing_test
    src/services/notifications/notificationmanager.cpp
    src/services/notifications/notificationmanager.h
    src/services/notifications/desktopnotificationadaptor.cpp
    src/services/notifications/desktopnotificationadaptor.h
    tests/notificationmanager_priority_routing_test.cpp
)

target_include_directories(notificationmanager_priority_routing_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(notificationmanager_priority_routing_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME notificationmanager_priority_routing_test COMMAND notificationmanager_priority_routing_test)

qt_add_executable(docksystem_render_contract_test
    tests/docksystem_render_contract_test.cpp
)

target_compile_definitions(docksystem_render_contract_test
    PRIVATE
        DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_link_libraries(docksystem_render_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME docksystem_render_contract_test COMMAND docksystem_render_contract_test)

qt_add_executable(filemanager_storage_notification_contract_test
    tests/filemanager_storage_notification_contract_test.cpp
)

target_compile_definitions(filemanager_storage_notification_contract_test
    PRIVATE
        DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_link_libraries(filemanager_storage_notification_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME filemanager_storage_notification_contract_test COMMAND filemanager_storage_notification_contract_test)

qt_add_executable(storage_automount_contract_test
    tests/storage_automount_contract_test.cpp
)

target_compile_definitions(storage_automount_contract_test
    PRIVATE
        DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

target_link_libraries(storage_automount_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME storage_automount_contract_test COMMAND storage_automount_contract_test)

qt_add_executable(storagepolicystore_migration_test
    src/services/storage/storagepolicystore.cpp
    src/services/storage/storagepolicystore.h
    tests/storagepolicystore_migration_test.cpp
)

target_include_directories(storagepolicystore_migration_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(storagepolicystore_migration_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME storagepolicystore_migration_test COMMAND storagepolicystore_migration_test)

qt_add_executable(storage_runtime_smoke_test
    tests/storage_runtime_smoke_test.cpp
)

target_link_libraries(storage_runtime_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME storage_runtime_smoke_test COMMAND storage_runtime_smoke_test)
set_tests_properties(storage_runtime_smoke_test PROPERTIES
    LABELS "storage-smoke"
)

qt_add_executable(shell_state_controller_test
    src/core/shell/shellstatecontroller.cpp
    src/core/shell/shellstatecontroller.h
    tests/shell_state_controller_test.cpp
)

target_link_libraries(shell_state_controller_test
    PRIVATE
        Qt6::Core
        Qt6::Qml
        Qt6::Test
)

target_include_directories(shell_state_controller_test PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME shell_state_controller_test COMMAND shell_state_controller_test)

qt_add_executable(shell_input_router_test
    src/core/shell/shellstatecontroller.cpp
    src/core/shell/shellstatecontroller.h
    src/core/shell/shellinputrouter.cpp
    src/core/shell/shellinputrouter.h
    tests/shell_input_router_test.cpp
)

target_link_libraries(shell_input_router_test
    PRIVATE
        Qt6::Core
        Qt6::Qml
        Qt6::Test
)

target_include_directories(shell_input_router_test PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME shell_input_router_test COMMAND shell_input_router_test)

qt_add_executable(shell_layer_watchdog_test
    src/core/shell/shellstatecontroller.cpp
    src/core/shell/shellstatecontroller.h
    src/core/shell/shelllayerwatchdog.cpp
    src/core/shell/shelllayerwatchdog.h
    tests/shell_layer_watchdog_test.cpp
)

target_link_libraries(shell_layer_watchdog_test
    PRIVATE
        Qt6::Core
        Qt6::Qml
        Qt6::Test
)

target_include_directories(shell_layer_watchdog_test PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME shell_layer_watchdog_test COMMAND shell_layer_watchdog_test)

qt_add_executable(shell_contract_test
    src/core/shell/shellstatecontroller.cpp
    src/core/shell/shellstatecontroller.h
    src/core/shell/shelllayerwatchdog.cpp
    src/core/shell/shelllayerwatchdog.h
    tests/shell_contract_test.cpp
)

target_link_libraries(shell_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Qml
        Qt6::Test
)

target_include_directories(shell_contract_test PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(shell_contract_test PRIVATE
    SLM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME shell_contract_test COMMAND shell_contract_test)

qt_add_executable(workspace_ui_smoke_test
    tests/workspace_ui_smoke_test.cpp
)

target_link_libraries(workspace_ui_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)

target_include_directories(workspace_ui_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME workspace_ui_smoke_test COMMAND workspace_ui_smoke_test)
set_tests_properties(workspace_ui_smoke_test PROPERTIES
    ENVIRONMENT
        "WORKSPACE_QML_COMPONENT_DIR=${CMAKE_SOURCE_DIR}/Qml/components/workspace;BUILD_QML_IMPORT_ROOT=${CMAKE_BINARY_DIR}/qml"
)

qt_add_executable(workspace_e2e_smoke_test
    src/core/workspace/spacesmanager.cpp
    src/core/workspace/spacesmanager.h
    src/core/workspace/workspacemanager.cpp
    src/core/workspace/workspacemanager.h
    src/core/workspace/windowingbackendmanager.cpp
    src/core/workspace/windowingbackendmanager.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    src/core/workspace/multitaskingcontroller.cpp
    src/core/workspace/multitaskingcontroller.h
    tests/workspace_e2e_smoke_test.cpp
)

target_link_libraries(workspace_e2e_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Concurrent
        Qt6::Test
        PkgConfig::GIO
)

target_include_directories(workspace_e2e_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME workspace_e2e_smoke_test COMMAND workspace_e2e_smoke_test)

qt_add_executable(devicectl_smoke_test
    src/services/storage/storagemanager.cpp
    src/services/storage/storagemanager.h
    src/services/storage/storagepolicystore.cpp
    src/services/storage/storagepolicystore.h
    src/daemon/devicesd/devicesmanager.cpp
    src/daemon/devicesd/devicesmanager.h
    src/daemon/devicesd/devicesservice.cpp
    src/daemon/devicesd/devicesservice.h
    src/daemon/devicesd/storageservice.cpp
    src/daemon/devicesd/storageservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/devicectl_smoke_test.cpp
)

target_include_directories(devicectl_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(devicectl_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

add_dependencies(devicectl_smoke_test devicectl)
add_test(NAME devicectl_smoke_test COMMAND devicectl_smoke_test)

qt_add_executable(windowingctl_smoke_test
    src/core/workspace/windowingbackendmanager.cpp
    src/core/workspace/windowingbackendmanager.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/windowingctl_smoke_test.cpp
)

target_include_directories(windowingctl_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(windowingctl_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Concurrent
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

add_dependencies(windowingctl_smoke_test windowingctl)
add_test(NAME windowingctl_smoke_test COMMAND windowingctl_smoke_test)

qt_add_executable(globalmenuctl_smoke_test
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/globalmenuctl_smoke_test.cpp
)

target_include_directories(globalmenuctl_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(globalmenuctl_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_dependencies(globalmenuctl_smoke_test globalmenuctl)
add_test(NAME globalmenuctl_smoke_test COMMAND globalmenuctl_smoke_test)

qt_add_executable(globalmenumanager_cache_test
    globalmenumanager.cpp
    globalmenumanager.h
    tests/globalmenumanager_cache_test.cpp
)

target_link_libraries(globalmenumanager_cache_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Test
)

add_test(NAME globalmenumanager_cache_test COMMAND globalmenumanager_cache_test)
set_tests_properties(globalmenumanager_cache_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(statusnotifierhost_test
    src/services/indicator/statusnotifierhost.cpp
    src/services/indicator/statusnotifierhost.h
    tests/statusnotifierhost_test.cpp
)

target_link_libraries(statusnotifierhost_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Test
)

add_test(NAME statusnotifierhost_test COMMAND statusnotifierhost_test)
set_tests_properties(statusnotifierhost_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(filemanagerapi_daemon_recovery_test
    src/services/storage/storagemanager.cpp
    src/services/storage/storagemanager.h
    src/services/storage/storagepolicystore.cpp
    src/services/storage/storagepolicystore.h
    src/apps/filemanager/filemanagerapi.cpp
    src/apps/filemanager/filemanagerapi_open.cpp
    src/apps/filemanager/filemanagerapi_archive.cpp
    src/apps/filemanager/filemanagerapi_favorites.cpp
    src/apps/filemanager/filemanagerapi_watch.cpp
    src/apps/filemanager/filemanagerapi_thumbnail.cpp
    src/apps/filemanager/filemanagerapi_batch.cpp
    src/apps/filemanager/filemanagerapi_recent.cpp
    src/apps/filemanager/filemanagerapi_openwith.cpp
    src/apps/filemanager/filemanagerapi_slmactions.cpp
    src/apps/filemanager/filemanagerapi_storage.cpp
    src/apps/filemanager/filemanagerapi_fs.cpp
    src/apps/filemanager/filemanagerapi_listing.cpp
    src/apps/filemanager/filemanagerapi_io.cpp
    src/apps/filemanager/filemanagerapi_metadata.cpp
    src/apps/filemanager/filemanagerapi_sharing.cpp
    src/apps/filemanager/filemanagerapi_portal.cpp
    src/apps/filemanager/filemanagerapi_daemon_transfer.cpp
    src/apps/filemanager/filemanagerapi_daemon_state.cpp
    src/apps/filemanager/filemanagerapi_daemon_remove.cpp
    src/apps/filemanager/filemanagerapi_daemon_emptytrash.cpp
    src/apps/filemanager/filemanagerapi_daemon_control.cpp
    src/apps/filemanager/filemanagerapi_daemon_recovery_state.cpp
    src/apps/filemanager/filemanagerapi_daemon_recovery_signals.cpp
    src/apps/filemanager/filemanagerapi_batch_control.cpp
    src/core/actions/slmactiontypes.cpp
    src/core/actions/slmactiontypes.h
    src/core/actions/slmactioncondition.cpp
    src/core/actions/slmactioncondition.h
    src/core/actions/slmdesktopactionparser.cpp
    src/core/actions/slmdesktopactionparser.h
    src/core/actions/slmactiondiscovery.cpp
    src/core/actions/slmactiondiscovery.h
    src/core/actions/slmactionregistry.cpp
    src/core/actions/slmactionregistry.h
    src/core/actions/framework/slmdesktopentryscanner.cpp
    src/core/actions/framework/slmdesktopentryscanner.h
    src/core/actions/framework/slmframeworktypes.h
    src/core/actions/framework/acl/slmacllexer.cpp
    src/core/actions/framework/acl/slmacllexer.h
    src/core/actions/framework/acl/slmaclparser.cpp
    src/core/actions/framework/acl/slmaclparser.h
    src/core/actions/framework/slmdesktopentryparsermodule.cpp
    src/core/actions/framework/slmdesktopentryparsermodule.h
    src/core/actions/framework/slmmetadataparser.cpp
    src/core/actions/framework/slmmetadataparser.h
    src/core/actions/framework/slmactionregistryindex.cpp
    src/core/actions/framework/slmactionregistryindex.h
    src/core/actions/framework/slmactioncachelayer.cpp
    src/core/actions/framework/slmactioncachelayer.h
    src/core/actions/framework/slmcachemanager.cpp
    src/core/actions/framework/slmcachemanager.h
    src/core/actions/framework/slmcapabilityresolverlayer.cpp
    src/core/actions/framework/slmcapabilityresolverlayer.h
    src/core/actions/framework/slmcapabilitybuilders.cpp
    src/core/actions/framework/slmcapabilitybuilders.h
    src/core/actions/framework/slmgrouptreebuilder.cpp
    src/core/actions/framework/slmgrouptreebuilder.h
    src/core/actions/framework/slmactioninvoker.cpp
    src/core/actions/framework/slmactioninvoker.h
    src/core/actions/framework/slmactionframework.cpp
    src/core/actions/framework/slmactionframework.h
    src/core/execution/appexecutiongate.cpp
    src/core/execution/appexecutiongate.h
    src/core/execution/appruntimeregistry.cpp
    src/core/execution/appruntimeregistry.h
    src/core/launcher/applauncher.cpp
    src/core/launcher/applauncher.h
    src/core/launcher/launchenvresolver.cpp
    src/core/launcher/launchenvresolver.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
    dockidentity.cpp
    dockidentity.h
    dockmodel.cpp
    dockmodel.h
    shortcutmodel.cpp
    shortcutmodel.h
    src/apps/filemanager/include/filemanagerapi.h
    src/core/system/componentregistry.cpp
    src/core/system/componentregistry.h
    src/core/system/dependencyguard.cpp
    src/core/system/dependencyguard.h
    src/apps/filemanager/ops/fileoperationsmanager.cpp
    src/apps/filemanager/ops/fileoperationsmanager.h
    src/apps/filemanager/ops/fileoperationserrors.h
    src/apps/filemanager/ops/fileoperationsservice.cpp
    src/apps/filemanager/ops/fileoperationsservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/filemanager/filemanagerapi_daemon_recovery_test.cpp
)
target_include_directories(filemanagerapi_daemon_recovery_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/filemanager/include
)

target_link_libraries(filemanagerapi_daemon_recovery_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Sql
        Qt6::Concurrent
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME filemanagerapi_daemon_recovery_test COMMAND filemanagerapi_daemon_recovery_test)
set_tests_properties(filemanagerapi_daemon_recovery_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(NAME filemanagerapi_daemon_recovery_strict_test COMMAND filemanagerapi_daemon_recovery_test)
set_tests_properties(filemanagerapi_daemon_recovery_strict_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SLM_RUN_STRICT_FILEOPS_RECOVERY=1"
    LABELS "stress;dbus"
)

qt_add_executable(filemanagerapi_fileops_contract_test
    src/services/storage/storagemanager.cpp
    src/services/storage/storagemanager.h
    src/services/storage/storagepolicystore.cpp
    src/services/storage/storagepolicystore.h
    src/apps/filemanager/filemanagerapi.cpp
    src/apps/filemanager/filemanagerapi_open.cpp
    src/apps/filemanager/filemanagerapi_archive.cpp
    src/apps/filemanager/filemanagerapi_favorites.cpp
    src/apps/filemanager/filemanagerapi_watch.cpp
    src/apps/filemanager/filemanagerapi_thumbnail.cpp
    src/apps/filemanager/filemanagerapi_batch.cpp
    src/apps/filemanager/filemanagerapi_recent.cpp
    src/apps/filemanager/filemanagerapi_openwith.cpp
    src/apps/filemanager/filemanagerapi_slmactions.cpp
    src/apps/filemanager/filemanagerapi_storage.cpp
    src/apps/filemanager/filemanagerapi_fs.cpp
    src/apps/filemanager/filemanagerapi_listing.cpp
    src/apps/filemanager/filemanagerapi_io.cpp
    src/apps/filemanager/filemanagerapi_metadata.cpp
    src/apps/filemanager/filemanagerapi_sharing.cpp
    src/apps/filemanager/filemanagerapi_portal.cpp
    src/apps/filemanager/filemanagerapi_daemon_transfer.cpp
    src/apps/filemanager/filemanagerapi_daemon_state.cpp
    src/apps/filemanager/filemanagerapi_daemon_remove.cpp
    src/apps/filemanager/filemanagerapi_daemon_emptytrash.cpp
    src/apps/filemanager/filemanagerapi_daemon_control.cpp
    src/apps/filemanager/filemanagerapi_daemon_recovery_state.cpp
    src/apps/filemanager/filemanagerapi_daemon_recovery_signals.cpp
    src/apps/filemanager/filemanagerapi_batch_control.cpp
    src/core/actions/slmactiontypes.cpp
    src/core/actions/slmactiontypes.h
    src/core/actions/slmactioncondition.cpp
    src/core/actions/slmactioncondition.h
    src/core/actions/slmdesktopactionparser.cpp
    src/core/actions/slmdesktopactionparser.h
    src/core/actions/slmactiondiscovery.cpp
    src/core/actions/slmactiondiscovery.h
    src/core/actions/slmactionregistry.cpp
    src/core/actions/slmactionregistry.h
    src/core/actions/framework/slmdesktopentryscanner.cpp
    src/core/actions/framework/slmdesktopentryscanner.h
    src/core/actions/framework/slmframeworktypes.h
    src/core/actions/framework/acl/slmacllexer.cpp
    src/core/actions/framework/acl/slmacllexer.h
    src/core/actions/framework/acl/slmaclparser.cpp
    src/core/actions/framework/acl/slmaclparser.h
    src/core/actions/framework/slmdesktopentryparsermodule.cpp
    src/core/actions/framework/slmdesktopentryparsermodule.h
    src/core/actions/framework/slmmetadataparser.cpp
    src/core/actions/framework/slmmetadataparser.h
    src/core/actions/framework/slmactionregistryindex.cpp
    src/core/actions/framework/slmactionregistryindex.h
    src/core/actions/framework/slmactioncachelayer.cpp
    src/core/actions/framework/slmactioncachelayer.h
    src/core/actions/framework/slmcachemanager.cpp
    src/core/actions/framework/slmcachemanager.h
    src/core/actions/framework/slmcapabilityresolverlayer.cpp
    src/core/actions/framework/slmcapabilityresolverlayer.h
    src/core/actions/framework/slmcapabilitybuilders.cpp
    src/core/actions/framework/slmcapabilitybuilders.h
    src/core/actions/framework/slmgrouptreebuilder.cpp
    src/core/actions/framework/slmgrouptreebuilder.h
    src/core/actions/framework/slmactioninvoker.cpp
    src/core/actions/framework/slmactioninvoker.h
    src/core/actions/framework/slmactionframework.cpp
    src/core/actions/framework/slmactionframework.h
    src/core/execution/appexecutiongate.cpp
    src/core/execution/appexecutiongate.h
    src/core/execution/appruntimeregistry.cpp
    src/core/execution/appruntimeregistry.h
    src/core/launcher/applauncher.cpp
    src/core/launcher/applauncher.h
    src/core/launcher/launchenvresolver.cpp
    src/core/launcher/launchenvresolver.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
    dockidentity.cpp
    dockidentity.h
    dockmodel.cpp
    dockmodel.h
    shortcutmodel.cpp
    shortcutmodel.h
    src/apps/filemanager/include/filemanagerapi.h
    src/core/system/componentregistry.cpp
    src/core/system/componentregistry.h
    src/core/system/dependencyguard.cpp
    src/core/system/dependencyguard.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/filemanager/filemanagerapi_fileops_contract_test.cpp
)
target_include_directories(filemanagerapi_fileops_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/filemanager/include
)

target_link_libraries(filemanagerapi_fileops_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Sql
        Qt6::Concurrent
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME filemanagerapi_fileops_contract_test COMMAND filemanagerapi_fileops_contract_test)
set_tests_properties(filemanagerapi_fileops_contract_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

if(LIBARCHIVE_FOUND)
qt_add_executable(archivebackend_test
    src/services/archive/archivebackend.cpp
    src/services/archive/archivebackend.h
    tests/archivebackend_test.cpp
)
target_include_directories(archivebackend_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(archivebackend_test
    PRIVATE
        Qt6::Core
        Qt6::Test
        PkgConfig::LIBARCHIVE
)
add_test(NAME archivebackend_test COMMAND $<TARGET_FILE:archivebackend_test>)
endif()

# Contract test: FileManagerShellBridge ↔ fileopsd integration
qt_add_executable(filemanager_shell_contract_test
    ${FILEMANAGER_APP_SOURCES}
    ${FILEMANAGER_OPS_SERVICE_SOURCES}
    ${FILEMANAGER_PROGRESS_SOURCES}
    ${SLM_ACTION_FRAMEWORK_SOURCES}
    ${SLM_PERMISSION_TEST_SOURCES}
    appmodel.cpp
    appmodel.h
    dockidentity.cpp
    dockidentity.h
    dockmodel.cpp
    dockmodel.h
    shortcutmodel.cpp
    shortcutmodel.h
    src/core/execution/appexecutiongate.cpp
    src/core/execution/appexecutiongate.h
    src/core/execution/appruntimeregistry.cpp
    src/core/execution/appruntimeregistry.h
    src/core/launcher/applauncher.cpp
    src/core/launcher/applauncher.h
    src/core/launcher/launchenvresolver.cpp
    src/core/launcher/launchenvresolver.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
    src/filemanager/FileManagerShellBridge.cpp
    src/filemanager/FileManagerShellBridge.h
    src/core/system/componentregistry.cpp
    src/core/system/componentregistry.h
    src/core/system/dependencyguard.cpp
    src/core/system/dependencyguard.h
    tests/filemanager/filemanager_shell_contract_test.cpp
)
target_include_directories(filemanager_shell_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/filemanager/include
)
target_link_libraries(filemanager_shell_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Sql
        Qt6::Concurrent
        Qt6::Test
        PkgConfig::GIO
)
add_test(NAME filemanager_shell_contract_test COMMAND filemanager_shell_contract_test)
set_tests_properties(filemanager_shell_contract_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SLM_DEV_MODE=1"
)

qt_add_executable(sessionstateservice_dbus_test
    src/daemon/desktopd/sessionstatemanager.cpp
    src/daemon/desktopd/sessionstatemanager.h
    src/daemon/desktopd/sessionauthenticator.cpp
    src/daemon/desktopd/sessionauthenticator.h
    src/daemon/desktopd/sessionunlockthrottle.cpp
    src/daemon/desktopd/sessionunlockthrottle.h
    src/daemon/desktopd/sessionstateservice.cpp
    src/daemon/desktopd/sessionstateservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    src/core/workspace/windowingbackendmanager.cpp
    src/core/workspace/windowingbackendmanager.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    tests/sessionstateservice_dbus_test.cpp
)

target_include_directories(sessionstateservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(sessionstateservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Network
        Qt6::Concurrent
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME sessionstateservice_dbus_test COMMAND sessionstateservice_dbus_test)

qt_add_executable(sessionunlockthrottle_test
    src/daemon/desktopd/sessionunlockthrottle.cpp
    src/daemon/desktopd/sessionunlockthrottle.h
    tests/sessionunlockthrottle_test.cpp
)

target_include_directories(sessionunlockthrottle_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(sessionunlockthrottle_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME sessionunlockthrottle_test COMMAND sessionunlockthrottle_test)

qt_add_executable(devicesservice_dbus_test
    src/services/storage/storagemanager.cpp
    src/services/storage/storagemanager.h
    src/services/storage/storagepolicystore.cpp
    src/services/storage/storagepolicystore.h
    src/daemon/devicesd/devicesmanager.cpp
    src/daemon/devicesd/devicesmanager.h
    src/daemon/devicesd/devicesservice.cpp
    src/daemon/devicesd/devicesservice.h
    src/daemon/devicesd/storageservice.cpp
    src/daemon/devicesd/storageservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/devicesservice_dbus_test.cpp
)
 
target_include_directories(devicesservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(devicesservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME devicesservice_dbus_test COMMAND devicesservice_dbus_test)

qt_add_executable(storageservice_dbus_test
    src/services/storage/storagemanager.cpp
    src/services/storage/storagemanager.h
    src/services/storage/storagepolicystore.cpp
    src/services/storage/storagepolicystore.h
    src/daemon/devicesd/devicesmanager.cpp
    src/daemon/devicesd/devicesmanager.h
    src/daemon/devicesd/storageservice.cpp
    src/daemon/devicesd/storageservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/storageservice_dbus_test.cpp
)

target_include_directories(storageservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(storageservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME storageservice_dbus_test COMMAND storageservice_dbus_test)

qt_add_executable(settingsservice_dbus_test
    src/services/settingsd/settingsstore.cpp
    src/services/settingsd/settingsstore.h
    src/services/settingsd/settingsservice.cpp
    src/services/settingsd/settingsservice.h
    src/services/theme-policy/appthemeclassifier.cpp
    src/services/theme-policy/appthemeclassifier.h
    src/services/theme-policy/themepolicyengine.cpp
    src/services/theme-policy/themepolicyengine.h
    tests/settingsservice_dbus_test.cpp
)

target_include_directories(settingsservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(settingsservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME settingsservice_dbus_test COMMAND settingsservice_dbus_test)

qt_add_executable(firewallservice_dbus_contract_test
    src/daemon/firewalld/firewallservice.cpp
    src/daemon/firewalld/firewallservice.h
    src/daemon/firewalld/firewalldbusadaptor.cpp
    src/daemon/firewalld/firewalldbusadaptor.h
    src/daemon/firewalld/firewalltypes.h
    src/services/firewall/policyengine.cpp
    src/services/firewall/policyengine.h
    src/services/firewall/policystore.cpp
    src/services/firewall/policystore.h
    src/services/firewall/nftablesadapter.cpp
    src/services/firewall/nftablesadapter.h
    src/services/firewall/appidentityclient.cpp
    src/services/firewall/appidentityclient.h
    tests/firewallservice_dbus_contract_test.cpp
)

target_include_directories(firewallservice_dbus_contract_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(firewallservice_dbus_contract_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME firewallservice_dbus_contract_test COMMAND firewallservice_dbus_contract_test)

qt_add_executable(firewall_policyengine_contract_test
    src/daemon/firewalld/firewalltypes.h
    src/services/firewall/policyengine.cpp
    src/services/firewall/policyengine.h
    src/services/firewall/policystore.cpp
    src/services/firewall/policystore.h
    src/services/firewall/nftablesadapter.cpp
    src/services/firewall/nftablesadapter.h
    src/services/firewall/appidentityclient.cpp
    src/services/firewall/appidentityclient.h
    tests/firewall_policyengine_contract_test.cpp
)

target_include_directories(firewall_policyengine_contract_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(firewall_policyengine_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME firewall_policyengine_contract_test COMMAND firewall_policyengine_contract_test)

qt_add_executable(firewall_nftables_adapter_test
    src/services/firewall/nftablesadapter.cpp
    src/services/firewall/nftablesadapter.h
    tests/firewall_nftables_adapter_test.cpp
)

target_include_directories(firewall_nftables_adapter_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(firewall_nftables_adapter_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME firewall_nftables_adapter_test COMMAND firewall_nftables_adapter_test)

qt_add_executable(firewall_identity_mapping_contract_test
    src/services/firewall/appidentityclient.cpp
    src/services/firewall/appidentityclient.h
    tests/firewall_identity_mapping_contract_test.cpp
)

target_include_directories(firewall_identity_mapping_contract_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(firewall_identity_mapping_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME firewall_identity_mapping_contract_test COMMAND firewall_identity_mapping_contract_test)

qt_add_executable(settings_searchengine_context_ranking_test
    src/apps/settings/moduleloader.cpp
    src/apps/settings/moduleloader.h
    src/apps/settings/searchengine.cpp
    src/apps/settings/searchengine.h
    tests/settings_searchengine_context_ranking_test.cpp
)

target_include_directories(settings_searchengine_context_ranking_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(settings_searchengine_context_ranking_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME settings_searchengine_context_ranking_test COMMAND settings_searchengine_context_ranking_test)

qt_add_executable(contextservice_dbus_test
    src/services/context/contextservice.cpp
    src/services/context/contextservice.h
    src/services/context/ruleengine.cpp
    src/services/context/ruleengine.h
    tests/contextservice_dbus_test.cpp
)

target_include_directories(contextservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(contextservice_dbus_test PRIVATE SLM_CONTEXT_TEST_HOOKS)

target_link_libraries(contextservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME contextservice_dbus_test COMMAND $<TARGET_FILE:contextservice_dbus_test>)

qt_add_executable(contextclient_test
    src/services/context/contextclient.cpp
    src/services/context/contextclient.h
    tests/contextclient_test.cpp
)

target_include_directories(contextclient_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(contextclient_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME contextclient_test COMMAND $<TARGET_FILE:contextclient_test>)

qt_add_executable(desktopsettingsclient_test
    src/apps/settings/desktopsettingsclient.cpp
    src/apps/settings/desktopsettingsclient.h
    tests/desktopsettingsclient_test.cpp
)

target_include_directories(desktopsettingsclient_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(desktopsettingsclient_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        Qt6::Gui
)

add_test(NAME desktopsettingsclient_test COMMAND $<TARGET_FILE:desktopsettingsclient_test>)

qt_add_executable(appearance_contexttime_ui_contract_test
    tests/appearance_contexttime_ui_contract_test.cpp
)

target_include_directories(appearance_contexttime_ui_contract_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(appearance_contexttime_ui_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME appearance_contexttime_ui_contract_test COMMAND appearance_contexttime_ui_contract_test)

qt_add_executable(appthemeclassifier_test
    src/services/theme-policy/appthemeclassifier.cpp
    src/services/theme-policy/appthemeclassifier.h
    tests/appthemeclassifier_test.cpp
)

target_include_directories(appthemeclassifier_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(appthemeclassifier_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME appthemeclassifier_test COMMAND appthemeclassifier_test)

qt_add_executable(themepolicyengine_test
    src/services/theme-policy/appthemeclassifier.cpp
    src/services/theme-policy/appthemeclassifier.h
    src/services/theme-policy/themepolicyengine.cpp
    src/services/theme-policy/themepolicyengine.h
    tests/themepolicyengine_test.cpp
)

target_include_directories(themepolicyengine_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(themepolicyengine_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME themepolicyengine_test COMMAND themepolicyengine_test)

qt_add_executable(foldersharingservice_dbus_test
    src/daemon/desktopd/foldersharingservice.cpp
    src/daemon/desktopd/foldersharingservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/foldersharingservice_dbus_test.cpp
)

target_include_directories(foldersharingservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(foldersharingservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME foldersharingservice_dbus_test COMMAND foldersharingservice_dbus_test)

qt_add_executable(captureservice_dbus_test
    src/daemon/desktopd/captureservice.cpp
    src/daemon/desktopd/captureservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/captureservice_dbus_test.cpp
)

target_include_directories(captureservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(captureservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME captureservice_dbus_test COMMAND captureservice_dbus_test)

qt_add_executable(inputcaptureservice_dbus_test
    src/core/workspace/compositorinputcapturebackendservice.cpp
    src/core/workspace/compositorinputcapturebackendservice.h
    src/core/workspace/compositorinputcaptureprimitiveservice.cpp
    src/core/workspace/compositorinputcaptureprimitiveservice.h
    src/daemon/desktopd/inputcapturebackend.cpp
    src/daemon/desktopd/inputcapturebackend.h
    src/daemon/desktopd/inputcaptureservice.cpp
    src/daemon/desktopd/inputcaptureservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/inputcaptureservice_dbus_test.cpp
)

target_include_directories(inputcaptureservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(inputcaptureservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME inputcaptureservice_dbus_test COMMAND inputcaptureservice_dbus_test)

qt_add_executable(capturestreamingestor_dbus_test
    src/daemon/desktopd/captureservice.cpp
    src/daemon/desktopd/captureservice.h
    src/daemon/desktopd/capturestreamingestor.cpp
    src/daemon/desktopd/capturestreamingestor.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/capturestreamingestor_dbus_test.cpp
)

target_include_directories(capturestreamingestor_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(capturestreamingestor_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME capturestreamingestor_dbus_test COMMAND capturestreamingestor_dbus_test)

qt_add_executable(screencastservice_dbus_test
    src/daemon/desktopd/screencastservice.cpp
    src/daemon/desktopd/screencastservice.h
    src/daemon/desktopd/screencaststreambackend.cpp
    src/daemon/desktopd/screencaststreambackend.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/screencastservice_dbus_test.cpp
)

target_include_directories(screencastservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(screencastservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)
if(SLM_HAVE_PIPEWIRE)
    target_compile_definitions(screencastservice_dbus_test PRIVATE SLM_HAVE_PIPEWIRE=1)
    target_link_libraries(screencastservice_dbus_test PRIVATE PkgConfig::PIPEWIRE PkgConfig::SPA)
endif()

add_test(NAME screencastservice_dbus_test COMMAND screencastservice_dbus_test)

qt_add_executable(portalservice_dbus_test
    src/daemon/portald/portalmanager.cpp
    src/daemon/portald/portalmanager.h
    src/daemon/portald/portalservice.cpp
    src/daemon/portald/portalservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/portalservice_dbus_test.cpp
)

target_include_directories(portalservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(portalservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME portalservice_dbus_test COMMAND portalservice_dbus_test)

qt_add_executable(implportalservice_dbus_test
    src/daemon/portald/portalmanager.cpp
    src/daemon/portald/portalmanager.h
    src/daemon/portald/implportalservice.cpp
    src/daemon/portald/implportalservice.h
    src/daemon/portald/implportaladaptors.cpp
    src/daemon/portald/implportaladaptors.h
    src/daemon/portald/portal-adapter/PortalAppContextResolver.cpp
    src/daemon/portald/portal-adapter/PortalAppContextResolver.h
    src/daemon/portald/portal-adapter/PortalBackendService.cpp
    src/daemon/portald/portal-adapter/PortalBackendService.h
    src/daemon/portald/portal-adapter/PortalAccessMediator.cpp
    src/daemon/portald/portal-adapter/PortalAccessMediator.h
    src/daemon/portald/portal-adapter/PortalCapabilityMapper.cpp
    src/daemon/portald/portal-adapter/PortalCapabilityMapper.h
    src/daemon/portald/portal-adapter/PortalPermissionStoreAdapter.cpp
    src/daemon/portald/portal-adapter/PortalPermissionStoreAdapter.h
    src/daemon/portald/portal-adapter/PortalDialogBridge.cpp
    src/daemon/portald/portal-adapter/PortalDialogBridge.h
    src/daemon/portald/portal-adapter/PortalSecretBridge.cpp
    src/daemon/portald/portal-adapter/PortalSecretBridge.h
    src/daemon/portald/portal-adapter/PortalRequestManager.cpp
    src/daemon/portald/portal-adapter/PortalRequestManager.h
    src/daemon/portald/portal-adapter/PortalRequestObject.cpp
    src/daemon/portald/portal-adapter/PortalRequestObject.h
    src/daemon/portald/portal-adapter/PortalSessionManager.cpp
    src/daemon/portald/portal-adapter/PortalSessionManager.h
    src/daemon/portald/portal-adapter/PortalSessionObject.cpp
    src/daemon/portald/portal-adapter/PortalSessionObject.h
    src/daemon/portald/portal-adapter/PortalResponseSerializer.cpp
    src/daemon/portald/portal-adapter/PortalResponseSerializer.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/implportalservice_dbus_test.cpp
)

target_link_libraries(implportalservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

target_include_directories(implportalservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME implportalservice_dbus_test COMMAND implportalservice_dbus_test)

qt_add_executable(portal_secret_integration_test
    src/daemon/portald/portalmanager.cpp
    src/daemon/portald/portalmanager.h
    src/daemon/portald/portalservice.cpp
    src/daemon/portald/portalservice.h
    src/daemon/portald/portal-adapter/PortalBackendService.cpp
    src/daemon/portald/portal-adapter/PortalBackendService.h
    src/daemon/portald/portal-adapter/PortalAccessMediator.cpp
    src/daemon/portald/portal-adapter/PortalAccessMediator.h
    src/daemon/portald/portal-adapter/PortalCapabilityMapper.cpp
    src/daemon/portald/portal-adapter/PortalCapabilityMapper.h
    src/daemon/portald/portal-adapter/PortalPermissionStoreAdapter.cpp
    src/daemon/portald/portal-adapter/PortalPermissionStoreAdapter.h
    src/daemon/portald/portal-adapter/PortalDialogBridge.cpp
    src/daemon/portald/portal-adapter/PortalDialogBridge.h
    src/daemon/portald/portal-adapter/PortalRequestManager.cpp
    src/daemon/portald/portal-adapter/PortalRequestManager.h
    src/daemon/portald/portal-adapter/PortalRequestObject.cpp
    src/daemon/portald/portal-adapter/PortalRequestObject.h
    src/daemon/portald/portal-adapter/PortalSessionManager.cpp
    src/daemon/portald/portal-adapter/PortalSessionManager.h
    src/daemon/portald/portal-adapter/PortalSessionObject.cpp
    src/daemon/portald/portal-adapter/PortalSessionObject.h
    src/daemon/portald/portal-adapter/PortalResponseSerializer.cpp
    src/daemon/portald/portal-adapter/PortalResponseSerializer.h
    src/daemon/portald/portal-adapter/PortalSecretBridge.cpp
    src/daemon/portald/portal-adapter/PortalSecretBridge.h
    src/services/secret/secretservice.cpp
    src/services/secret/secretservice.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/portal_secret_integration_test.cpp
)

target_include_directories(portal_secret_integration_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(portal_secret_integration_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME portal_secret_integration_test COMMAND portal_secret_integration_test)
set_tests_properties(portal_secret_integration_test PROPERTIES
    LABELS "secret-consent"
)

qt_add_executable(screencast_revoke_e2e_test
    src/daemon/portald/portalmanager.cpp
    src/daemon/portald/portalmanager.h
    src/daemon/portald/implportalservice.cpp
    src/daemon/portald/implportalservice.h
    src/daemon/portald/implportaladaptors.cpp
    src/daemon/portald/implportaladaptors.h
    src/daemon/portald/portal-adapter/PortalAppContextResolver.cpp
    src/daemon/portald/portal-adapter/PortalAppContextResolver.h
    src/core/permissions/CallerIdentity.cpp
    src/core/permissions/CallerIdentity.h
    src/daemon/desktopd/captureservice.cpp
    src/daemon/desktopd/captureservice.h
    src/daemon/desktopd/screencastservice.cpp
    src/daemon/desktopd/screencastservice.h
    src/daemon/desktopd/screencaststreambackend.cpp
    src/daemon/desktopd/screencaststreambackend.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/screencast_revoke_e2e_test.cpp
)

target_include_directories(screencast_revoke_e2e_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(screencast_revoke_e2e_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)
if(SLM_HAVE_PIPEWIRE)
    target_compile_definitions(screencast_revoke_e2e_test PRIVATE SLM_HAVE_PIPEWIRE=1)
    target_link_libraries(screencast_revoke_e2e_test PRIVATE PkgConfig::PIPEWIRE PkgConfig::SPA)
endif()

add_test(NAME screencast_revoke_e2e_test COMMAND screencast_revoke_e2e_test)

qt_add_executable(screencastprivacymodel_dbus_test
    src/services/portal/screencastprivacymodel.cpp
    src/services/portal/screencastprivacymodel.h
    tests/screencastprivacymodel_dbus_test.cpp
)

target_link_libraries(screencastprivacymodel_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME screencastprivacymodel_dbus_test COMMAND screencastprivacymodel_dbus_test)

qt_add_executable(implportal_payload_snapshot_test
    src/daemon/portald/portalmanager.cpp
    src/daemon/portald/portalmanager.h
    src/daemon/portald/implportalservice.cpp
    src/daemon/portald/implportalservice.h
    src/daemon/portald/implportaladaptors.cpp
    src/daemon/portald/implportaladaptors.h
    src/daemon/portald/portal-adapter/PortalAppContextResolver.cpp
    src/daemon/portald/portal-adapter/PortalAppContextResolver.h
    src/core/permissions/CallerIdentity.cpp
    src/core/permissions/CallerIdentity.h
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/implportal_payload_snapshot_test.cpp
)

target_include_directories(implportal_payload_snapshot_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(implportal_payload_snapshot_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME implportal_payload_snapshot_test COMMAND implportal_payload_snapshot_test)

qt_add_executable(screenshotsavehelper_test
    screenshotsavehelper.cpp
    screenshotsavehelper.h
    tests/screenshotsavehelper_test.cpp
)

target_link_libraries(screenshotsavehelper_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME screenshotsavehelper_test COMMAND screenshotsavehelper_test)

qt_add_executable(printsettingsmodel_test
    src/printing/core/PrintSettingsModel.cpp
    src/printing/core/PrintSettingsModel.h
    src/printing/core/PrintTicketSerializer.cpp
    src/printing/core/PrintTicketSerializer.h
    src/printing/core/PrintTypes.h
    src/printing/persistence/PrinterSettingsStore.cpp
    src/printing/persistence/PrinterSettingsStore.h
    tests/printsettingsmodel_test.cpp
)

target_link_libraries(printsettingsmodel_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME printsettingsmodel_test COMMAND $<TARGET_FILE:printsettingsmodel_test>)

qt_add_executable(envvalidator_test
    src/apps/settings/modules/developer/envvalidator.cpp
    src/apps/settings/modules/developer/envvalidator.h
    tests/envvalidator_test.cpp
)
target_link_libraries(envvalidator_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME envvalidator_test COMMAND $<TARGET_FILE:envvalidator_test>)

qt_add_executable(localenvstore_test
    src/apps/settings/modules/developer/enventry.h
    src/apps/settings/modules/developer/localenvstore.cpp
    src/apps/settings/modules/developer/localenvstore.h
    tests/localenvstore_test.cpp
)
target_link_libraries(localenvstore_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME localenvstore_test COMMAND $<TARGET_FILE:localenvstore_test>)

qt_add_executable(envvariablecontroller_test
    src/apps/settings/modules/developer/enventry.h
    src/apps/settings/modules/developer/envvalidator.cpp
    src/apps/settings/modules/developer/envvalidator.h
    src/apps/settings/modules/developer/localenvstore.cpp
    src/apps/settings/modules/developer/localenvstore.h
    src/apps/settings/modules/developer/envvariablemodel.cpp
    src/apps/settings/modules/developer/envvariablemodel.h
    src/apps/settings/modules/developer/envvariablecontroller.cpp
    src/apps/settings/modules/developer/envvariablecontroller.h
    tests/envvariablecontroller_test.cpp
)
target_link_libraries(envvariablecontroller_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME envvariablecontroller_test COMMAND $<TARGET_FILE:envvariablecontroller_test>)

qt_add_executable(mergeresolver_test
    src/apps/settings/modules/developer/enventry.h
    src/apps/settings/modules/developer/localenvstore.cpp
    src/apps/settings/modules/developer/localenvstore.h
    src/services/envd/envtypes.h
    src/services/envd/mergeresolver.cpp
    src/services/envd/mergeresolver.h
    tests/mergeresolver_test.cpp
)
target_link_libraries(mergeresolver_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME mergeresolver_test COMMAND $<TARGET_FILE:mergeresolver_test>)

qt_add_executable(launchenvresolver_test
    src/core/launcher/launchenvresolver.cpp
    src/core/launcher/launchenvresolver.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
    src/apps/settings/modules/developer/enventry.h
    tests/launchenvresolver_test.cpp
)
target_link_libraries(launchenvresolver_test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_test(NAME launchenvresolver_test COMMAND $<TARGET_FILE:launchenvresolver_test>)

qt_add_executable(printermanager_test
    src/printing/core/PrintTypes.h
    src/printing/core/PrinterCapabilityProvider.cpp
    src/printing/core/PrinterCapabilityProvider.h
    src/printing/core/PrinterManager.cpp
    src/printing/core/PrinterManager.h
    tests/printermanager_test.cpp
)

target_link_libraries(printermanager_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME printermanager_test COMMAND $<TARGET_FILE:printermanager_test>)

qt_add_executable(printpreviewmodel_test
    src/printing/core/PrintPreviewModel.cpp
    src/printing/core/PrintPreviewModel.h
    src/printing/preview/IPdfRenderer.h
    src/printing/preview/FallbackPdfRenderer.h
    src/printing/preview/FallbackPdfRenderer.cpp
    src/printing/preview/PopplerPdfRenderer.h
    src/printing/preview/PopplerPdfRenderer.cpp
    src/printing/preview/PrintPreviewCache.h
    src/printing/preview/PrintPreviewCache.cpp
    tests/printpreviewmodel_test.cpp
)

target_include_directories(printpreviewmodel_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/printing
)

target_link_libraries(printpreviewmodel_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
if(TARGET Poppler::Qt6)
    target_link_libraries(printpreviewmodel_test PRIVATE Poppler::Qt6)
    target_compile_definitions(printpreviewmodel_test PRIVATE SLM_HAVE_POPPLER_QT6)
endif()

add_test(NAME printpreviewmodel_test COMMAND $<TARGET_FILE:printpreviewmodel_test>)

qt_add_executable(printjobbuilder_test
    src/printing/core/PrintTypes.h
    src/printing/core/PrintTicketSerializer.cpp
    src/printing/core/PrintTicketSerializer.h
    src/printing/core/PrintSettingsModel.cpp
    src/printing/core/PrintSettingsModel.h
    src/printing/core/PrintSession.cpp
    src/printing/core/PrintSession.h
    src/printing/core/PrintJobBuilder.cpp
    src/printing/core/PrintJobBuilder.h
    src/printing/core/JobSubmitter.cpp
    src/printing/core/JobSubmitter.h
    tests/printjobbuilder_test.cpp
)

target_link_libraries(printjobbuilder_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME printjobbuilder_test COMMAND $<TARGET_FILE:printjobbuilder_test>)

qt_add_executable(portalmanager_filechooser_contract_test
    src/daemon/portald/portalmanager.cpp
    src/daemon/portald/portalmanager.h
    tests/portalmanager_filechooser_contract_test.cpp
)

target_include_directories(portalmanager_filechooser_contract_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(portalmanager_filechooser_contract_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME portalmanager_filechooser_contract_test COMMAND portalmanager_filechooser_contract_test)

qt_add_executable(portalmanager_openuri_contract_test
    src/daemon/portald/portalmanager.cpp
    src/daemon/portald/portalmanager.h
    tests/portalmanager_openuri_contract_test.cpp
)

target_include_directories(portalmanager_openuri_contract_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(portalmanager_openuri_contract_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME portalmanager_openuri_contract_test COMMAND portalmanager_openuri_contract_test)

qt_add_executable(portalmanager_visual_contract_test
    src/daemon/portald/portalmanager.cpp
    src/daemon/portald/portalmanager.h
    tests/portalmanager_visual_contract_test.cpp
)

target_include_directories(portalmanager_visual_contract_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(portalmanager_visual_contract_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME portalmanager_visual_contract_test COMMAND portalmanager_visual_contract_test)

qt_add_executable(portalmanager_capture_contract_test
    src/daemon/portald/portalmanager.cpp
    src/daemon/portald/portalmanager.h
    tests/portalmanager_capture_contract_test.cpp
)

target_link_libraries(portalmanager_capture_contract_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME portalmanager_capture_contract_test COMMAND portalmanager_capture_contract_test)

qt_add_executable(portalchooserlogichelper_test
    portalchooserlogichelper.cpp
    portalchooserlogichelper.h
    tests/portalchooserlogichelper_test.cpp
)

target_link_libraries(portalchooserlogichelper_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME portalchooserlogichelper_test COMMAND portalchooserlogichelper_test)

qt_add_executable(appstartupargs_test
    src/bootstrap/appstartupargs.cpp
    src/bootstrap/appstartupargs.h
    tests/appstartupargs_test.cpp
)

target_link_libraries(appstartupargs_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME appstartupargs_test COMMAND appstartupargs_test)

qt_add_executable(daemonservicebootstrap_test
    src/bootstrap/daemonservicebootstrap.cpp
    src/bootstrap/daemonservicebootstrap.h
    tests/daemonservicebootstrap_test.cpp
)

target_link_libraries(daemonservicebootstrap_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME daemonservicebootstrap_test COMMAND daemonservicebootstrap_test)

qt_add_executable(daemonservicebootstraprunner_test
    src/bootstrap/daemonservicebootstrap.cpp
    src/bootstrap/daemonservicebootstrap.h
    src/bootstrap/daemonservicebootstraprunner.cpp
    src/bootstrap/daemonservicebootstraprunner.h
    tests/daemonservicebootstraprunner_test.cpp
)

target_link_libraries(daemonservicebootstraprunner_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME daemonservicebootstraprunner_test COMMAND daemonservicebootstraprunner_test)

qt_add_executable(topbar_popups_smoke_test
    tests/topbar_popups_smoke_test.cpp
)

if (DESKTOP_SHELL_QML_JS_FILES)
    qt6_add_resources(topbar_popups_smoke_test topbar_smoke_qml_js_raw
        PREFIX "/qt/qml/Slm_Desktop"
        FILES ${DESKTOP_SHELL_QML_JS_FILES}
    )
endif()
qt6_add_resources(topbar_popups_smoke_test topbar_smoke_qml_raw
    PREFIX "/qt/qml/Slm_Desktop"
    FILES ${DESKTOP_SHELL_QML_FILES}
)

target_compile_definitions(topbar_popups_smoke_test
    PRIVATE
        BUILD_QML_IMPORT_ROOT=\"${CMAKE_CURRENT_BINARY_DIR}\"
        TOPBAR_QML_COMPONENT_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/Qml/components/topbar\"
)

target_link_libraries(topbar_popups_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)

add_test(NAME topbar_popups_smoke_test COMMAND topbar_popups_smoke_test)
set_tests_properties(topbar_popups_smoke_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(filemanager_dialogs_smoke_test
    tests/filemanager/filemanager_dialogs_smoke_test.cpp
)

if (DESKTOP_SHELL_QML_JS_FILES)
    qt6_add_resources(filemanager_dialogs_smoke_test filemanager_smoke_qml_js_raw
        PREFIX "/qt/qml/Slm_Desktop"
        FILES ${DESKTOP_SHELL_QML_JS_FILES}
    )
endif()
qt6_add_resources(filemanager_dialogs_smoke_test filemanager_smoke_qml_raw
    PREFIX "/qt/qml/Slm_Desktop"
    FILES ${DESKTOP_SHELL_QML_FILES}
)

target_compile_definitions(filemanager_dialogs_smoke_test
    PRIVATE
        BUILD_QML_IMPORT_ROOT=\"${CMAKE_CURRENT_BINARY_DIR}\"
        FILEMANAGER_QML_COMPONENT_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/Qml/apps/filemanager\"
)

target_link_libraries(filemanager_dialogs_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Test
)

add_test(NAME filemanager_dialogs_smoke_test COMMAND filemanager_dialogs_smoke_test)
set_tests_properties(filemanager_dialogs_smoke_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Convenience aggregate target for the FileManager split profile.
# Keeps extraction validation simple before moving to standalone repository.
set(_SLM_FILEMANAGER_TEST_SUITE_DEPS)
foreach(_fm_test_target
    fileoperationsmanager_test
    fileoperationsservice_dbus_test
    fileopctl_smoke_test
    filemanagerapi_daemon_recovery_test
    filemanagerapi_fileops_contract_test
    filemanager_dialogs_smoke_test
)
    if (TARGET ${_fm_test_target})
        list(APPEND _SLM_FILEMANAGER_TEST_SUITE_DEPS ${_fm_test_target})
    endif()
endforeach()

if (_SLM_FILEMANAGER_TEST_SUITE_DEPS)
    add_custom_target(filemanager_test_suite
        DEPENDS ${_SLM_FILEMANAGER_TEST_SUITE_DEPS}
    )
endif()

qt_add_executable(workspacemanager_dbus_test
    appmodel.cpp
    appmodel.h
    globalsearchservice.cpp
    globalsearchservice.h
    ${SLM_ACTION_TEST_SOURCES}
    ${SLM_PERMISSION_TEST_SOURCES}
    src/core/workspace/workspacemanager.cpp
    src/core/workspace/workspacemanager.h
    src/daemon/desktopd/daemonhealthmonitor.cpp
    src/daemon/desktopd/daemonhealthmonitor.h
    src/daemon/desktopd/desktopdaemonservice.cpp
    src/daemon/desktopd/desktopdaemonservice.h
    src/core/workspace/spacesmanager.cpp
    src/core/workspace/spacesmanager.h
    src/services/clipboard/ClipboardSearchProvider.cpp
    src/services/clipboard/ClipboardSearchProvider.h
    src/services/clipboard/contentclassifier.cpp
    src/services/clipboard/contentclassifier.h
    src/services/clipboard/clipboardtypes.h
    src/core/workspace/windowingbackendmanager.cpp
    src/core/workspace/windowingbackendmanager.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    tests/workspacemanager_dbus_test.cpp
)

target_include_directories(workspacemanager_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(workspacemanager_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Network
        Qt6::Concurrent
        Qt6::Test
        Qt6::Sql
        PkgConfig::GIO
)

add_test(NAME workspacemanager_dbus_test
    COMMAND ${CMAKE_COMMAND} -E env
        QT_QPA_PLATFORM=offscreen
        ${CMAKE_SOURCE_DIR}/scripts/run-dbus-test.sh
        $<TARGET_FILE:workspacemanager_dbus_test>
)

qt_add_executable(daemonhealthmonitor_test
    src/daemon/desktopd/daemonhealthmonitor.cpp
    src/daemon/desktopd/daemonhealthmonitor.h
    tests/daemonhealthmonitor_test.cpp
)

target_link_libraries(daemonhealthmonitor_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME daemonhealthmonitor_test COMMAND daemonhealthmonitor_test)

qt_add_executable(globalsearchservice_dbus_test
    appmodel.cpp
    appmodel.h
    globalsearchservice.cpp
    globalsearchservice.h
    ${SLM_ACTION_TEST_SOURCES}
    ${SLM_PERMISSION_TEST_SOURCES}
    src/core/workspace/spacesmanager.cpp
    src/core/workspace/spacesmanager.h
    src/core/workspace/windowingbackendmanager.cpp
    src/core/workspace/windowingbackendmanager.h
    src/core/workspace/kwinwaylandipcclient.cpp
    src/core/workspace/kwinwaylandipcclient.h
    src/core/workspace/kwinwaylandstatemodel.cpp
    src/core/workspace/kwinwaylandstatemodel.h
    src/core/workspace/kwinsupportinfoparser.cpp
    src/core/workspace/kwinsupportinfoparser.h
    src/core/workspace/workspacemanager.cpp
    src/core/workspace/workspacemanager.h
    src/services/clipboard/ClipboardSearchProvider.cpp
    src/services/clipboard/ClipboardSearchProvider.h
    src/services/clipboard/contentclassifier.cpp
    src/services/clipboard/contentclassifier.h
    src/services/clipboard/clipboardtypes.h
    tests/globalsearchservice_dbus_test.cpp
)

target_include_directories(globalsearchservice_dbus_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(globalsearchservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Network
        Qt6::Concurrent
        Qt6::Test
        Qt6::Sql
        PkgConfig::GIO
)

add_test(NAME globalsearchservice_dbus_test
    COMMAND ${CMAKE_COMMAND} -E env
        QT_QPA_PLATFORM=offscreen
        ${CMAKE_SOURCE_DIR}/scripts/run-dbus-test.sh
        $<TARGET_FILE:globalsearchservice_dbus_test>
)

qt_add_executable(clipboardsearchprovider_test
    src/services/clipboard/ClipboardSearchProvider.cpp
    src/services/clipboard/ClipboardSearchProvider.h
    src/services/clipboard/contentclassifier.cpp
    src/services/clipboard/contentclassifier.h
    src/services/clipboard/clipboardtypes.h
    tests/clipboardsearchprovider_test.cpp
)

target_link_libraries(clipboardsearchprovider_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME clipboardsearchprovider_test
    COMMAND ${CMAKE_COMMAND} -E env
        QT_QPA_PLATFORM=offscreen
        ${CMAKE_SOURCE_DIR}/scripts/run-dbus-test.sh
        $<TARGET_FILE:clipboardsearchprovider_test>
)

qt_add_executable(clipboardwatcher_backend_test
    tests/clipboardwatcher_backend_test.cpp
)

target_include_directories(clipboardwatcher_backend_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(clipboardwatcher_backend_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME clipboardwatcher_backend_test
    COMMAND ${CMAKE_COMMAND} -E env
        QT_QPA_PLATFORM=offscreen
        $<TARGET_FILE:clipboardwatcher_backend_test>
)

qt_add_executable(dbussecurityguard_test
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/dbussecurityguard_test.cpp
)

target_link_libraries(dbussecurityguard_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME dbussecurityguard_test COMMAND dbussecurityguard_test)

qt_add_executable(policyengine_test
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/policyengine_test.cpp
)

target_link_libraries(policyengine_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME policyengine_test COMMAND policyengine_test)

qt_add_executable(permissionstore_payload_contract_test
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/permissionstore_payload_contract_test.cpp
)
 
target_include_directories(permissionstore_payload_contract_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(permissionstore_payload_contract_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME permissionstore_payload_contract_test COMMAND permissionstore_payload_contract_test)

qt_add_executable(trustresolver_test
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/trustresolver_test.cpp
)
target_link_libraries(trustresolver_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)
add_test(NAME trustresolver_test COMMAND trustresolver_test)

qt_add_executable(policy_integration_test
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/policy_integration_test.cpp
)
target_link_libraries(policy_integration_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)
add_test(NAME policy_integration_test COMMAND policy_integration_test)

qt_add_executable(portal_adapter_lifecycle_persistence_test
    ${SLM_PERMISSION_TEST_SOURCES}
    src/daemon/portald/portal-adapter/PortalCapabilityMapper.cpp
    src/daemon/portald/portal-adapter/PortalCapabilityMapper.h
    src/daemon/portald/portal-adapter/PortalDialogBridge.cpp
    src/daemon/portald/portal-adapter/PortalDialogBridge.h
    src/daemon/portald/portal-adapter/PortalAccessMediator.cpp
    src/daemon/portald/portal-adapter/PortalAccessMediator.h
    src/daemon/portald/portal-adapter/PortalResponseSerializer.cpp
    src/daemon/portald/portal-adapter/PortalResponseSerializer.h
    src/daemon/portald/portal-adapter/PortalRequestObject.cpp
    src/daemon/portald/portal-adapter/PortalRequestObject.h
    src/daemon/portald/portal-adapter/PortalRequestManager.cpp
    src/daemon/portald/portal-adapter/PortalRequestManager.h
    src/daemon/portald/portal-adapter/PortalSessionObject.cpp
    src/daemon/portald/portal-adapter/PortalSessionObject.h
    src/daemon/portald/portal-adapter/PortalSessionManager.cpp
    src/daemon/portald/portal-adapter/PortalSessionManager.h
    src/daemon/portald/portal-adapter/PortalPermissionStoreAdapter.cpp
    src/daemon/portald/portal-adapter/PortalPermissionStoreAdapter.h
    tests/portal_adapter_lifecycle_persistence_test.cpp
)
 
target_include_directories(portal_adapter_lifecycle_persistence_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(portal_adapter_lifecycle_persistence_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME portal_adapter_lifecycle_persistence_test
    COMMAND portal_adapter_lifecycle_persistence_test)

qt_add_executable(portal_adapter_race_cancellation_test
    ${SLM_PERMISSION_TEST_SOURCES}
    src/daemon/portald/portal-adapter/PortalCapabilityMapper.cpp
    src/daemon/portald/portal-adapter/PortalCapabilityMapper.h
    src/daemon/portald/portal-adapter/PortalDialogBridge.cpp
    src/daemon/portald/portal-adapter/PortalDialogBridge.h
    src/daemon/portald/portal-adapter/PortalAccessMediator.cpp
    src/daemon/portald/portal-adapter/PortalAccessMediator.h
    src/daemon/portald/portal-adapter/PortalResponseSerializer.cpp
    src/daemon/portald/portal-adapter/PortalResponseSerializer.h
    src/daemon/portald/portal-adapter/PortalRequestObject.cpp
    src/daemon/portald/portal-adapter/PortalRequestObject.h
    src/daemon/portald/portal-adapter/PortalRequestManager.cpp
    src/daemon/portald/portal-adapter/PortalRequestManager.h
    src/daemon/portald/portal-adapter/PortalSessionObject.cpp
    src/daemon/portald/portal-adapter/PortalSessionObject.h
    src/daemon/portald/portal-adapter/PortalSessionManager.cpp
    src/daemon/portald/portal-adapter/PortalSessionManager.h
    src/daemon/portald/portal-adapter/PortalPermissionStoreAdapter.cpp
    src/daemon/portald/portal-adapter/PortalPermissionStoreAdapter.h
    tests/portal_adapter_race_cancellation_test.cpp
)

target_include_directories(portal_adapter_race_cancellation_test PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(portal_adapter_race_cancellation_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
)

add_test(NAME portal_adapter_race_cancellation_test
    COMMAND portal_adapter_race_cancellation_test)

qt_add_executable(portalresponseserializer_contract_test
    src/daemon/portald/portal-adapter/PortalResponseSerializer.cpp
    src/daemon/portald/portal-adapter/PortalResponseSerializer.h
    tests/portalresponseserializer_contract_test.cpp
)

target_link_libraries(portalresponseserializer_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME portalresponseserializer_contract_test
    COMMAND portalresponseserializer_contract_test)

qt_add_executable(portal_app_context_resolver_test
    src/daemon/portald/portal-adapter/PortalAppContextResolver.cpp
    src/daemon/portald/portal-adapter/PortalAppContextResolver.h
    src/core/permissions/CallerIdentity.cpp
    src/core/permissions/CallerIdentity.h
    tests/portal_app_context_resolver_test.cpp
)

target_link_libraries(portal_app_context_resolver_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)
target_include_directories(portal_app_context_resolver_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME portal_app_context_resolver_test
    COMMAND portal_app_context_resolver_test)

qt_add_executable(slmcapabilityservice_dbus_test
    src/daemon/desktopd/slmcapabilityservice.cpp
    src/daemon/desktopd/slmcapabilityservice.h
    ${SLM_ACTION_TEST_SOURCES}
    ${SLM_ACTION_FRAMEWORK_TEST_SOURCES}
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/slmcapabilityservice_dbus_test.cpp
)

target_link_libraries(slmcapabilityservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME slmcapabilityservice_dbus_test
    COMMAND ${CMAKE_COMMAND} -E env
        QT_QPA_PLATFORM=offscreen
        ${CMAKE_SOURCE_DIR}/scripts/run-dbus-test.sh
        $<TARGET_FILE:slmcapabilityservice_dbus_test>
)

qt_add_executable(tothespot_contextmenuhelper_test
    tothespotcontextmenuhelper.cpp
    tothespotcontextmenuhelper.h
    tests/tothespot_contextmenuhelper_test.cpp
)

target_link_libraries(tothespot_contextmenuhelper_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME tothespot_contextmenuhelper_test COMMAND tothespot_contextmenuhelper_test)

qt_add_executable(tothespot_texthighlighter_test
    tothespottexthighlighter.cpp
    tothespottexthighlighter.h
    tests/tothespot_texthighlighter_test.cpp
)

target_link_libraries(tothespot_texthighlighter_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME tothespot_texthighlighter_test COMMAND tothespot_texthighlighter_test)

qt_add_executable(tothespotservice_dbus_test
    tothespotservice.cpp
    tothespotservice.h
    resourcepaths.h
    ${SLM_ACTION_TEST_SOURCES}
    ${SLM_ACTION_FRAMEWORK_TEST_SOURCES}
    ${SLM_PERMISSION_TEST_SOURCES}
    tests/tothespotservice_dbus_test.cpp
)

target_link_libraries(tothespotservice_dbus_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Sql
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME tothespotservice_dbus_test
    COMMAND ${CMAKE_COMMAND} -E env
        QT_QPA_PLATFORM=offscreen
        $<TARGET_FILE:tothespotservice_dbus_test>
)

qt_add_executable(settingsapp_deeplink_anchor_test
    src/apps/settings/settingsapp.cpp
    src/apps/settings/settingsapp.h
    src/apps/settings/moduleloader.cpp
    src/apps/settings/moduleloader.h
    src/apps/settings/searchengine.cpp
    src/apps/settings/searchengine.h
    src/apps/settings/include/settingbinding.cpp
    src/apps/settings/include/settingbinding.h
    src/apps/settings/include/mockbinding.cpp
    src/apps/settings/include/mockbinding.h
    src/apps/settings/include/gsettingsbinding.cpp
    src/apps/settings/include/gsettingsbinding.h
    src/apps/settings/include/networkmanagerbinding.cpp
    src/apps/settings/include/networkmanagerbinding.h
    src/apps/settings/include/dbusbinding.cpp
    src/apps/settings/include/dbusbinding.h
    src/apps/settings/include/localsettingsbinding.cpp
    src/apps/settings/include/localsettingsbinding.h
    src/apps/settings/include/settingbindingresolver.cpp
    src/apps/settings/include/settingbindingresolver.h
    src/apps/settings/include/settingbindingfactory.cpp
    src/apps/settings/include/settingbindingfactory.h
    src/apps/settings/include/settingspolkitbridge.cpp
    src/apps/settings/include/settingspolkitbridge.h
    tests/settingsapp_deeplink_anchor_test.cpp
)

target_include_directories(settingsapp_deeplink_anchor_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/settings
    ${CMAKE_SOURCE_DIR}/src/apps/settings/include
)

target_link_libraries(settingsapp_deeplink_anchor_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME settingsapp_deeplink_anchor_test COMMAND settingsapp_deeplink_anchor_test)

qt_add_executable(settingsapp_policy_test
    src/apps/settings/settingsapp.cpp
    src/apps/settings/settingsapp.h
    src/apps/settings/moduleloader.cpp
    src/apps/settings/moduleloader.h
    src/apps/settings/searchengine.cpp
    src/apps/settings/searchengine.h
    src/apps/settings/include/settingbinding.cpp
    src/apps/settings/include/settingbinding.h
    src/apps/settings/include/mockbinding.cpp
    src/apps/settings/include/mockbinding.h
    src/apps/settings/include/gsettingsbinding.cpp
    src/apps/settings/include/gsettingsbinding.h
    src/apps/settings/include/networkmanagerbinding.cpp
    src/apps/settings/include/networkmanagerbinding.h
    src/apps/settings/include/dbusbinding.cpp
    src/apps/settings/include/dbusbinding.h
    src/apps/settings/include/localsettingsbinding.cpp
    src/apps/settings/include/localsettingsbinding.h
    src/apps/settings/include/settingbindingresolver.cpp
    src/apps/settings/include/settingbindingresolver.h
    src/apps/settings/include/settingbindingfactory.cpp
    src/apps/settings/include/settingbindingfactory.h
    src/apps/settings/include/settingspolkitbridge.cpp
    src/apps/settings/include/settingspolkitbridge.h
    tests/settingsapp_policy_test.cpp
)

target_include_directories(settingsapp_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/settings
    ${CMAKE_SOURCE_DIR}/src/apps/settings/include
)

target_link_libraries(settingsapp_policy_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME settingsapp_policy_test COMMAND settingsapp_policy_test)
set_tests_properties(settingsapp_policy_test PROPERTIES
    LABELS "baseline-flaky"
)

qt_add_executable(settingsapp_policy_core_test
    src/apps/settings/settingsapp.cpp
    src/apps/settings/settingsapp.h
    src/apps/settings/moduleloader.cpp
    src/apps/settings/moduleloader.h
    src/apps/settings/searchengine.cpp
    src/apps/settings/searchengine.h
    src/apps/settings/include/settingbinding.cpp
    src/apps/settings/include/settingbinding.h
    src/apps/settings/include/mockbinding.cpp
    src/apps/settings/include/mockbinding.h
    src/apps/settings/include/gsettingsbinding.cpp
    src/apps/settings/include/gsettingsbinding.h
    src/apps/settings/include/networkmanagerbinding.cpp
    src/apps/settings/include/networkmanagerbinding.h
    src/apps/settings/include/dbusbinding.cpp
    src/apps/settings/include/dbusbinding.h
    src/apps/settings/include/localsettingsbinding.cpp
    src/apps/settings/include/localsettingsbinding.h
    src/apps/settings/include/settingbindingresolver.cpp
    src/apps/settings/include/settingbindingresolver.h
    src/apps/settings/include/settingbindingfactory.cpp
    src/apps/settings/include/settingbindingfactory.h
    src/apps/settings/include/settingspolkitbridge.cpp
    src/apps/settings/include/settingspolkitbridge.h
    tests/settingsapp_policy_core_test.cpp
)

target_include_directories(settingsapp_policy_core_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/settings
    ${CMAKE_SOURCE_DIR}/src/apps/settings/include
)

target_link_libraries(settingsapp_policy_core_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME settingsapp_policy_core_test COMMAND settingsapp_policy_core_test)
set_tests_properties(settingsapp_policy_core_test PROPERTIES
    LABELS "policy-core"
)
set(_SLM_POLICY_CORE_TEST_SUITE_DEPS
    settingsapp_policy_core_test
)
add_custom_target(policy_core_test_suite
    DEPENDS ${_SLM_POLICY_CORE_TEST_SUITE_DEPS}
)

qt_add_executable(settingsapp_secret_consent_test
    src/apps/settings/settingsapp.cpp
    src/apps/settings/settingsapp.h
    src/apps/settings/moduleloader.cpp
    src/apps/settings/moduleloader.h
    src/apps/settings/searchengine.cpp
    src/apps/settings/searchengine.h
    src/apps/settings/include/settingbinding.cpp
    src/apps/settings/include/settingbinding.h
    src/apps/settings/include/mockbinding.cpp
    src/apps/settings/include/mockbinding.h
    src/apps/settings/include/gsettingsbinding.cpp
    src/apps/settings/include/gsettingsbinding.h
    src/apps/settings/include/networkmanagerbinding.cpp
    src/apps/settings/include/networkmanagerbinding.h
    src/apps/settings/include/dbusbinding.cpp
    src/apps/settings/include/dbusbinding.h
    src/apps/settings/include/localsettingsbinding.cpp
    src/apps/settings/include/localsettingsbinding.h
    src/apps/settings/include/settingbindingresolver.cpp
    src/apps/settings/include/settingbindingresolver.h
    src/apps/settings/include/settingbindingfactory.cpp
    src/apps/settings/include/settingbindingfactory.h
    src/apps/settings/include/settingspolkitbridge.cpp
    src/apps/settings/include/settingspolkitbridge.h
    tests/settingsapp_secret_consent_test.cpp
)

target_include_directories(settingsapp_secret_consent_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/settings
    ${CMAKE_SOURCE_DIR}/src/apps/settings/include
)

target_link_libraries(settingsapp_secret_consent_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME settingsapp_secret_consent_test COMMAND settingsapp_secret_consent_test)
set_tests_properties(settingsapp_secret_consent_test PROPERTIES
    LABELS "secret-consent"
)

qt_add_executable(settings_bindingresolver_test
    src/apps/settings/include/settingbindingresolver.cpp
    src/apps/settings/include/settingbindingresolver.h
    tests/settings_bindingresolver_test.cpp
)

target_include_directories(settings_bindingresolver_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/settings
    ${CMAKE_SOURCE_DIR}/src/apps/settings/include
)

target_link_libraries(settings_bindingresolver_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME settings_bindingresolver_test COMMAND settings_bindingresolver_test)

qt_add_executable(settings_moduleloader_contract_test
    src/apps/settings/moduleloader.cpp
    src/apps/settings/moduleloader.h
    tests/settings_moduleloader_contract_test.cpp
)

target_include_directories(settings_moduleloader_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/settings
)

target_link_libraries(settings_moduleloader_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME settings_moduleloader_contract_test COMMAND settings_moduleloader_contract_test)

qt_add_executable(settings_binding_contract_test
    src/apps/settings/include/settingbinding.cpp
    src/apps/settings/include/settingbinding.h
    src/apps/settings/include/mockbinding.cpp
    src/apps/settings/include/mockbinding.h
    src/apps/settings/include/networkmanagerbinding.cpp
    src/apps/settings/include/networkmanagerbinding.h
    tests/settings_binding_contract_test.cpp
)

target_include_directories(settings_binding_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/settings
    ${CMAKE_SOURCE_DIR}/src/apps/settings/include
)

target_link_libraries(settings_binding_contract_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME settings_binding_contract_test COMMAND settings_binding_contract_test)

qt_add_executable(settings_ui_smoke_test
    src/apps/settings/settingsapp.cpp
    src/apps/settings/settingsapp.h
    src/apps/settings/moduleloader.cpp
    src/apps/settings/moduleloader.h
    src/apps/settings/searchengine.cpp
    src/apps/settings/searchengine.h
    src/apps/settings/include/settingbinding.cpp
    src/apps/settings/include/settingbinding.h
    src/apps/settings/include/mockbinding.cpp
    src/apps/settings/include/mockbinding.h
    src/apps/settings/include/gsettingsbinding.cpp
    src/apps/settings/include/gsettingsbinding.h
    src/apps/settings/include/networkmanagerbinding.cpp
    src/apps/settings/include/networkmanagerbinding.h
    src/apps/settings/include/dbusbinding.cpp
    src/apps/settings/include/dbusbinding.h
    src/apps/settings/include/localsettingsbinding.cpp
    src/apps/settings/include/localsettingsbinding.h
    src/apps/settings/include/settingbindingresolver.cpp
    src/apps/settings/include/settingbindingresolver.h
    src/apps/settings/include/settingbindingfactory.cpp
    src/apps/settings/include/settingbindingfactory.h
    src/apps/settings/include/settingspolkitbridge.cpp
    src/apps/settings/include/settingspolkitbridge.h
    tests/settings_ui_smoke_test.cpp
)

target_include_directories(settings_ui_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/settings
    ${CMAKE_SOURCE_DIR}/src/apps/settings/include
)

target_compile_definitions(settings_ui_smoke_test PRIVATE
    BUILD_QML_IMPORT_ROOT=\"${CMAKE_CURRENT_BINARY_DIR}\"
    SETTINGS_QML_MAIN_FILE=\"${CMAKE_SOURCE_DIR}/Qml/apps/settings/Main.qml\"
    SETTINGS_MODULES_DIR=\"${CMAKE_SOURCE_DIR}/src/apps/settings/modules\"
)

target_link_libraries(settings_ui_smoke_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME settings_ui_smoke_test COMMAND settings_ui_smoke_test)

qt_add_executable(settings_missing_components_ui_test
    src/apps/settings/modules/developer/componenthealthcontroller.cpp
    src/apps/settings/modules/developer/componenthealthcontroller.h
    src/core/system/missingcomponentcontroller.cpp
    src/core/system/missingcomponentcontroller.h
    src/core/system/componentregistry.cpp
    src/core/system/componentregistry.h
    src/core/system/dependencyguard.cpp
    src/core/system/dependencyguard.h
    tests/settings_missing_components_ui_test.cpp
)

target_include_directories(settings_missing_components_ui_test PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(settings_missing_components_ui_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Qt6::Test
)

add_test(NAME settings_missing_components_ui_test COMMAND settings_missing_components_ui_test)
set_tests_properties(settings_missing_components_ui_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

qt_add_executable(settings_backend_fallback_test
    src/apps/settings/include/settingbinding.cpp
    src/apps/settings/include/settingbinding.h
    src/apps/settings/include/mockbinding.cpp
    src/apps/settings/include/mockbinding.h
    src/apps/settings/include/gsettingsbinding.cpp
    src/apps/settings/include/gsettingsbinding.h
    src/apps/settings/include/dbusbinding.cpp
    src/apps/settings/include/dbusbinding.h
    src/apps/settings/include/localsettingsbinding.cpp
    src/apps/settings/include/localsettingsbinding.h
    src/apps/settings/include/settingbindingresolver.cpp
    src/apps/settings/include/settingbindingresolver.h
    src/apps/settings/include/settingbindingfactory.cpp
    src/apps/settings/include/settingbindingfactory.h
    src/apps/settings/include/networkmanagerbinding.cpp
    src/apps/settings/include/networkmanagerbinding.h
    tests/settings_backend_fallback_test.cpp
)

target_include_directories(settings_backend_fallback_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/apps/settings
    ${CMAKE_SOURCE_DIR}/src/apps/settings/include
)

target_link_libraries(settings_backend_fallback_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME settings_backend_fallback_test COMMAND settings_backend_fallback_test)

qt_add_executable(portal_portals_conf_strategy_test
    tests/portal_portals_conf_strategy_test.cpp
)

target_compile_definitions(portal_portals_conf_strategy_test PRIVATE
    PORTAL_TEMPLATES_DIR=\"${CMAKE_SOURCE_DIR}/scripts/xdg-desktop-portal\"
)

target_link_libraries(portal_portals_conf_strategy_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME portal_portals_conf_strategy_test COMMAND portal_portals_conf_strategy_test)

qt_add_executable(portal_install_script_integration_test
    tests/portal_install_script_integration_test.cpp
)

target_compile_definitions(portal_install_script_integration_test PRIVATE
    SLM_INSTALL_PORTAL_SCRIPT=\"${CMAKE_SOURCE_DIR}/scripts/install-portal-user-service.sh\"
)

target_link_libraries(portal_install_script_integration_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME portal_install_script_integration_test COMMAND portal_install_script_integration_test)

qt_add_executable(portal_profile_snapshot_test
    tests/portal_profile_snapshot_test.cpp
)

target_compile_definitions(portal_profile_snapshot_test PRIVATE
    PORTAL_TEMPLATES_DIR=\"${CMAKE_SOURCE_DIR}/scripts/xdg-desktop-portal\"
)

target_link_libraries(portal_profile_snapshot_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME portal_profile_snapshot_test COMMAND portal_profile_snapshot_test)

qt_add_executable(portal_consent_dialog_copy_contract_test
    tests/portal_consent_dialog_copy_contract_test.cpp
)

target_link_libraries(portal_consent_dialog_copy_contract_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

target_compile_definitions(portal_consent_dialog_copy_contract_test PRIVATE
    DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

add_test(NAME portal_consent_dialog_copy_contract_test COMMAND portal_consent_dialog_copy_contract_test)
set_tests_properties(portal_consent_dialog_copy_contract_test PROPERTIES
    LABELS "secret-consent"
)

qt_add_executable(portal_consent_dialog_behavior_test
    tests/portal_consent_dialog_behavior_test.cpp
)

target_link_libraries(portal_consent_dialog_behavior_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)

target_compile_definitions(portal_consent_dialog_behavior_test PRIVATE
    BUILD_QML_IMPORT_ROOT=\"${CMAKE_CURRENT_BINARY_DIR}\"
    DESKTOP_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
)

add_test(NAME portal_consent_dialog_behavior_test COMMAND portal_consent_dialog_behavior_test)
set_tests_properties(portal_consent_dialog_behavior_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_QUICK_CONTROLS_FALLBACK_STYLE=Basic"
    LABELS "secret-consent"
)

qt_add_executable(portal_dialog_bridge_payload_test
    src/core/permissions/AccessContext.cpp
    src/core/permissions/AccessContext.h
    src/core/permissions/Capability.cpp
    src/core/permissions/Capability.h
    src/daemon/portald/portal-adapter/PortalDialogBridge.cpp
    src/daemon/portald/portal-adapter/PortalDialogBridge.h
    tests/portal_dialog_bridge_payload_test.cpp
)

target_link_libraries(portal_dialog_bridge_payload_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Test
)

add_test(NAME portal_dialog_bridge_payload_test COMMAND portal_dialog_bridge_payload_test)
set_tests_properties(portal_dialog_bridge_payload_test PROPERTIES
    LABELS "secret-consent"
)

qt_add_executable(portal_secret_consent_contract_snapshot_test
    tests/portal_secret_consent_contract_snapshot_test.cpp
)

target_compile_definitions(portal_secret_consent_contract_snapshot_test PRIVATE
    CONTRACTS_DIR=\"${CMAKE_SOURCE_DIR}/docs/contracts\"
)

target_link_libraries(portal_secret_consent_contract_snapshot_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME portal_secret_consent_contract_snapshot_test
    COMMAND portal_secret_consent_contract_snapshot_test)
set_tests_properties(portal_secret_consent_contract_snapshot_test PROPERTIES
    LABELS "secret-consent"
)

set(_SLM_SECRET_CONSENT_TEST_SUITE_DEPS
    portal_secret_integration_test
    portal_consent_dialog_copy_contract_test
    portal_consent_dialog_behavior_test
    portal_dialog_bridge_payload_test
    portal_secret_consent_contract_snapshot_test
    settingsapp_secret_consent_test
)
add_custom_target(secret_consent_test_suite
    DEPENDS ${_SLM_SECRET_CONSENT_TEST_SUITE_DEPS}
)

qt_add_executable(portal_slm_portal_snapshot_test
    tests/portal_slm_portal_snapshot_test.cpp
)

target_compile_definitions(portal_slm_portal_snapshot_test PRIVATE
    PORTAL_TEMPLATES_DIR=\"${CMAKE_SOURCE_DIR}/scripts/xdg-desktop-portal\"
)

target_link_libraries(portal_slm_portal_snapshot_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME portal_slm_portal_snapshot_test COMMAND portal_slm_portal_snapshot_test)

qt_add_executable(portal_dbus_service_template_snapshot_test
    tests/portal_dbus_service_template_snapshot_test.cpp
)

target_compile_definitions(portal_dbus_service_template_snapshot_test PRIVATE
    PORTAL_TEMPLATES_DIR=\"${CMAKE_SOURCE_DIR}/scripts/xdg-desktop-portal\"
)

target_link_libraries(portal_dbus_service_template_snapshot_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME portal_dbus_service_template_snapshot_test
         COMMAND portal_dbus_service_template_snapshot_test)

qt_add_executable(portal_systemd_unit_template_snapshot_test
    tests/portal_systemd_unit_template_snapshot_test.cpp
)

target_compile_definitions(portal_systemd_unit_template_snapshot_test PRIVATE
    SYSTEMD_TEMPLATES_DIR=\"${CMAKE_SOURCE_DIR}/scripts/systemd\"
)

target_link_libraries(portal_systemd_unit_template_snapshot_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME portal_systemd_unit_template_snapshot_test
         COMMAND portal_systemd_unit_template_snapshot_test)

add_test(
    NAME portal_profile_smoke_test
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/xdg-desktop-portal/portal-profile-smoke-test.sh
)

add_test(
    NAME portal_installed_artifacts_smoke_test
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/xdg-desktop-portal/portal-installed-artifacts-smoke-test.sh
)

add_test(
    NAME polkit_agent_runtime_smoke_test
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/smoke-polkit-agent-runtime.sh
)
set_tests_properties(polkit_agent_runtime_smoke_test PROPERTIES
    TIMEOUT 20
    RUN_SERIAL TRUE
)

add_test(
    NAME packagepolicy_wrapper_runtime_smoke_test
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/smoke-package-policy-wrapper.sh
)
set_tests_properties(packagepolicy_wrapper_runtime_smoke_test PROPERTIES
    TIMEOUT 20
    RUN_SERIAL TRUE
)

qt_add_executable(slmactionregistry_test
    src/core/actions/slmactiontypes.cpp
    src/core/actions/slmactiontypes.h
    src/core/actions/slmactioncondition.cpp
    src/core/actions/slmactioncondition.h
    src/core/actions/slmdesktopactionparser.cpp
    src/core/actions/slmdesktopactionparser.h
    src/core/actions/slmactiondiscovery.cpp
    src/core/actions/slmactiondiscovery.h
    src/core/actions/slmactionregistry.cpp
    src/core/actions/slmactionregistry.h
    src/core/execution/appexecutiongate.cpp
    src/core/execution/appexecutiongate.h
    src/core/execution/appruntimeregistry.cpp
    src/core/execution/appruntimeregistry.h
    src/core/launcher/applauncher.cpp
    src/core/launcher/applauncher.h
    src/core/launcher/launchenvresolver.cpp
    src/core/launcher/launchenvresolver.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
    dockidentity.cpp
    dockidentity.h
    dockmodel.cpp
    dockmodel.h
    shortcutmodel.cpp
    shortcutmodel.h
    tests/slmactionregistry_test.cpp
)

target_link_libraries(slmactionregistry_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME slmactionregistry_test COMMAND slmactionregistry_test)

qt_add_executable(slmcapability_framework_test
    src/core/actions/slmactiontypes.cpp
    src/core/actions/slmactiontypes.h
    src/core/actions/slmactioncondition.cpp
    src/core/actions/slmactioncondition.h
    src/core/actions/slmdesktopactionparser.cpp
    src/core/actions/slmdesktopactionparser.h
    src/core/actions/slmactiondiscovery.cpp
    src/core/actions/slmactiondiscovery.h
    src/core/actions/slmactionregistry.cpp
    src/core/actions/slmactionregistry.h
    src/core/execution/appexecutiongate.cpp
    src/core/execution/appexecutiongate.h
    src/core/execution/appruntimeregistry.cpp
    src/core/execution/appruntimeregistry.h
    src/core/launcher/applauncher.cpp
    src/core/launcher/applauncher.h
    src/core/launcher/launchenvresolver.cpp
    src/core/launcher/launchenvresolver.h
    src/apps/settings/modules/developer/envserviceclient.cpp
    src/apps/settings/modules/developer/envserviceclient.h
    dockidentity.cpp
    dockidentity.h
    dockmodel.cpp
    dockmodel.h
    shortcutmodel.cpp
    shortcutmodel.h
    src/core/actions/framework/slmframeworktypes.h
    src/core/actions/framework/acl/slmacllexer.cpp
    src/core/actions/framework/acl/slmacllexer.h
    src/core/actions/framework/acl/slmaclparser.cpp
    src/core/actions/framework/acl/slmaclparser.h
    src/core/actions/framework/slmgrouptreebuilder.cpp
    src/core/actions/framework/slmgrouptreebuilder.h
    tests/slmcapability_framework_test.cpp
)

target_link_libraries(slmcapability_framework_test
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Test
        PkgConfig::GIO
)

add_test(NAME slmcapability_framework_test COMMAND slmcapability_framework_test)

qt_add_executable(slmconfigmanager_validation_test
    tests/slmconfigmanager_validation_test.cpp
    src/login/libslmlogin/slmconfigmanager.cpp
)
target_include_directories(slmconfigmanager_validation_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(slmconfigmanager_validation_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
add_test(NAME slmconfigmanager_validation_test COMMAND slmconfigmanager_validation_test)

qt_add_executable(slmsessionstate_io_test
    tests/slmsessionstate_io_test.cpp
    src/login/libslmlogin/slmsessionstate.cpp
)
target_include_directories(slmsessionstate_io_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(slmsessionstate_io_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
add_test(NAME slmsessionstate_io_test COMMAND slmsessionstate_io_test)

qt_add_executable(sessionbroker_recovery_rollback_test
    tests/sessionbroker_recovery_rollback_test.cpp
    src/login/libslmlogin/slmconfigmanager.cpp
    src/login/session-broker/sessionrollback.cpp
)
target_include_directories(sessionbroker_recovery_rollback_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(sessionbroker_recovery_rollback_test
    PRIVATE
        Qt6::Core
        Qt6::Test
)
add_test(NAME sessionbroker_recovery_rollback_test COMMAND sessionbroker_recovery_rollback_test)

add_custom_target(test_headless
    COMMAND ${CMAKE_COMMAND} -E env
        QT_QPA_PLATFORM=offscreen
        ${CMAKE_SOURCE_DIR}/scripts/test.sh
        ${CMAKE_BINARY_DIR}
    DEPENDS
        dock_identity_test
        appcommandrouter_test
        windowingbackendmanager_test
        kwinwaylandstatemodel_test
        kwinsupportinfoparser_test
        fileoperationsmanager_test
        appmodel_xbel_fallback_test
        appmodel_scoring_weights_test
        fileoperationsservice_dbus_test
        fileopctl_smoke_test
        workspacectl_smoke_test
        devicectl_smoke_test
        windowingctl_smoke_test
        globalmenuctl_smoke_test
        globalmenumanager_cache_test
        statusnotifierhost_test
        filemanagerapi_daemon_recovery_test
        filemanager_shell_contract_test
        sessionstateservice_dbus_test
        devicesservice_dbus_test
        portalservice_dbus_test
        implportalservice_dbus_test
        implportal_payload_snapshot_test
        screenshotsavehelper_test
        envvalidator_test
        localenvstore_test
        envvariablecontroller_test
        mergeresolver_test
        launchenvresolver_test
        printsettingsmodel_test
        printermanager_test
        printpreviewmodel_test
        printjobbuilder_test
        portalmanager_filechooser_contract_test
        portalmanager_openuri_contract_test
        portalmanager_visual_contract_test
        portalmanager_capture_contract_test
        portalchooserlogichelper_test
        filemanager_dialogs_smoke_test
        workspacemanager_dbus_test
        workspacemanager_workspace_ops_test
        workspace_shortcuts_guard_test
        workspace_overlay_guard_test
        motion_scheduler_ui_guard_test
        topbar_mainmenu_recent_icons_guard_test
        notification_animation_contract_test
        notification_visual_contract_test
        notificationmanager_priority_routing_test
        docksystem_render_contract_test
        daemonhealthmonitor_test
        dbussecurityguard_test
        policyengine_test
        permissionstore_payload_contract_test
        trustresolver_test
        policy_integration_test
        portal_adapter_lifecycle_persistence_test
        portal_adapter_race_cancellation_test
        portalresponseserializer_contract_test
        portal_app_context_resolver_test
        slmcapabilityservice_dbus_test
        slmactionregistry_test
        slmcapability_framework_test
        slmconfigmanager_validation_test
        slmsessionstate_io_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(smoke_backends
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/smoke-backends.sh
    DEPENDS slm-desktop
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

# ── Login / Session Stack ─────────────────────────────────────────────────────

add_library(slm-login-lib STATIC ${SLM_LOGIN_LIB_SOURCES})
add_library(SlmLogin::Lib ALIAS slm-login-lib)
target_include_directories(slm-login-lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(slm-login-lib PUBLIC Qt6::Core)

# slm-session-broker
qt_add_executable(slm-session-broker ${SLM_SESSION_BROKER_SOURCES})
target_sources(slm-session-broker PRIVATE
    src/login/session-broker/sessionrollback.cpp
    src/login/session-broker/sessionrollback.h
)
target_link_libraries(slm-session-broker
    PRIVATE
        Qt6::Core
        SlmLogin::Lib
)
target_include_directories(slm-session-broker PRIVATE ${CMAKE_SOURCE_DIR})

# slm-watchdog
qt_add_executable(slm-watchdog ${SLM_WATCHDOG_SOURCES})
target_link_libraries(slm-watchdog
    PRIVATE
        Qt6::Core
        SlmLogin::Lib
)
target_include_directories(slm-watchdog PRIVATE ${CMAKE_SOURCE_DIR})

# slm-greeter
qt_add_executable(slm-greeter ${SLM_GREETER_SOURCES})
qt_add_qml_module(slm-greeter
    URI SlmGreeter
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Qml/greeter/Main.qml
        Qml/greeter/LoginPage.qml
        Qml/greeter/ModeSelector.qml
        Qml/greeter/PowerRow.qml
        Qml/components/system/MissingComponentsCard.qml
    RESOURCES
        Qml/greeter/assets/Background.jpeg
        Qml/greeter/assets/default-logo.svg
)
target_link_libraries(slm-greeter
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
        SlmLogin::Lib
)
target_include_directories(slm-greeter PRIVATE ${CMAKE_SOURCE_DIR})

install(TARGETS slm-session-broker
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(TARGETS slm-watchdog
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS slm-greeter
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES sessions/slm.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)

# slm-recovery-app
qt_add_executable(slm-recovery-app ${SLM_RECOVERY_APP_SOURCES})
qt_add_qml_module(slm-recovery-app
    URI SlmRecovery
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Qml/recovery/Main.qml
        Qml/recovery/ResetPage.qml
        Qml/recovery/SnapshotPage.qml
        Qml/recovery/LogPage.qml
        Qml/components/system/MissingComponentsCard.qml
)
target_link_libraries(slm-recovery-app
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        SlmLogin::Lib
)
target_include_directories(slm-recovery-app PRIVATE ${CMAKE_SOURCE_DIR})
install(TARGETS slm-recovery-app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# slm-polkit-agent (phase-1 bootstrap)
qt_add_executable(slm-polkit-agent
    src/login/polkit-agent/main.cpp
    src/login/polkit-agent/polkitagentapp.cpp
    src/login/polkit-agent/polkitagentapp.h
    src/login/polkit-agent/authsession.cpp
    src/login/polkit-agent/authsession.h
    src/login/polkit-agent/authdialogcontroller.cpp
    src/login/polkit-agent/authdialogcontroller.h
    src/apps/settings/desktopsettingsclient.cpp
    src/apps/settings/desktopsettingsclient.h
)
qt_add_qml_module(slm-polkit-agent
    URI SlmPolkitAgent
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Qml/polkit-agent/AuthDialog.qml
)
file(GLOB SLM_STYLE_MODULE_FILES
    "${CMAKE_SOURCE_DIR}/third_party/slm-style/qml/SlmStyle/*.qml"
    "${CMAKE_SOURCE_DIR}/third_party/slm-style/qml/SlmStyle/qmldir"
)
# Keep a filesystem QML module copy in the build root so components loaded
# from file:// (tests, ad-hoc loaders) can still resolve `import SlmStyle`.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/SlmStyle")
foreach(_slm_style_file ${SLM_STYLE_MODULE_FILES})
    configure_file("${_slm_style_file}"
                   "${CMAKE_CURRENT_BINARY_DIR}/SlmStyle/"
                   COPYONLY)
endforeach()
qt_add_resources(slm-desktop "slm_desktop_slmstyle_module"
    PREFIX "/qt/qml/SlmStyle"
    BASE "${CMAKE_SOURCE_DIR}/third_party/slm-style/qml/SlmStyle"
    FILES ${SLM_STYLE_MODULE_FILES}
)
qt_add_resources(slm-settings "slm_settings_slmstyle_module"
    PREFIX "/qt/qml/SlmStyle"
    BASE "${CMAKE_SOURCE_DIR}/third_party/slm-style/qml/SlmStyle"
    FILES ${SLM_STYLE_MODULE_FILES}
)
qt_add_resources(slm-polkit-agent "slm_polkit_agent_slmstyle_module"
    PREFIX "/qt/qml/SlmStyle"
    BASE "${CMAKE_SOURCE_DIR}/third_party/slm-style/qml/SlmStyle"
    FILES ${SLM_STYLE_MODULE_FILES}
)
target_link_libraries(slm-polkit-agent
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
if(POLKIT_AGENT_FOUND AND POLKIT_GOBJECT_FOUND)
    target_compile_definitions(slm-polkit-agent PRIVATE SLM_HAVE_POLKIT_AGENT=1)
    target_link_libraries(slm-polkit-agent PRIVATE PkgConfig::POLKIT_AGENT PkgConfig::POLKIT_GOBJECT)
endif()
target_include_directories(slm-polkit-agent PRIVATE ${CMAKE_SOURCE_DIR})
install(TARGETS slm-polkit-agent
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
