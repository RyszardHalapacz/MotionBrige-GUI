cmake_minimum_required(VERSION 3.16)

project(GUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# ============================================================================
#  motionbridge_gui — DLL eksportująca setPTR / setConfigPath / runGUI
# ============================================================================

add_library(motionbridge_gui SHARED
    mainwindow.cpp
    mainwindow.h
    motionbridge_api.h
    pipeline_options.h
    pipeline_options.cpp
)

target_compile_definitions(motionbridge_gui PRIVATE MOTIONBRIDGE_GUI_EXPORTS)
target_link_libraries(motionbridge_gui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

qt_add_resources(motionbridge_gui "config"
    PREFIX "/config"
    FILES resources/config/pipeline_options.yaml
)

add_custom_command(TARGET motionbridge_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/dark.qss
        $<TARGET_FILE_DIR:motionbridge_gui>/dark.qss
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/icnos
        $<TARGET_FILE_DIR:motionbridge_gui>/icnos
)

include(GNUInstallDirs)
install(TARGETS motionbridge_gui
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
