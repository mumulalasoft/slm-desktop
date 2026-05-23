# slm-installer-harness — visual harness for installer QML screens.
#
# Built only when SLM_BUILD_INSTALLER=ON (the parent CMakeLists.txt
# short-circuits before reaching this directory otherwise). The harness
# does not get installed by the slm-installer package — it's a
# developer-only tool.

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

qt_add_executable(slm-installer-harness
    harness-main.cpp
    resources.qrc
)

set_target_properties(slm-installer-harness PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(slm-installer-harness
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

# Intentionally no install() — harness is for `cmake --build` only.
