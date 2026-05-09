# Fuzzing tests
if(ENABLE_FUZZING)
    find_package(PkgConfig REQUIRED)
    
    add_executable(fuzz_dkcs fuzz_dkcs.c)
    target_link_libraries(fuzz_dkcs dkcs)
    
    add_executable(fuzz_ccc fuzz_ccc.c)
    target_link_libraries(fuzz_ccc ccc)
    
    add_executable(fuzz_scp03 fuzz_scp03.c)
    target_link_libraries(fuzz_scp03 scp03)
endif()
