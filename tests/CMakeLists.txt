include(GoogleTest)

add_executable(rfp-tests
    core/Crc32Tests.cpp
    stego/StegoParamsTests.cpp
    stego/StegoDispersionTests.cpp
    stego/StegoRoundTripTests.cpp
    stego/StegoSmartSelectionTests.cpp
    stego/StegoEdgeCasesTests.cpp
)

rfp_set_project_options(rfp-tests)
rfp_set_compiler_warnings(rfp-tests)

target_link_libraries(rfp-tests
    PRIVATE
        rfp::core
        rfp::stego
        GTest::gtest_main
        
)

gtest_discover_tests(rfp-tests)
