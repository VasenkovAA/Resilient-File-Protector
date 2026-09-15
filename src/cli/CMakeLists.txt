find_package(PNG REQUIRED)

# libpng API шумит под -Wconversion, поэтому изолируем его в отдельной
# библиотеке без rfp_set_compiler_warnings.
add_library(rfp_cli_png STATIC PngIO.cpp)
rfp_set_project_options(rfp_cli_png)
target_include_directories(rfp_cli_png
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(rfp_cli_png
    PRIVATE
        rfp::core
        rfp::stego
        PNG::PNG
)

add_executable(rfp-cli
    main.cpp
    ImageIO.cpp
)

rfp_set_project_options(rfp-cli)
rfp_set_compiler_warnings(rfp-cli)

target_link_libraries(rfp-cli
    PRIVATE
        rfp::core
        rfp::stego
        rfp::crypto
        rfp::payload
        rfp_cli_png
)
