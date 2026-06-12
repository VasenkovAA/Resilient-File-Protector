add_library(rfp_core STATIC
    ByteBuffer.cpp
    Crc32.cpp
)

add_library(rfp::core ALIAS rfp_core)

rfp_set_project_options(rfp_core)
rfp_set_compiler_warnings(rfp_core)

target_include_directories(rfp_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
