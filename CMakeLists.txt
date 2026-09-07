cmake_minimum_required(VERSION 3.20)
project(spiral_os_home VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HOME_BUILD_TESTS "Build HOME tests" ON)

add_library(home_core
    src/world_types.cpp
)
add_library(HOME::Core ALIAS home_core)

target_include_directories(home_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(home_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(home_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(home_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(HOME_BUILD_TESTS)
    enable_testing()
    add_executable(home_l1_tests tests/l1_foundation_tests.cpp)
    target_link_libraries(home_l1_tests PRIVATE HOME::Core)
    add_test(NAME home.l1.foundation COMMAND home_l1_tests)
endif()
