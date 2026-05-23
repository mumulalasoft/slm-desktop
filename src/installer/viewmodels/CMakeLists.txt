add_library(slm_installer_viewmodels STATIC
    SlmInstallerDiskModel.cpp
    SlmInstallerDiskModel.h
)

set_target_properties(slm_installer_viewmodels PROPERTIES
    AUTOMOC ON
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(slm_installer_viewmodels
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(slm_installer_viewmodels
    PUBLIC
        Qt6::Core
)
