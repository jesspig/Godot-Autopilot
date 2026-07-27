add_executable(test_log_system
    test_log_system.cpp
    ${CMAKE_SOURCE_DIR}/src/core/log_system.cpp)

target_link_libraries(test_log_system PRIVATE
    gtest_main
    Threads::Threads)

target_include_directories(test_log_system PRIVATE
    ${CMAKE_SOURCE_DIR}/src)

add_test(NAME test_log_system COMMAND test_log_system)
