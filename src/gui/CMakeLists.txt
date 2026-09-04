cmake_minimum_required(VERSION 3.16)
project(rfp-gui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core Concurrent)

qt6_add_resources(rfp-gui_RESOURCES
    docs.qrc
)

add_executable(rfp-gui
    main.cpp
    MainWindow.cpp
    MainWindow.h
    QtImageAdapter.cpp
    QtImageAdapter.h
    HelpDialog.cpp
    HelpDialog.h
    SettingsDialog.cpp
    SettingsDialog.h
    MaskingDialog.cpp
    MaskingDialog.h
    ${rfp-gui_RESOURCES}
)

target_link_libraries(rfp-gui
    PRIVATE
        Qt6::Widgets
        Qt6::Gui
        Qt6::Core
        Qt6::Concurrent
        rfp::core
        rfp::stego
        rfp::crypto
)

set_target_properties(rfp-gui PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

install(TARGETS rfp-gui BUNDLE DESTINATION . LIBRARY DESTINATION bin RUNTIME DESTINATION bin)