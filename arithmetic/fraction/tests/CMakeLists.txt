add_executable(meow_test_fraction meow_test_fraction.cpp)

target_link_libraries(
        meow_test_fraction
        PRIVATE
        gtest_main)
target_link_libraries(
        meow_test_fraction
        PRIVATE
        mp_os_lggr_clnt_lggr)
target_link_libraries(
        meow_test_fraction
        PUBLIC
        mp_os_arthmtc_frctn)