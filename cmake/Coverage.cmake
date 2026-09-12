# cmake/Coverage.cmake
#
# Cross-platform code-coverage instrumentation + report target.
#
#   cmake -S . -B build/cov -G Ninja \
#         -DCMAKE_BUILD_TYPE=Debug \
#         -DRFP_ENABLE_COVERAGE=ON
#   cmake --build build/cov --target coverage
#
# Generates under build/cov/coverage/:
#   - coverage.txt       (text summary, always)
#   - index.html         (browsable HTML, gcovr / lcov)
#   - coverage.xml       (Cobertura, for CI / Codecov)
#   - coverage.json      (machine-readable summary, gcovr only)
#
# Tooling per platform:
#   Linux / macOS  -> gcovr (preferred) or lcov+genhtml
#   Windows        -> OpenCppCoverage (Chocolatey / vcpkg) + ReportGenerator
#
# Installation:
#   pip install gcovr                        (any platform with Python)
#   apt install lcov                         (Linux fallback)
#   brew install gcovr lcov                  (macOS fallback)
#   choco install opencppcoverage reportgenerator  (Windows)

option(RFP_ENABLE_COVERAGE "Instrument build for code coverage" OFF)

if(RFP_ENABLE_COVERAGE)
    message(STATUS "Coverage: instrumentation enabled")

    if(MSVC)
        # MSVC has no gcov. We rely on OpenCppCoverage, which needs PDBs
        # and no inlining to get accurate line info.
        add_compile_options(/Zi /Od /Ob0)
        add_link_options(/DEBUG:FULL /INCREMENTAL:NO)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(
            --coverage
            -O0 -g
            -fno-inline
            -fno-inline-small-functions
            -fno-default-inline)
        add_link_options(--coverage)
    else()
        message(WARNING
            "Coverage: unsupported compiler '${CMAKE_CXX_COMPILER_ID}'. "
            "Instrumentation will be skipped.")
    endif()
endif()

# ---------------------------------------------------------------------------
#  rfp_add_coverage_target()
#
#  Call once from the top-level CMakeLists.txt after all targets are defined.
# ---------------------------------------------------------------------------
function(rfp_add_coverage_target)
    if(NOT RFP_ENABLE_COVERAGE)
        return()
    endif()

    set(REPORT_DIR "${CMAKE_BINARY_DIR}/coverage")
    set(SRC_ROOT   "${PROJECT_SOURCE_DIR}")
    set(TEST_EXE   $<TARGET_FILE:rfp-tests>)

    # ---------- Windows / MSVC: OpenCppCoverage ----------
    if(MSVC)
        find_program(OPENCPPCOVERAGE_EXE
            NAMES OpenCppCoverage OpenCppCoverage.exe
            HINTS "$ENV{ProgramFiles}/OpenCppCoverage"
                  "$ENV{ChocolateyInstall}/bin")
        if(NOT OPENCPPCOVERAGE_EXE)
            add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E echo
                    "OpenCppCoverage not found. Install: choco install opencppcoverage"
                COMMAND ${CMAKE_COMMAND} -E false
                COMMENT "Coverage tool missing")
            return()
        endif()

        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory "${REPORT_DIR}"
            COMMAND "${OPENCPPCOVERAGE_EXE}"
                    --sources "${SRC_ROOT}/src"
                    --sources "${SRC_ROOT}/include"
                    --excluded_sources "${SRC_ROOT}/third_party"
                    --excluded_sources "${SRC_ROOT}/tests"
                    --excluded_sources "${CMAKE_BINARY_DIR}"
                    --export_type cobertura:"${REPORT_DIR}/coverage.xml"
                    -- "${TEST_EXE}"
            COMMAND ${CMAKE_COMMAND} -E echo
                    "Cobertura report written to ${REPORT_DIR}/coverage.xml"
            COMMAND ${CMAKE_COMMAND} -E echo
                    "For HTML: reportgenerator -reports:${REPORT_DIR}/coverage.xml -targetdir:${REPORT_DIR}/html"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            USES_TERMINAL
            COMMENT "Running tests under OpenCppCoverage")
        return()
    endif()

    # ---------- POSIX: gcovr (preferred) ----------
    find_program(GCOVR_EXE NAMES gcovr)
    if(GCOVR_EXE)
        add_custom_target(coverage
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${CMAKE_COMMAND} -E make_directory "${REPORT_DIR}"
            COMMAND "${GCOVR_EXE}"
                --root            "${SRC_ROOT}"
                --object-directory "${CMAKE_BINARY_DIR}"
                --filter          "${SRC_ROOT}/src/"
                --filter          "${SRC_ROOT}/include/"
                --exclude         "${SRC_ROOT}/third_party/"
                --exclude         "${SRC_ROOT}/tests/"
                --exclude         "${CMAKE_BINARY_DIR}/"
                --exclude-unreachable-branches
                --exclude-throw-branches
                --print-summary
                --txt             "${REPORT_DIR}/coverage.txt"
                --xml             "${REPORT_DIR}/coverage.xml"
                --html-details    "${REPORT_DIR}/index.html"
                --json-summary    "${REPORT_DIR}/coverage.json"
                --json-summary-pretty
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            USES_TERMINAL
            COMMENT "Running tests + generating coverage report (gcovr)")
        return()
    endif()

    # ---------- POSIX: lcov + genhtml (fallback) ----------
    find_program(LCOV_EXE    NAMES lcov)
    find_program(GENHTML_EXE NAMES genhtml)
    if(LCOV_EXE AND GENHTML_EXE)
        set(LCOV_INFO "${REPORT_DIR}/lcov.info")
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory "${REPORT_DIR}"
            COMMAND "${LCOV_EXE}" --zerocounters --directory "${CMAKE_BINARY_DIR}"
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND "${LCOV_EXE}"
                --capture --directory "${CMAKE_BINARY_DIR}"
                --output-file "${LCOV_INFO}"
                --no-external
                --rc geninfo_unexecuted_blocks=1
            COMMAND "${LCOV_EXE}"
                --remove "${LCOV_INFO}"
                    "*/third_party/*" "*/tests/*" "/usr/*" "*/_deps/*"
                --output-file "${LCOV_INFO}"
            COMMAND "${GENHTML_EXE}"
                "${LCOV_INFO}"
                --output-directory "${REPORT_DIR}/html"
                --legend --show-details
            COMMAND ${CMAKE_COMMAND} -E echo
                    "Open ${REPORT_DIR}/html/index.html"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            USES_TERMINAL
            COMMENT "Running tests + generating coverage report (lcov)")
        return()
    endif()

    # ---------- No tool found ----------
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E echo
            "No coverage tool found."
        COMMAND ${CMAKE_COMMAND} -E echo
            "  Linux/macOS : pip install gcovr   (or: apt install lcov)"
        COMMAND ${CMAKE_COMMAND} -E echo
            "  Windows     : choco install opencppcoverage reportgenerator"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Coverage tool missing")
endfunction()