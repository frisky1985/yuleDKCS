add_executable(${TARGET}
    main.c
    bringup_selftest.c
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal
    ${CMAKE_SOURCE_DIR}/hal/kw47a
)
