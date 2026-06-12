add_executable(rfp-gui
    main.cpp
    MainWindow.cpp
    MainWindow.h
    QtImageAdapter.cpp
    QtImageAdapter.h
)

rfp_set_project_options(rfp-gui)
rfp_set_compiler_warnings(rfp-gui)

target_link_libraries(rfp-gui
    PRIVATE
        Qt6::Widgets
        Qt6::Gui
        Qt6::Core
        rfp::core
        rfp::stego
        rfp::crypto
)

set_target_properties(rfp-gui PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
