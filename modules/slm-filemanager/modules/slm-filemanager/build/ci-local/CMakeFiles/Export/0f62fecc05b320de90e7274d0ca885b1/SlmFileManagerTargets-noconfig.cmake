#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SlmFileManager::slm-filemanager-core" for configuration ""
set_property(TARGET SlmFileManager::slm-filemanager-core APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(SlmFileManager::slm-filemanager-core PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libslm-filemanager-core.a"
  )

list(APPEND _cmake_import_check_targets SlmFileManager::slm-filemanager-core )
list(APPEND _cmake_import_check_files_for_SlmFileManager::slm-filemanager-core "${_IMPORT_PREFIX}/lib/libslm-filemanager-core.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
