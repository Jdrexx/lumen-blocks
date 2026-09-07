cmake_minimum_required(VERSION 3.22)
project(lumen_blocks_rules_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Host-side tests for the pure game rules (no Android SDK/NDK required).
add_executable(game_rules_test game_rules_test.cpp)
target_include_directories(game_rules_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp)
enable_testing()
add_test(NAME game_rules_test COMMAND game_rules_test)
