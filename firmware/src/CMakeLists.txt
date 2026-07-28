add_executable(${TARGET}
    main.c
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal
    ${CMAKE_SOURCE_DIR}/hal/kw47a
)

target_link_libraries(${TARGET} PRIVATE
    hal_kw47a
    ccc_protocol
    icce_protocol
    iccoa_protocol
)
