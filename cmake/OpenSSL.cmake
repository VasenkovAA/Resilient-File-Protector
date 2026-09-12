# cmake/OpenSSL.cmake
#
# OpenSSL integration.
#
# Strategy:
#   RFP_USE_SUBMODULE_OPENSSL = AUTO (default)
#       - if system OpenSSL found -> use it (fast, recommended)
#       - otherwise -> build from third_party/openssl submodule
#   RFP_USE_SUBMODULE_OPENSSL = ON   -> always build from submodule
#   RFP_USE_SUBMODULE_OPENSSL = OFF  -> always use system OpenSSL (error if not found)
#
set(RFP_USE_SUBMODULE_OPENSSL "AUTO" CACHE STRING
    "Use OpenSSL from submodule: AUTO | ON | OFF")
set_property(CACHE RFP_USE_SUBMODULE_OPENSSL
    PROPERTY STRINGS AUTO ON OFF)

find_package(OpenSSL QUIET)

set(_rfp_build_openssl FALSE)
if(RFP_USE_SUBMODULE_OPENSSL STREQUAL "ON")
    set(_rfp_build_openssl TRUE)
elseif(RFP_USE_SUBMODULE_OPENSSL STREQUAL "AUTO")
    if(NOT OpenSSL_FOUND)
        set(_rfp_build_openssl TRUE)
    endif()
elseif(RFP_USE_SUBMODULE_OPENSSL STREQUAL "OFF")
    if(NOT OpenSSL_FOUND)
        message(FATAL_ERROR
            "System OpenSSL not found and RFP_USE_SUBMODULE_OPENSSL=OFF. "
            "Install libssl-dev (Ubuntu) / openssl (brew) / vcpkg install openssl.")
    endif()
else()
    message(FATAL_ERROR "Invalid RFP_USE_SUBMODULE_OPENSSL='${RFP_USE_SUBMODULE_OPENSSL}'")
endif()

if(_rfp_build_openssl)
    message(STATUS "OpenSSL: building from third_party/openssl submodule")

    include(ExternalProject)

    set(_openssl_src "${PROJECT_SOURCE_DIR}/third_party/openssl")
    if(NOT EXISTS "${_openssl_src}/Configure")
        message(FATAL_ERROR
            "OpenSSL submodule not initialised. "
            "Run: git submodule update --init --recursive")
    endif()

    set(_openssl_prefix  "${CMAKE_BINARY_DIR}/openssl-install")
    set(_openssl_include "${_openssl_prefix}/include")
    set(_openssl_libdir  "${_openssl_prefix}/lib")

    # IMPORTANT: create directories NOW so CMake does not complain about
    # non-existent INTERFACE_INCLUDE_DIRECTORIES at configure time.
    file(MAKE_DIRECTORY "${_openssl_include}")
    file(MAKE_DIRECTORY "${_openssl_libdir}")

    if(WIN32)
        set(_openssl_configure
            perl "${_openssl_src}/Configure"
                 VC-WIN64A
                 --prefix=${_openssl_prefix}
                 --openssldir=${_openssl_prefix}
                 no-shared no-tests no-apps)
        set(_openssl_build   nmake)
        set(_openssl_install nmake install_sw)
        set(_openssl_crypto  "${_openssl_libdir}/libcrypto.lib")
        set(_openssl_ssl     "${_openssl_libdir}/libssl.lib")
    else()
        set(_openssl_configure
            "${_openssl_src}/Configure"
            --prefix=${_openssl_prefix}
            --openssldir=${_openssl_prefix}
            no-shared no-tests no-apps)
        set(_openssl_build   make -j)
        set(_openssl_install make install_sw)
        set(_openssl_crypto  "${_openssl_libdir}/libcrypto.a")
        set(_openssl_ssl     "${_openssl_libdir}/libssl.a")
    endif()

    ExternalProject_Add(rfp_openssl_build
        SOURCE_DIR        "${_openssl_src}"
        PREFIX            "${CMAKE_BINARY_DIR}/openssl-ep"
        CONFIGURE_COMMAND ${_openssl_configure}
        BUILD_COMMAND     ${_openssl_build}
        INSTALL_COMMAND   ${_openssl_install}
        BUILD_IN_SOURCE   FALSE
        UPDATE_COMMAND    ""
        BUILD_BYPRODUCTS  "${_openssl_crypto}" "${_openssl_ssl}"
        LOG_CONFIGURE     ON
        LOG_BUILD         ON
        LOG_INSTALL       ON
    )

    add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION             "${_openssl_crypto}"
        INTERFACE_INCLUDE_DIRECTORIES "${_openssl_include}"
    )
    add_dependencies(OpenSSL::Crypto rfp_openssl_build)

    add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION             "${_openssl_ssl}"
        INTERFACE_INCLUDE_DIRECTORIES "${_openssl_include}"
        INTERFACE_LINK_LIBRARIES      OpenSSL::Crypto
    )
    add_dependencies(OpenSSL::SSL rfp_openssl_build)

    # OpenSSL needs dl + pthread on Linux
    if(UNIX AND NOT APPLE)
        set_property(TARGET OpenSSL::Crypto APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES dl pthread)
    endif()

else()
    if(NOT OpenSSL_FOUND)
        message(FATAL_ERROR "OpenSSL not found and submodule build disabled.")
    endif()
    message(STATUS "OpenSSL: using system package — ${OPENSSL_VERSION}")
    # find_package(OpenSSL) already defines OpenSSL::Crypto and OpenSSL::SSL
endif()