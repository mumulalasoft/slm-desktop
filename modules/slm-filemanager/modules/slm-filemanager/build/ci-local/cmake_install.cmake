# Install script for directory: /home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/modules/slm-filemanager/build/ci-local/libslm-filemanager-core.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/slm-filemanager" TYPE FILE FILES
    "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/../../src/apps/filemanager/include/filemanagerapi.h"
    "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/../../src/apps/filemanager/include/filemanagermodel.h"
    "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/../../src/apps/filemanager/include/filemanagermodelfactory.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SlmFileManager/SlmFileManagerTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SlmFileManager/SlmFileManagerTargets.cmake"
         "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/modules/slm-filemanager/build/ci-local/CMakeFiles/Export/0f62fecc05b320de90e7274d0ca885b1/SlmFileManagerTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SlmFileManager/SlmFileManagerTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SlmFileManager/SlmFileManagerTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SlmFileManager" TYPE FILE FILES "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/modules/slm-filemanager/build/ci-local/CMakeFiles/Export/0f62fecc05b320de90e7274d0ca885b1/SlmFileManagerTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SlmFileManager" TYPE FILE FILES "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/modules/slm-filemanager/build/ci-local/CMakeFiles/Export/0f62fecc05b320de90e7274d0ca885b1/SlmFileManagerTargets-noconfig.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SlmFileManager" TYPE FILE FILES
    "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/modules/slm-filemanager/build/ci-local/SlmFileManagerConfig.cmake"
    "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/modules/slm-filemanager/build/ci-local/SlmFileManagerConfigVersion.cmake"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/modules/slm-filemanager/build/ci-local/tests/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/garis/Development/Qt/Desktop_Shell/modules/slm-filemanager/modules/slm-filemanager/build/ci-local/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
