cmake_minimum_required(VERSION 3.20)
project(morze_signaling_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost 1.83 REQUIRED COMPONENTS system json)
find_package(Threads REQUIRED)

add_executable(signaling_server
  src/main.cpp
  src/signaling_server.cpp
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
    src/signaling_server.cpp
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
endif()
