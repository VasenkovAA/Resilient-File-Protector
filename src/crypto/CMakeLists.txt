add_library(rfp_crypto STATIC
    CryptoPlaceholder.cpp
)

add_library(rfp::crypto ALIAS rfp_crypto)

rfp_set_project_options(rfp_crypto)
rfp_set_compiler_warnings(rfp_crypto)

target_include_directories(rfp_crypto
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(rfp_crypto
    PUBLIC
        rfp::core
)
