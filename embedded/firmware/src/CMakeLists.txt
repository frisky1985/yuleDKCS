add_executable(${TARGET}
    main.c
    bringup_selftest.c
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal
    ${CMAKE_SOURCE_DIR}/hal/kw47a
)

if(SECURE_BOOT)
    target_sources(${TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/../secure_boot/src/secure_boot.c)
    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/../secure_boot/include
        ${CMAKE_SOURCE_DIR}/../icce_protocol/src/crypto)
endif()
