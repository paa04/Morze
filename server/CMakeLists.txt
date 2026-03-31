cmake_minimum_required(VERSION 3.20)
project(morze_signaling_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SIGNALING_ENABLE_POSTGRES "Build PostgreSQL repository integration" OFF)

find_package(Boost 1.83 REQUIRED COMPONENTS system json)
find_package(Threads REQUIRED)

add_executable(signaling_server
  src/main.cpp
  src/signaling_server.cpp
  src/domain/room_registry.cpp
  src/application/protocol.cpp
  src/application/message_handler.cpp
  src/infrastructure/ws_session.cpp
  src/infrastructure/listener.cpp
)

target_include_directories(signaling_server
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(signaling_server
  PRIVATE
    Boost::system
    Boost::json
    Threads::Threads
)

target_compile_options(signaling_server PRIVATE -Wall -Wextra -Wpedantic)

if (SIGNALING_ENABLE_POSTGRES)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)

  add_library(signaling_postgres_store
    src/infrastructure/persistence/postgres_room_store.cpp
  )

  target_include_directories(signaling_postgres_store
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  target_link_libraries(signaling_postgres_store
    PRIVATE
      PkgConfig::PQXX
  )

  target_compile_options(signaling_postgres_store PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if (BUILD_TESTING)
  find_package(GTest REQUIRED)
  include(GoogleTest)

  add_executable(signaling_server_tests
          tests/units/signaling_server_tests.cpp
    src/application/protocol.cpp
  )

  target_include_directories(signaling_server_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  target_link_libraries(signaling_server_tests
    PRIVATE
      Boost::system
      Boost::json
      Threads::Threads
      GTest::gtest_main
  )

  target_compile_options(signaling_server_tests PRIVATE -Wall -Wextra -Wpedantic)

  gtest_discover_tests(signaling_server_tests)

  add_executable(room_registry_tests
    tests/units/room_registry_tests.cpp
    src/domain/room_registry.cpp
  )

  target_include_directories(room_registry_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  target_link_libraries(room_registry_tests
    PRIVATE
      Threads::Threads
      GTest::gtest_main
  )

  target_compile_options(room_registry_tests PRIVATE -Wall -Wextra -Wpedantic)

  gtest_discover_tests(room_registry_tests)

  add_executable(signaling_e2e_tests
          tests/e2e_signaling/signaling_tests.cpp
          tests/e2e_signaling/fixtures/signaling_e2e_fixture.cpp
          tests/e2e_signaling/clients/ws_test_client.cpp
    src/signaling_server.cpp
    src/domain/room_registry.cpp
    src/application/protocol.cpp
    src/application/message_handler.cpp
    src/infrastructure/ws_session.cpp
    src/infrastructure/listener.cpp
  )

  target_include_directories(signaling_e2e_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e_signaling
  )

  target_link_libraries(signaling_e2e_tests
    PRIVATE
      Boost::system
      Boost::json
      Threads::Threads
      GTest::gtest_main
  )

  target_compile_options(signaling_e2e_tests PRIVATE -Wall -Wextra -Wpedantic)

  add_test(
    NAME signaling_e2e_happy_path
    COMMAND signaling_e2e_tests --gtest_filter=TestFixture.HappyPathJoin

  )
  set_tests_properties(signaling_e2e_happy_path PROPERTIES LABELS e2e)

  add_test(
          NAME signaling_e2e_left_test
          COMMAND signaling_e2e_tests --gtest_filter=TestFixture.LeaveTest
  )

  set_tests_properties(signaling_e2e_left_test PROPERTIES LABELS e2e)

  add_test(
          NAME signaling_e2e_full_pos
          COMMAND signaling_e2e_tests --gtest_filter=TestFixture.FullPathTest
  )

  set_tests_properties(signaling_e2e_full_pos PROPERTIES LABELS e2e)

endif()
