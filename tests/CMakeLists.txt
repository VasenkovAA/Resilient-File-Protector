include(GoogleTest)

add_executable(rfp-tests
    core/Crc32Tests.cpp
    crypto/CryptoRegistryTests.cpp
    crypto/CipherTests.cpp
    crypto/HashTests.cpp
    crypto/KdfTests.cpp
    crypto/RandomTests.cpp
    crypto/SecureMemoryTests.cpp
    crypto/EncodingTests.cpp
    crypto/CryptoPlaceholderTests.cpp
    payload/PayloadCryptoTests.cpp
    stego/StegoParamsTests.cpp
    stego/StegoDispersionTests.cpp
    stego/StegoRoundTripTests.cpp
    stego/StegoSmartSelectionTests.cpp
    stego/StegoEdgeCasesTests.cpp
)

rfp_set_project_options(rfp-tests)
rfp_set_compiler_warnings(rfp-tests)

target_include_directories(rfp-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto    # <-- чтобы PayloadCryptoTests нашел TestHelpers.h
)

target_link_libraries(rfp-tests
    PRIVATE
        rfp::core
        rfp::stego
        rfp::crypto
        rfp::payload
        GTest::gtest_main
)

gtest_discover_tests(rfp-tests)
