add_library(rfp_stego STATIC
    Capacity.cpp
    ImageBuffer.cpp
    StegoDecoder.cpp
    StegoEncoder.cpp
    StegoSlots.cpp
    StegoSlots.h
)

add_library(rfp::stego ALIAS rfp_stego)

rfp_set_project_options(rfp_stego)
rfp_set_compiler_warnings(rfp_stego)

target_include_directories(rfp_stego
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rfp_stego
    PUBLIC
        rfp::core
)
