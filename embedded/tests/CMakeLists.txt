# ============================================================
# DKCS 测试程序
# ============================================================

# 测试可执行文件
add_executable(test_ccc_crypto
    test_ccc_crypto.c
)
target_link_libraries(test_ccc_crypto
    DKCS
    mbedcrypto
    sm_crypto
)
target_include_directories(test_ccc_crypto
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
add_test(NAME test_ccc_crypto COMMAND test_ccc_crypto)

add_executable(test_ccc_core
    test_ccc_core.c
)
target_link_libraries(test_ccc_core
    DKCS
    mbedcrypto
    sm_crypto
)
target_include_directories(test_ccc_core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
add_test(NAME test_ccc_core COMMAND test_ccc_core)

add_executable(test_ccc_keymgmt
    test_ccc_keymgmt.c
)
target_link_libraries(test_ccc_keymgmt
    DKCS
    mbedcrypto
    sm_crypto
)
target_include_directories(test_ccc_keymgmt
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
add_test(NAME test_ccc_keymgmt COMMAND test_ccc_keymgmt)
