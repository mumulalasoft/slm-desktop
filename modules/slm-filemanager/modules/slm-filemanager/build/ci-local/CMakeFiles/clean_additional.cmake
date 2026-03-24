# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles/slm-filemanager-core_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/slm-filemanager-core_autogen.dir/ParseCache.txt"
  "slm-filemanager-core_autogen"
  )
endif()
