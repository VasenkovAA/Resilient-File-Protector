add_library(rfp_payload STATIC
    PayloadCrypto.cpp
)

add_library(rfp::payload ALIAS rfp_payload)

rfp_set_project_options(rfp_payload)
rfp_set_compiler_warnings(rfp_payload)

target_include_directories(rfp_payload
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(rfp_payload
    PUBLIC
        rfp::core
        rfp::crypto
)
