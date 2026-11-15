# HAL (Hardware Abstraction Layer)
#
# Two backends are selected by the top-level firmware CMakeLists:
#   - host/hal_host.c   : host simulation (CI / link verification, no NXP SDK)
#   - kw47a/hal_kw47a.c : real NXP KW47A target (requires MCUXpresso SDK)

if(FIRMWARE_HOST_SIM)
    add_library(hal STATIC
        host/hal_host.c
        kw47a/kw47a_nv.c
        kw47a/kw47a_flash_ll.c
        kw47a/kw47a_otp_ll.c
        kw47a/kw47a_flash_mcux.c
    )
    target_include_directories(hal PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kw47a
    )
else()
    add_library(hal STATIC
        kw47a/hal_kw47a.c
        kw47a/kw47a_nv.c
        kw47a/kw47a_flash_ll.c
        kw47a/kw47a_otp_ll.c
        kw47a/kw47a_flash_mcux.c
    )
    target_include_directories(hal PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kw47a
    )
    target_link_libraries(hal PRIVATE
        ${MCUX_SDK_PLATFORM_LIBS}
    )
endif()

# sec_nv_ops_t / sec_nv_bind() (ccc_protocol security module)
target_include_directories(hal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccc_protocol/src/security
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ccc_protocol/include
)
