cmake_minimum_required(VERSION 3.20)
project(morze_signaling_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

include(CTest)

if (BUILD_TESTING)
  find_package(GTest REQUIRED)
  include(GoogleTest)

  add_executable(signaling_server_tests
    tests/signaling_server_tests.cpp
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
    tests/room_registry_tests.cpp
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
endif()
