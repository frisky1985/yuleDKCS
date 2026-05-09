# 测试程序 CMakeLists.txt

# CCC 测试
if(ENABLE_CCC)
    add_executable(test_ccc_core
        test_ccc_core.c
    )
    target_link_libraries(test_ccc_core DKCS)
    add_test(NAME ccc_core COMMAND test_ccc_core)
endif()

# ICCE 测试
if(ENABLE_ICCE)
    add_executable(test_icce_core
        test_icce_core.c
    )
    target_link_libraries(test_icce_core DKCS)
    add_test(NAME icce_core COMMAND test_icce_core)
endif()

# ICCOA 测试
if(ENABLE_ICCOA)
    add_executable(test_iccoa_core
        test_iccoa_core.c
    )
    target_link_libraries(test_iccoa_core DKCS)
    add_test(NAME iccoa_core COMMAND test_iccoa_core)
endif()

# 统一接口测试
add_executable(test_dkcs
    test_dkcs.c
)
target_link_libraries(test_dkcs DKCS)
add_test(NAME dkcs COMMAND test_dkcs)
