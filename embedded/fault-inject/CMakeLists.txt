# =============================================================================
# CMakeLists.txt — yuleDKCS Fault Injection Module
#
# Targets:
#   dk-fault-inject          — Static library (production = stubs)
#   dk-fault-inject-test     — Static library (injection enabled)
#   dk-fault-inject-runner   — Host-side test executable
#
# Build Configuration:
#   DK_FAULT_INJECT_TESTS    (BOOL) — Enable fault injection for test builds
#   DK_FAULT_INJECT_BUILD_TEST (BOOL) — Build the self-test executable
#
# Dependencies:
#   - yuleOSH FaultInject framework (FaultInject.h, FaultInject_Cfg.h, etc.)
#     Expected at: ${YULEOSH_FAULT_INJECT_DIR}
# =============================================================================

cmake_minimum_required(VERSION 3.16)

project(dk-fault-inject
    VERSION 1.0.0
    DESCRIPTION "yuleDKCS Digital Key Fault Injection Framework"
    LANGUAGES C
)

# ── Options ──────────────────────────────────────────────────────────────────
option(DK_FAULT_INJECT_TESTS       "Build with fault injection enabled"         OFF)
option(DK_FAULT_INJECT_BUILD_TEST  "Build self-test executable"                 OFF)

# ── Sources ───────────────────────────────────────────────────────────────────
set(DK_FAULT_INJECT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DKFaultInject.c
)

# ── Include paths ────────────────────────────────────────────────────────────
set(DK_FAULT_INJECT_INCS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# Optionally include yuleOSH fault-inject framework if available
if(DEFINED YULEOSH_FAULT_INJECT_DIR)
    list(APPEND DK_FAULT_INJECT_INCS ${YULEOSH_FAULT_INJECT_DIR}/inc)
endif()

# ============================================================================
# Target 1: dk-fault-inject (production library — stubs, injection OFF)
# ============================================================================
add_library(dk-fault-inject STATIC
    ${DK_FAULT_INJECT_SOURCES}
)

target_include_directories(dk-fault-inject PUBLIC ${DK_FAULT_INJECT_INCS})

target_compile_definitions(dk-fault-inject PRIVATE
    $<$<BOOL:${DK_FAULT_INJECT_TESTS}>:DK_FAULT_INJECT_ENABLE=1>
)

# ============================================================================
# Target 2: dk-fault-inject-test (test library — injection enabled)
# ============================================================================
if(DK_FAULT_INJECT_TESTS)
    add_library(dk-fault-inject-test STATIC
        ${DK_FAULT_INJECT_SOURCES}
    )

    target_include_directories(dk-fault-inject-test PUBLIC ${DK_FAULT_INJECT_INCS})
    target_compile_definitions(dk-fault-inject-test PRIVATE
        DK_FAULT_INJECT_ENABLE=1
    )

    # ── Self-test executable ──────────────────────────────────────────────
    if(DK_FAULT_INJECT_BUILD_TEST)
        set(DK_FI_TEST_SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/test/test_dk_fault_inject.c
        )

        add_executable(dk-fault-inject-runner
            ${DK_FI_TEST_SOURCES}
        )

        target_include_directories(dk-fault-inject-runner PRIVATE
            ${DK_FAULT_INJECT_INCS}
            ${CMAKE_CURRENT_SOURCE_DIR}/test
        )

        target_link_libraries(dk-fault-inject-runner PRIVATE
            dk-fault-inject-test
        )

        target_compile_definitions(dk-fault-inject-runner PRIVATE
            DK_FAULT_INJECT_ENABLE=1
        )

        # Register test (requires CTest)
        include(CTest)
        add_test(NAME dk-fault-inject-self-test
            COMMAND dk-fault-inject-runner
        )
    endif()
endif()

# ── Install rules ────────────────────────────────────────────────────────────
install(TARGETS dk-fault-inject
    ARCHIVE  DESTINATION lib
    LIBRARY  DESTINATION lib
    RUNTIME  DESTINATION bin
)
install(DIRECTORY inc/
    DESTINATION include/dk-fault-inject
    FILES_MATCHING PATTERN "*.h"
)
