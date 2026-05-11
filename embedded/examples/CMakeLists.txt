# 示例程序 CMakeLists.txt

# CCC 示例
if(ENABLE_CCC)
    add_executable(example_ccc_pairing
        example_ccc_pairing.c
    )
    target_link_libraries(example_ccc_pairing DKCS)
    
    add_executable(example_ccc_unlock
        example_ccc_unlock.c
    )
    target_link_libraries(example_ccc_unlock DKCS)
endif()

# ICCE 示例
if(ENABLE_ICCE)
    add_executable(example_icce_pairing
        example_icce_pairing.c
    )
    target_link_libraries(example_icce_pairing DKCS)
endif()

# ICCOA 示例
if(ENABLE_ICCOA)
    add_executable(example_iccoa_control
        example_iccoa_control.c
    )
    target_link_libraries(example_iccoa_control DKCS)
endif()

# 统一接口示例
add_executable(example_unified_api
    example_unified_api.c
)
target_link_libraries(example_unified_api DKCS)
