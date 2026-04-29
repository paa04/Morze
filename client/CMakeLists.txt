cmake_minimum_required(VERSION 3.20)
project(Morze)
include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
#set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/qt6" CACHE PATH "Qt6 prefix path")

FetchContent_Declare(
        sqlite3
        URL https://www.sqlite.org/2026/sqlite-amalgamation-3530000.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3)
set(SQLITE3_C_FILE "${sqlite3_SOURCE_DIR}/sqlite3.c")
if(NOT EXISTS "${SQLITE3_C_FILE}")
    file(GLOB SQLITE3_C_CANDIDATES "${sqlite3_SOURCE_DIR}/*/sqlite3.c")
    list(LENGTH SQLITE3_C_CANDIDATES SQLITE3_C_COUNT)
    if(SQLITE3_C_COUNT GREATER 0)
        list(GET SQLITE3_C_CANDIDATES 0 SQLITE3_C_FILE)
    endif()
endif()
if(NOT EXISTS "${SQLITE3_C_FILE}")
    message(FATAL_ERROR "sqlite3.c not found in fetched sqlite3 sources: ${sqlite3_SOURCE_DIR}")
endif()
add_library(sqlite3 STATIC "${SQLITE3_C_FILE}")
get_filename_component(SQLITE3_INCLUDE_DIR "${SQLITE3_C_FILE}" DIRECTORY)
target_include_directories(sqlite3 PUBLIC "${SQLITE3_INCLUDE_DIR}")

FetchContent_Declare(
        sqlite_orm
        GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
        GIT_TAG        v1.9.1 # Или любой другой тег/коммит, который вы предпочитаете
)
FetchContent_GetProperties(sqlite_orm)
if(NOT sqlite_orm_POPULATED)
    FetchContent_Populate(sqlite_orm)
endif()
add_library(sqlite_orm INTERFACE)
target_include_directories(sqlite_orm INTERFACE ${sqlite_orm_SOURCE_DIR}/include)
add_library(sqlite_orm::sqlite_orm ALIAS sqlite_orm)

if(WIN32 AND NOT DEFINED OPENSSL_ROOT_DIR)
    # Стандартный путь OpenSSL для MSYS2 MinGW64.
    set(OPENSSL_ROOT_DIR "C:/msys64/mingw64" CACHE PATH "OpenSSL root directory")
endif()
if(WIN32)
    set(OPENSSL_USE_STATIC_LIBS FALSE CACHE BOOL "Use dynamic OpenSSL libs" FORCE)
endif()

find_package(LibDataChannel QUIET)
if(NOT LibDataChannel_FOUND)
    message(STATUS "System libdatachannel not found, fetching from source...")
    set(NO_QT ON CACHE BOOL "Disable Qt support in libdatachannel" FORCE)
    set(CMAKE_AUTOMOC OFF)
    FetchContent_Declare(
            libdatachannel
            GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
            GIT_TAG v0.24.2
    )
    FetchContent_MakeAvailable(libdatachannel)
    set(CMAKE_AUTOMOC ON)
    # FetchContent target is "datachannel", alias it to match system package
    if(NOT TARGET LibDataChannel::LibDataChannel)
        add_library(LibDataChannel::LibDataChannel ALIAS datachannel)
    endif()
else()
    message(STATUS "Using system libdatachannel")
endif()

FetchContent_Declare(
        cpm
        GIT_REPOSITORY https://github.com/cpm-cmake/CPM.cmake
        GIT_TAG v0.42.1
)
FetchContent_MakeAvailable(cpm)

FetchContent_Declare(
        boost_headers
        URL https://archives.boost.io/release/1.90.0/source/boost_1_90_0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(boost_headers)
add_library(Boost_headers INTERFACE)
target_include_directories(Boost_headers INTERFACE ${boost_headers_SOURCE_DIR})
target_compile_definitions(Boost_headers INTERFACE BOOST_ASIO_STANDALONE BOOST_ASIO_HAS_CO_AWAIT)
add_library(Boost::uuid ALIAS Boost_headers)
add_library(Boost::asio ALIAS Boost_headers)
add_library(Boost::beast ALIAS Boost_headers)

find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    # libdatachannel (when fetched) bundles its own nlohmann_json target
    if(TARGET nlohmann_json)
        message(STATUS "Using nlohmann_json bundled with libdatachannel")
    else()
        message(STATUS "System nlohmann_json not found, fetching from source...")
        FetchContent_Declare(
                json
                GIT_REPOSITORY https://github.com/nlohmann/json.git
                GIT_TAG v3.11.3
        )
        FetchContent_MakeAvailable(json)
    endif()
else()
    message(STATUS "Using system nlohmann_json")
endif()

# Boost headers-only target настроен выше.



find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)

add_executable(Morze main.cpp
        repositories/dbContext/Database.cpp
        repositories/dbContext/Database.h
        repositories/DAO/ChatDAO.h
        repositories/DAO/ChatMemberDAO.h
        repositories/DAO/MessageDAO.h
        globalEnums/GlobalEnums.h
        repositories/DAO/ConnectionProfileDAO.h
        repositories/DBConfiguration.h
        globalEnums/MessageDirectionConverter.h
        globalEnums/DeliveryStateConverter.h
        globalEnums/ChatTypeConverter.h
        globalConverters/TimePointConverter.h
        globalConverters/UUIDConverter.h
        config/ConfigManager.cpp
        config/ConfigManager.h
        repositories/DAO/converters/ChatDAOConverter.h
        services/Models/ChatMemberModel.h
        services/Models/MessageModel.h
        services/Models/ConnectionProfileModel.h
        repositories/DAO/converters/ChatMemberDAOConverter.h
        repositories/DAO/converters/ConnectionProfileDAOConverter.h
        repositories/DAO/converters/MessageDAOConverter.h
        exceptions/ChatExcpetions.h
        repositories/repositories/ChatRepository.cpp
        repositories/repositories/ChatRepository.h
        exceptions/ChatMemberExceptions.h
        repositories/repositories/ChatMemberRepository.cpp
        repositories/repositories/ChatMemberRepository.h
        exceptions/ConnectionProfileExceptions.h
        repositories/repositories/ConnectionProfileRepository.cpp
        repositories/repositories/ConnectionProfileRepository.h
        exceptions/MessageExceptions.h
        repositories/repositories/MessageRepository.cpp
        repositories/repositories/MessageRepository.h
        services/Models/UserModel.h
        services/Models/UserModel.h
        repositories/DAO/UserDAO.h
        repositories/DAO/UserDAO.h
        services/Models/UserModel.h
        repositories/DAO/converters/UserDAOConverter.h
        exceptions/UserExceptions.h
        repositories/repositories/UserRepository.cpp
        repositories/repositories/UserRepository.h
        services/services/ChatService.cpp
        services/services/ChatService.h
        services/services/ChatMemberService.cpp
        services/services/ChatMemberService.h
        services/services/MessageService.cpp
        services/services/MessageService.h
        services/services/ChatMemberService.h
        services/services/UserService.cpp
        services/services/UserService.h
        services/services/SignalingService.cpp
        services/services/SignalingService.h
        services/services/WebRTCService.cpp
        services/services/WebRTCService.h
        services/services/ConnectionProfileService.cpp
        services/services/ConnectionProfileService.h
        controllers/DTO/ChatDTO.h
        controllers/DTO/ChatMemberDTO.h
        controllers/DTO/ChatMemberDTO.h
        controllers/DTO/MessageDTO.h
        controllers/DTO/ConnectionProfileDTO.h
        controllers/DTO/UserDTO.h
        controllers/controllers/CommunicationController.cpp
        controllers/controllers/CommunicationController.h
        controllers/DTO/converters/ChatDTOConverter.h
        controllers/DTO/converters/ChatMemberDTOConverter.h
        controllers/DTO/converters/MessageDTOConverter.h
        controllers/DTO/converters/ConnectionProfileDTOConverter.h
        controllers/DTO/converters/UserDTOConverter.h
        DI/DependencyContainer.cpp
        DI/DependencyContainer.h)

target_include_directories(Morze PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/repositories/DAO
        ${CMAKE_CURRENT_SOURCE_DIR}/repositories/DAO/converters
        ${CMAKE_CURRENT_SOURCE_DIR}/repositories/repositories
        ${CMAKE_CURRENT_SOURCE_DIR}/repositories/dbContext
        ${CMAKE_CURRENT_SOURCE_DIR}/repositories
        ${CMAKE_CURRENT_SOURCE_DIR}/globalEnums
        ${CMAKE_CURRENT_SOURCE_DIR}/globalConverters
        ${CMAKE_CURRENT_SOURCE_DIR}/services/Models
        ${CMAKE_CURRENT_SOURCE_DIR}/services
        ${CMAKE_CURRENT_SOURCE_DIR}/exceptions
        ${CMAKE_CURRENT_SOURCE_DIR}/services/services
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/DTO
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/DTO/converters
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/controllers
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        ${CMAKE_CURRENT_SOURCE_DIR}/DI
        ${Qt6_INCLUDE_DIRS}
)

target_link_libraries(Morze PRIVATE
        sqlite_orm::sqlite_orm
        sqlite3
        Boost::uuid
        Boost::asio
        nlohmann_json::nlohmann_json
        Boost::beast
        LibDataChannel::LibDataChannel
        Qt6::Core
        Qt6::Network
        Qt6::WebSockets
)

# Copy config.json next to the binary so it's always found
add_custom_command(TARGET Morze POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.json
        $<TARGET_FILE_DIR:Morze>/config.json)

if(WIN32)
    target_link_libraries(Morze PRIVATE ws2_32)

    # Runtime OpenSSL DLLs needed by libdatachannel on Windows.
    set(_MORZE_OPENSSL_DIRS "")
    if(DEFINED OPENSSL_ROOT_DIR AND NOT OPENSSL_ROOT_DIR STREQUAL "")
        list(APPEND _MORZE_OPENSSL_DIRS "${OPENSSL_ROOT_DIR}/bin" "${OPENSSL_ROOT_DIR}/lib")
    endif()
    list(APPEND _MORZE_OPENSSL_DIRS "C:/msys64/mingw64/bin")
    list(REMOVE_DUPLICATES _MORZE_OPENSSL_DIRS)

    foreach(_MORZE_SSL_DIR IN LISTS _MORZE_OPENSSL_DIRS)
        if(EXISTS "${_MORZE_SSL_DIR}/libssl-3-x64.dll")
            add_custom_command(TARGET Morze POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_MORZE_SSL_DIR}/libssl-3-x64.dll"
                $<TARGET_FILE_DIR:Morze>/libssl-3-x64.dll)
        endif()
        if(EXISTS "${_MORZE_SSL_DIR}/libcrypto-3-x64.dll")
            add_custom_command(TARGET Morze POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_MORZE_SSL_DIR}/libcrypto-3-x64.dll"
                $<TARGET_FILE_DIR:Morze>/libcrypto-3-x64.dll)
        endif()
    endforeach()
