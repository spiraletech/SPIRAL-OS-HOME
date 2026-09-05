cmake_minimum_required(VERSION 3.20)

project(spiral_os_home LANGUAGES CXX)

add_library(spiral_world_contract INTERFACE)
target_include_directories(spiral_world_contract INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(spiral_world_contract INTERFACE cxx_std_20)

option(SPIRAL_HOME_BUILD_TESTS "Build Spiral HOME contract tests" ON)

if(SPIRAL_HOME_BUILD_TESTS)
    enable_testing()
    add_executable(world_contract_smoke tests/world_contract_smoke.cpp)
    target_link_libraries(world_contract_smoke PRIVATE spiral_world_contract)
    add_test(NAME world_contract_smoke COMMAND world_contract_smoke)
endif()
