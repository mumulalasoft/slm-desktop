# cmake/FindSlmFileManager.cmake
#
# Menentukan target SlmFileManager::Core dan SlmFileManager::Integration
# dengan dua mode operasi eksplisit:
#
#   DEV MODE  (SLM_USE_LOCAL_FILEMANAGER=ON):
#     Cari package dari local workspace checkout.
#     Konvensi: repo filemanager ada di ../filemanager relatif terhadap
#     source dir desktop shell, atau ditentukan via SLM_FILEMANAGER_BUILD_DIR.
#
#   RELEASE MODE (SLM_USE_LOCAL_FILEMANAGER=OFF, default):
#     find_package() normal dari system/installed prefix.
#
# Variabel input:
#   SLM_USE_LOCAL_FILEMANAGER   BOOL   ON = dev mode
#   SLM_FILEMANAGER_SOURCE_DIR  PATH   override source dir (opsional di dev mode)
#   SLM_FILEMANAGER_BUILD_DIR   PATH   override build dir  (opsional di dev mode)
#
# Target yang tersedia setelah include:
#   SlmFileManager::Core          library inti (selalu tersedia)
#   SlmFileManager::Integration   shell bridge layer (tersedia jika dibangun)

option(SLM_USE_LOCAL_FILEMANAGER
    "Use local workspace checkout of slm-filemanager instead of installed package"
    OFF)

set(SLM_FILEMANAGER_SOURCE_DIR "" CACHE PATH
    "Path to slm-filemanager source directory (DEV MODE only)")
set(SLM_FILEMANAGER_BUILD_DIR "" CACHE PATH
    "Path to slm-filemanager build directory (DEV MODE only)")

# ── DEV MODE ──────────────────────────────────────────────────────────────────
if(SLM_USE_LOCAL_FILEMANAGER)
    # Resolve source dir: argumen eksplisit > konvensi workspace berdampingan
    if(NOT SLM_FILEMANAGER_SOURCE_DIR)
        get_filename_component(_slm_workspace "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)
        set(SLM_FILEMANAGER_SOURCE_DIR "${_slm_workspace}/filemanager")
    endif()
    # Resolve build dir
    if(NOT SLM_FILEMANAGER_BUILD_DIR)
        set(SLM_FILEMANAGER_BUILD_DIR "${SLM_FILEMANAGER_SOURCE_DIR}/build/dev")
    endif()

    if(NOT EXISTS "${SLM_FILEMANAGER_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "[SLM] SLM_USE_LOCAL_FILEMANAGER=ON tapi source tidak ditemukan di:\n"
            "  ${SLM_FILEMANAGER_SOURCE_DIR}\n"
            "Set SLM_FILEMANAGER_SOURCE_DIR ke path checkout lokal filemanager.")
    endif()

    message(STATUS "[SLM] FileManager: DEV MODE — ${SLM_FILEMANAGER_SOURCE_DIR}")

    # Configure sub-build jika CMakeCache belum ada
    if(NOT EXISTS "${SLM_FILEMANAGER_BUILD_DIR}/CMakeCache.txt")
        message(STATUS "[SLM] Configuring filemanager sub-build di ${SLM_FILEMANAGER_BUILD_DIR}...")
        set(_install_prefix "${SLM_FILEMANAGER_BUILD_DIR}/install")
        execute_process(
            COMMAND ${CMAKE_COMMAND}
                -B "${SLM_FILEMANAGER_BUILD_DIR}"
                -S "${SLM_FILEMANAGER_SOURCE_DIR}"
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_INSTALL_PREFIX=${_install_prefix}
            RESULT_VARIABLE _fm_cfg_result
            OUTPUT_QUIET
        )
        if(NOT _fm_cfg_result EQUAL 0)
            message(FATAL_ERROR "[SLM] Gagal configure filemanager sub-build.")
        endif()
    endif()

    # Cari package dari build dir lokal
    find_package(SlmFileManager
        REQUIRED
        PATHS
            "${SLM_FILEMANAGER_BUILD_DIR}"
            "${SLM_FILEMANAGER_BUILD_DIR}/install/lib/cmake/SlmFileManager"
        NO_DEFAULT_PATH
    )

    # Tambah QML import path agar QML engine menemukan SlmFileManager module
    set(SLM_FILEMANAGER_QML_IMPORT_PATH
        "${SLM_FILEMANAGER_BUILD_DIR}/install/lib/qt6/qml"
        CACHE INTERNAL "QML import path for local filemanager build")

# ── RELEASE MODE ──────────────────────────────────────────────────────────────
else()
    message(STATUS "[SLM] FileManager: RELEASE MODE — mencari installed package")
    find_package(SlmFileManager REQUIRED)
    set(SLM_FILEMANAGER_QML_IMPORT_PATH "" CACHE INTERNAL "")
endif()

# ── Verifikasi target ─────────────────────────────────────────────────────────
if(NOT TARGET SlmFileManager::Core)
    message(FATAL_ERROR "[SLM] SlmFileManager::Core target tidak tersedia setelah find_package.")
endif()

# Integration target bersifat opsional — buat stub bila tidak tersedia
if(NOT TARGET SlmFileManager::Integration)
    message(STATUS "[SLM] SlmFileManager::Integration tidak tersedia — menggunakan stub interface.")
    add_library(_slm_fm_integration_stub INTERFACE)
    add_library(SlmFileManager::Integration ALIAS _slm_fm_integration_stub)
endif()

message(STATUS "[SLM] SlmFileManager version: ${SlmFileManager_VERSION}")
