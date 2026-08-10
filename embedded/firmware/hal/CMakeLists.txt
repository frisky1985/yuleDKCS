add_library(hal_kw47a STATIC
    kw47a/hal_kw47a.c
)

target_include_directories(hal_kw47a PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/kw47a
)

target_link_libraries(hal_kw47a PRIVATE
    ${MCUX_SDK_PLATFORM_LIBS}
)
