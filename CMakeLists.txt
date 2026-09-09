cmake_minimum_required(VERSION 3.20)
project(spiral_os_home VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(HOME_BUILD_TESTS "Build HOME tests" ON)
add_library(home_core
 src/world_types.cpp
 src/entity_registry.cpp
 src/topology.cpp
 src/versioned_world.cpp
 src/snapshot_v12.cpp
 src/persistence.cpp
 src/world_clock.cpp
 src/versioned_world_clock.cpp
 src/calendar.cpp
 src/temporal_domain.cpp
 src/events.cpp
 src/weather.cpp
 src/world_affect.cpp
 src/versioned_world_affect.cpp
 src/player_life.cpp
 src/versioned_world_life.cpp
 src/needs_mood_autonomy.cpp
 src/versioned_world_dynamics.cpp
 src/relationships.cpp
 src/versioned_world_social.cpp
 src/inventory.cpp
 src/versioned_world_inventory.cpp
 src/progression.cpp
 src/versioned_world_progression.cpp
 src/theorism.cpp
 src/versioned_world_theorism.cpp
)
add_library(HOME::Core ALIAS home_core)
target_include_directories(home_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_compile_features(home_core PUBLIC cxx_std_20)
if(MSVC)
 target_compile_options(home_core PRIVATE /W4 /permissive-)
else()
 target_compile_options(home_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(HOME_BUILD_TESTS)
 enable_testing()
 function(home_add_test level source test_name)
  add_executable(home_l${level}_tests tests/${source})
  target_link_libraries(home_l${level}_tests PRIVATE HOME::Core)
  if(MSVC)
   target_compile_options(home_l${level}_tests PRIVATE /UNDEBUG)
  else()
   target_compile_options(home_l${level}_tests PRIVATE -UNDEBUG)
  endif()
  add_test(NAME ${test_name} COMMAND home_l${level}_tests)
 endfunction()
 home_add_test(1 l1_foundation_tests.cpp home.l1.foundation)
 home_add_test(2 l2_entity_registry_tests.cpp home.l2.entity_registry)
 home_add_test(3 l3_world_delta_tests.cpp home.l3.world_deltas)
 home_add_test(4 l4_topology_tests.cpp home.l4.topology)
 home_add_test(5 l5_transaction_tests.cpp home.l5.transactions)
 home_add_test(6 l6_persistence_tests.cpp home.l6.persistence)
 home_add_test(7 l7_world_clock_tests.cpp home.l7.world_clock)
 home_add_test(8 l8_calendar_tests.cpp home.l8.calendar)
 home_add_test(9 l9_temporal_domain_tests.cpp home.l9.temporal_domains)
 home_add_test(10 l10_events_tests.cpp home.l10.events)
 home_add_test(11 l11_weather_tests.cpp home.l11.weather)
 home_add_test(12 l12_world_affect_tests.cpp home.l12.world_affect)
 home_add_test(13 l13_player_life_tests.cpp home.l13.player_life)
 home_add_test(14 l14_needs_mood_autonomy_tests.cpp home.l14.needs_mood_autonomy)
 home_add_test(15 l15_relationships_households_tests.cpp home.l15.relationships_households)
 home_add_test(16 l16_items_inventory_tests.cpp home.l16.items_inventory)
 home_add_test(17 l17_skills_tasks_quests_tests.cpp home.l17.skills_tasks_quests)
 home_add_test(18 l18_subclass_theorism_aura_tests.cpp home.l18.subclass_theorism_aura)
endif()
