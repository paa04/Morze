cmake_minimum_required(VERSION 3.20)
project(morze_signaling_server LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Старый FindBoost (CMake 3.30+), FetchContent::Populate (CMP0169) — тихо для -Wno-dev / Qt.
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

# Короткий путь к FetchContent на Windows (такой же, как в client/).
if(WIN32 AND NOT FETCHCONTENT_BASE_DIR)
  set(FETCHCONTENT_BASE_DIR "C:/_morze_fc" CACHE PATH "Base dir for FetchContent")
endif()

include(FetchContent)
find_package(Threads REQUIRED)

# 1) Системный Boost, если есть. 2) Иначе — скачать архив (без vcpkg/MSYS2).
option(MORZE_PREFER_SYSTEM_BOOST "Искать Boost в системе (find_package) перед авто-загрузкой" ON)
if(MORZE_PREFER_SYSTEM_BOOST)
  # MODULE — классический FindBoost (на Linux в apt; на Windows сможет сработать при vcpkg).
  find_package(Boost 1.83 QUIET COMPONENTS system json)
endif()
if(Boost_FOUND)
  add_library(morze_server_boost_iface INTERFACE)
  target_link_libraries(morze_server_boost_iface INTERFACE Boost::system Boost::json)
  if(WIN32)
    target_link_libraries(morze_server_boost_iface INTERFACE ws2_32 mswsock)
  endif()
else()
  message(STATUS
    "Boost не найден через find_package: загружаю boost_1_86_0.tar.gz (требуется сеть, ~180 MB)")

  FetchContent_Declare(
    morze_boost_headers
    URL https://archives.boost.org/release/1.86.0/source/boost_1_86_0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_GetProperties(morze_boost_headers)
  if(NOT morze_boost_headers_POPULATED)
    FetchContent_Populate(morze_boost_headers)
  endif()
  if(NOT EXISTS "${morze_boost_headers_SOURCE_DIR}/boost/version.hpp")
    message(FATAL_ERROR "Скачанный Boost неполный: нет ${morze_boost_headers_SOURCE_DIR}/boost/version.hpp")
  endif()

  add_library(morze_server_boost_fetched INTERFACE)
  target_include_directories(
    morze_server_boost_fetched
    SYSTEM INTERFACE "${morze_boost_headers_SOURCE_DIR}"
  )
  target_compile_definitions(
    morze_server_boost_fetched
    INTERFACE
    BOOST_ERROR_CODE_HEADER_ONLY=1
  )
  if(WIN32)
    # ws2_32: Winsock2; mswsock: AcceptEx, GetAcceptExSockaddrs (Boost.Beast/Asio)
    target_link_libraries(morze_server_boost_fetched INTERFACE ws2_32 mswsock)
  endif()

  add_library(morze_boost_json_impl STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost_json_one.cpp
  )
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(morze_boost_json_impl PRIVATE -w)
  elseif(MSVC)
    target_compile_options(morze_boost_json_impl PRIVATE /W0)
  endif()
  target_link_libraries(morze_boost_json_impl PUBLIC morze_server_boost_fetched)

  add_library(morze_server_boost_iface INTERFACE)
  target_link_libraries(
    morze_server_boost_iface
    INTERFACE
    morze_server_boost_fetched
    morze_boost_json_impl
  )
endif()

# SQLite3: find_package или amalgamation, как в client.
option(MORZE_PREFER_SYSTEM_SQLITE "Искать SQLite3 (find_package) перед авто-загрузкой" ON)
if(MORZE_PREFER_SYSTEM_SQLITE)
  find_package(SQLite3 QUIET)
endif()
if(NOT SQLite3_FOUND)
  message(STATUS "SQLite3 не найден: загружаю sqlite-amalgamation")
  FetchContent_Declare(
    morze_sqlite3
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3530000.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_GetProperties(morze_sqlite3)
  if(NOT morze_sqlite3_POPULATED)
    FetchContent_Populate(morze_sqlite3)
  endif()
  set(_MORZE_SQLITE3_C "${morze_sqlite3_SOURCE_DIR}/sqlite3.c")
  if(NOT EXISTS "${_MORZE_SQLITE3_C}")
    file(GLOB _morze_sql_candidates "${morze_sqlite3_SOURCE_DIR}/*/sqlite3.c")
    if(_morze_sql_candidates)
      list(GET _morze_sql_candidates 0 _MORZE_SQLITE3_C)
    endif()
  endif()
  if(NOT EXISTS "${_MORZE_SQLITE3_C}")
    message(FATAL_ERROR "Файл sqlite3.c не найден в скачанном sqlite-amalgamation")
  endif()
  get_filename_component(_morze_sql_inc "${_MORZE_SQLITE3_C}" DIRECTORY)
  add_library(morze_sqlite3 STATIC "${_MORZE_SQLITE3_C}")
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(morze_sqlite3 PRIVATE -w)
  endif()
  target_include_directories(morze_sqlite3 PUBLIC "${_morze_sql_inc}")
  if(NOT TARGET SQLite::SQLite3)
    add_library(SQLite::SQLite3 ALIAS morze_sqlite3)
  endif()
endif()

# Header-only ORM, без find_package внутри проекта.
FetchContent_Declare(
  sqlite_orm
  GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
  GIT_TAG        v1.9
)
FetchContent_GetProperties(sqlite_orm)
if(NOT sqlite_orm_POPULATED)
  FetchContent_Populate(sqlite_orm)
endif()
add_library(morze_sqlite_orm INTERFACE)
target_include_directories(morze_sqlite_orm INTERFACE ${sqlite_orm_SOURCE_DIR}/include)
if(NOT TARGET sqlite_orm::sqlite_orm)
  add_library(sqlite_orm::sqlite_orm ALIAS morze_sqlite_orm)
endif()

# --- shared library with server core + sqlite store ---
add_library(signaling_core STATIC
  src/signaling_server.cpp
  src/domain/room_registry.cpp
  src/application/protocol.cpp
  src/application/message_handler.cpp
  src/infrastructure/ws_session.cpp
  src/infrastructure/listener.cpp
  src/infrastructure/persistence/sqlite_room_store.cpp
)

target_include_directories(signaling_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(signaling_core
  PUBLIC
    morze_server_boost_iface
    Threads::Threads
    SQLite::SQLite3
    sqlite_orm::sqlite_orm
)

target_compile_options(signaling_core PRIVATE -Wall -Wextra -Wpedantic)

# --- main executable ---
add_executable(signaling_server src/main.cpp)
target_link_libraries(signaling_server PRIVATE signaling_core)
target_compile_options(signaling_server PRIVATE -Wall -Wextra -Wpedantic)

# P2P E2E: libdatachannel (rtc/peerconnection.hpp) — тяжёлая зависимость; внешний /usr/local/lib
# в типичной среде Windows нет, поэтому по умолчанию выкл. Вкл: -DMORZE_BUILD_P2P_E2E=ON
option(
  MORZE_BUILD_P2P_E2E
  "Собрать e2e-тесты P2P-чата (тянет libdatachannel; на Win нужен OpenSSL, см. client/CMakeLists.txt)"
  OFF
)

include(CTest)

if (BUILD_TESTING)
  find_package(GTest QUIET)
  if(NOT GTest_FOUND AND NOT TARGET GTest::gtest_main)
    message(STATUS
      "GTest not on PATH: FetchContent googletest (needs network, first time slower)")
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      morze_googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.15.2
      GIT_SHALLOW     TRUE
    )
    FetchContent_MakeAvailable(morze_googletest)
  endif()
  if(NOT TARGET GTest::gtest_main)
    message(FATAL_ERROR
      "GTest: install Google Test or set BUILD_TESTING=OFF (see CMake cache)")
  endif()
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
      morze_server_boost_iface
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

  # --- e2e: p2p chat tests (opt-in, libdatachannel) ---
  if(MORZE_BUILD_P2P_E2E)
    if(NOT TARGET datachannel)
      message(STATUS "MORZE_BUILD_P2P_E2E: FetchContent libdatachannel (долгая первая сборка)")
      set(NO_QT ON CACHE BOOL "Disable Qt in libdatachannel" FORCE)
      set(NO_MEDIA ON CACHE BOOL "Disable media in libdatachannel" FORCE)
      if(WIN32 AND NOT DEFINED OPENSSL_ROOT_DIR)
        set(OPENSSL_ROOT_DIR "C:/msys64/mingw64" CACHE PATH "OpenSSL (MSYS2) for libdatachannel" FORCE)
      endif()
      if(WIN32)
        set(OPENSSL_USE_STATIC_LIBS FALSE CACHE BOOL "" FORCE)
      endif()
      FetchContent_Declare(
        morze_server_libdatachannel
        GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
        GIT_TAG v0.24.2
        GIT_CONFIG core.longpaths=true
      )
      FetchContent_MakeAvailable(morze_server_libdatachannel)
    endif()
    if(NOT TARGET datachannel)
      message(FATAL_ERROR "MORZE_BUILD_P2P_E2E: libdatachannel не дала target datachannel")
    endif()

    add_library(p2p_chat_e2e_clients
      tests/e2e_p2p_chat/clients/p2p_chat_test_client.cpp
    )

    target_include_directories(p2p_chat_e2e_clients
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e_p2p_chat
    )

    if(NOT WIN32)
      target_link_directories(p2p_chat_e2e_clients PRIVATE /usr/local/lib)
    endif()

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
  endif()
endif()
