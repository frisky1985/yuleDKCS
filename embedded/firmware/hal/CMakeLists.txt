# HAL (Hardware Abstraction Layer)
#
# Two backends are selected by the top-level firmware CMakeLists:
#   - host/hal_host.c   : host simulation (CI / link verification, no NXP SDK)
#   - kw47a/hal_kw47a.c : real NXP KW47A target (requires MCUXpresso SDK)

if(FIRMWARE_HOST_SIM)
    add_library(hal STATIC
        host/hal_host.c
    )
    target_include_directories(hal PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kw47a
    )
else()
    add_library(hal STATIC
        kw47a/hal_kw47a.c
    )
    target_include_directories(hal PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kw47a
    )
    target_link_libraries(hal PRIVATE
        ${MCUX_SDK_PLATFORM_LIBS}
    )
endif()