endif()

# ------------------- GUI подпроект -------------------
option(BUILD_GUI_SUBPROJECT "Build Qt Quick GUI subproject" ON)
if(BUILD_GUI_SUBPROJECT)
    find_package(Qt6 QUIET COMPONENTS Quick)
    if(Qt6Quick_FOUND)
        add_subdirectory(gui)
    else()
        message(STATUS "Qt6Quick not found — skipping GUI subproject")
    endif()
endif()

# ------------------- Тесты -------------------
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    # Загружаем doctest через FetchContent
    FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.5.2
    )
    FetchContent_MakeAvailable(doctest)

    # Исходники тестов
    set(TEST_SOURCES
            tests/TestMain.cpp
            repositories/repositories/ChatRepository.cpp
            repositories/repositories/ChatRepository.h
            tests/repository/TestChatRepository.cpp
            repositories/repositories/ChatRepository.cpp
            repositories/repositories/ChatMemberRepository.cpp
            tests/repository/TestChatMemberRepository.cpp
            repositories/repositories/ConnectionProfileRepository.cpp
            repositories/repositories/ConnectionProfileRepository.h
            tests/repository/TestConnectionProfileRepository.cpp
            repositories/repositories/MessageRepository.cpp
            repositories/repositories/MessageRepository.h
            tests/repository/TestMessageRepository.cpp
            repositories/repositories/UserRepository.cpp
            services/services/ChatService.cpp
            services/services/ChatService.h
            services/services/ChatMemberService.cpp
            services/services/ChatMemberService.h
            services/services/MessageService.cpp
            services/services/MessageService.h
            services/services/ConnectionProfileService.cpp
            services/services/ConnectionProfileService.h
            services/services/UserService.cpp
            services/services/UserService.h
            services/services/SignalingService.cpp
            services/services/SignalingService.h
            services/services/WebRTCService.cpp
            services/services/WebRTCService.h
            controllers/DTO/ChatDTO.h
            controllers/DTO/ChatMemberDTO.h
            controllers/DTO/ChatMemberDTO.h
            controllers/DTO/MessageDTO.h
            controllers/DTO/ConnectionProfileDTO.h
            controllers/DTO/UserDTO.h
            controllers/controllers/CommunicationController.cpp
            controllers/controllers/CommunicationController.h
            controllers/DTO/converters/ChatDTOConverter.h
            controllers/DTO/converters/ChatMemberDTOConverter.h
            controllers/DTO/converters/MessageDTOConverter.h
            controllers/DTO/converters/ConnectionProfileDTOConverter.h
            controllers/DTO/converters/UserDTOConverter.h
            tests/repository/TestUserRepository.cpp
            tests/service/TestChatMemberService.cpp
            tests/service/TestChatService.cpp
            tests/service/TestConfigurationProfile.cpp
            tests/service/TestMessageService.cpp
            tests/service/TestUserService.cpp
    )

    add_executable(MorzeTests ${TEST_SOURCES})

    # Добавляем пути к заголовкам основного проекта (чтобы тесты видели репозитории и модели)
    target_include_directories(MorzeTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/repositories/DAO
            ${CMAKE_CURRENT_SOURCE_DIR}/repositories/DAO/converters
            ${CMAKE_CURRENT_SOURCE_DIR}/repositories/repositories
            ${CMAKE_CURRENT_SOURCE_DIR}/repositories/dbContext
            ${CMAKE_CURRENT_SOURCE_DIR}/repositories
            ${CMAKE_CURRENT_SOURCE_DIR}/globalEnums
            ${CMAKE_CURRENT_SOURCE_DIR}/globalConverters
            ${CMAKE_CURRENT_SOURCE_DIR}/services/Models
            ${CMAKE_CURRENT_SOURCE_DIR}/services
            ${CMAKE_CURRENT_SOURCE_DIR}/services/services
            ${CMAKE_CURRENT_SOURCE_DIR}/config
            ${CMAKE_CURRENT_SOURCE_DIR}/exceptions
            ${CMAKE_CURRENT_SOURCE_DIR}/controllers/controllers
            ${CMAKE_CURRENT_SOURCE_DIR}/controllers/DTO
            ${CMAKE_CURRENT_SOURCE_DIR}/controllers/DTO/converters
            ${Qt6_INCLUDE_DIRS}
    )

    # Линкуем всё то же, что и основное приложение + doctest
    target_link_libraries(MorzeTests PRIVATE
            sqlite_orm::sqlite_orm
            sqlite3
            Boost::uuid
            Boost::asio
            Boost::beast
            nlohmann_json::nlohmann_json
            doctest::doctest
            LibDataChannel::LibDataChannel
            Qt6::Core
            Qt6::Network
            Qt6::WebSockets
    )

    # Включаем CTest для интеграции с IDE
    include(CTest)
    add_test(NAME MorzeTests COMMAND MorzeTests)
endif()