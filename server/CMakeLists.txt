cmake_minimum_required(VERSION 3.20)
project(morze_signaling_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.83 REQUIRED COMPONENTS system json)
find_package(Threads REQUIRED)

# --- sqlite_orm (header-only, fetched automatically) ---
include(FetchContent)
FetchContent_Declare(
  sqlite_orm
  GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
  GIT_TAG        v1.9
)
FetchContent_MakeAvailable(sqlite_orm)

find_package(SQLite3 REQUIRED)

# --- shared library with server core + sqlite store ---
add_library(signaling_core STATIC
  src/signaling_server.cpp
  src/domain/room_registry.cpp
  src/application/protocol.cpp
  src/application/message_handler.cpp
  src/infrastructure/ws_session.cpp
  src/infrastructure/http_detect_session.cpp
  src/infrastructure/listener.cpp
  src/infrastructure/persistence/sqlite_room_store.cpp
)

target_include_directories(signaling_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(signaling_core
  PUBLIC
    Boost::system
    Boost::json
    Threads::Threads
    SQLite::SQLite3
    sqlite_orm::sqlite_orm
)

target_compile_options(signaling_core PRIVATE -Wall -Wextra -Wpedantic)

# --- main executable ---
add_executable(signaling_server src/main.cpp)
target_link_libraries(signaling_server PRIVATE signaling_core)
target_compile_options(signaling_server PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)

if (BUILD_TESTING)
  find_package(GTest REQUIRED)
  include(GoogleTest)

  # --- unit: protocol tests ---
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

  # --- unit: room_registry tests ---
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

  # --- unit: sqlite_room_store tests ---
  add_executable(sqlite_room_store_tests
    tests/units/sqlite_room_store_tests.cpp
    src/infrastructure/persistence/sqlite_room_store.cpp
  )

  target_include_directories(sqlite_room_store_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  target_link_libraries(sqlite_room_store_tests
    PRIVATE
      Threads::Threads
      SQLite::SQLite3
      sqlite_orm::sqlite_orm
      GTest::gtest_main
  )

  target_compile_options(sqlite_room_store_tests PRIVATE -Wall -Wextra -Wpedantic)
  gtest_discover_tests(sqlite_room_store_tests)

  # --- e2e: signaling tests ---
  add_executable(signaling_e2e_tests
          tests/e2e_signaling/signaling_tests.cpp
          tests/e2e_signaling/fixtures/signaling_e2e_fixture.cpp
          tests/e2e_signaling/clients/ws_test_client.cpp
  )

  target_include_directories(signaling_e2e_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e_signaling
  )

  target_link_libraries(signaling_e2e_tests
    PRIVATE
      signaling_core
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

  # --- e2e: persistence tests ---
  add_executable(persistence_e2e_tests
    tests/e2e_signaling/persistence_tests.cpp
    tests/e2e_signaling/fixtures/signaling_e2e_fixture.cpp
    tests/e2e_signaling/clients/ws_test_client.cpp
  )

  target_include_directories(persistence_e2e_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e_signaling
  )

  target_link_libraries(persistence_e2e_tests
    PRIVATE
      signaling_core
      GTest::gtest_main
  )

  target_compile_options(persistence_e2e_tests PRIVATE -Wall -Wextra -Wpedantic)
  gtest_discover_tests(persistence_e2e_tests)

  # --- e2e: p2p chat tests ---
  add_library(p2p_chat_e2e_clients
    tests/e2e_p2p_chat/clients/p2p_chat_test_client.cpp
  )

  target_include_directories(p2p_chat_e2e_clients
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e_p2p_chat
  )

  target_link_directories(p2p_chat_e2e_clients
    PRIVATE
      /usr/local/lib
  )

  target_link_libraries(p2p_chat_e2e_clients
    PRIVATE
      Threads::Threads
      datachannel
  )

  target_compile_options(p2p_chat_e2e_clients PRIVATE -Wall -Wextra -Wpedantic)

  add_executable(p2p_chat_e2e_tests
    tests/e2e_p2p_chat/p2p_e2e_tests.cpp
    tests/e2e_p2p_chat/fixture/p2p_fixture.cpp
    tests/e2e_signaling/clients/ws_test_client.cpp
          tests/e2e_p2p_chat/group_e2e_tests.cpp
  )

  target_include_directories(p2p_chat_e2e_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e_p2p_chat
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e_signaling
  )

  target_link_libraries(p2p_chat_e2e_tests
    PRIVATE
      signaling_core
      GTest::gtest_main
      p2p_chat_e2e_clients
  )

  target_compile_options(p2p_chat_e2e_tests PRIVATE -Wall -Wextra -Wpedantic)

  add_test(
    NAME p2p_chat_e2e_happy_path
    COMMAND p2p_chat_e2e_tests --gtest_filter=TestFixtureP2p.happy_path_full
  )
  set_tests_properties(p2p_chat_e2e_happy_path PROPERTIES LABELS e2e)

  add_test(
    NAME group_chat_e2e_broadcast
    COMMAND p2p_chat_e2e_tests --gtest_filter=TestFixtureP2p.group_message_is_broadcast_to_other_members
  )
  set_tests_properties(group_chat_e2e_broadcast PROPERTIES LABELS e2e)

  add_test(
    NAME group_chat_e2e_buffered_messages
    COMMAND p2p_chat_e2e_tests --gtest_filter=TestFixtureP2p.offline_member_receives_buffered_messages_on_reconnect
  )
  set_tests_properties(group_chat_e2e_buffered_messages PROPERTIES LABELS e2e)

  # --- e2e: real client integration tests (requires Qt6) ---
  set(CLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../client)
  find_package(Qt6 QUIET COMPONENTS Core Network WebSockets)

  if (Qt6_FOUND AND EXISTS "${CLIENT_DIR}/services/services/SignalingService.cpp")
    add_executable(client_e2e_tests
      tests/e2e_client/main.cpp
      tests/e2e_client/fixture/client_e2e_fixture.cpp
      tests/e2e_client/signaling_service_tests.cpp
      tests/e2e_client/communication_controller_tests.cpp
      ${CLIENT_DIR}/services/services/SignalingService.cpp
      ${CLIENT_DIR}/services/services/WebRTCService.cpp
      ${CLIENT_DIR}/controllers/controllers/CommunicationController.cpp
    )

    set_target_properties(client_e2e_tests PROPERTIES AUTOMOC ON)

    target_include_directories(client_e2e_tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e_client
      ${CLIENT_DIR}/services/services
      ${CLIENT_DIR}/controllers/controllers
      ${CLIENT_DIR}/controllers/DTO
    )

    target_link_directories(client_e2e_tests PRIVATE /usr/local/lib)

    target_link_libraries(client_e2e_tests PRIVATE
      signaling_core
      GTest::gtest
      Qt6::Core
      Qt6::Network
      Qt6::WebSockets
      datachannel
    )

    target_compile_options(client_e2e_tests PRIVATE -Wall -Wextra -Wpedantic)
    gtest_discover_tests(client_e2e_tests)
  endif()

endif()
