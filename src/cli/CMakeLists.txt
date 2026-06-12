add_executable(rfp-cli
    main.cpp
)

rfp_set_project_options(rfp-cli)
rfp_set_compiler_warnings(rfp-cli)

target_link_libraries(rfp-cli
    PRIVATE
        rfp::core
        rfp::stego
)
