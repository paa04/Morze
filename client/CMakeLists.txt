cmake_minimum_required(VERSION 4.1)
project(Morze)
include(FetchContent)

set(CMAKE_CXX_STANDARD 23)

FetchContent_Declare(
        sqlite3
        URL https://www.sqlite.org/2026/sqlite-amalgamation-3530000.zip
)
FetchContent_MakeAvailable(sqlite3)

FetchContent_Declare(
        sqlite_orm
        GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
        GIT_TAG        v1.9.1 # Или любой другой тег/коммит, который вы предпочитаете
)
FetchContent_MakeAvailable(sqlite_orm)

FetchContent_Declare(
        libdatachannel
        GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
        GIT_TAG v0.24.2
)
FetchContent_MakeAvailable(libdatachannel)

FetchContent_Declare(
        cpm
        GIT_REPOSITORY https://github.com/cpm-cmake/CPM.cmake
        GIT_TAG v0.42.1
)
FetchContent_MakeAvailable(cpm)

set(BOOST_JSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BOOST_JSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

function(add_boost_library NAME REPO TAG)
    cmake_policy(SET CMP0169 OLD)
    FetchContent_Declare(
            boost_${NAME}
            GIT_REPOSITORY ${REPO}
            GIT_TAG ${TAG}
            GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(boost_${NAME})
    if(NOT boost_${NAME}_POPULATED)
        FetchContent_Populate(boost_${NAME})
    endif()
    add_library(Boost_${NAME} INTERFACE)
    target_include_directories(Boost_${NAME} INTERFACE ${boost_${NAME}_SOURCE_DIR}/include)
    add_library(Boost::${NAME} ALIAS Boost_${NAME})
endfunction()

# Явно добавляем каждую необходимую библиотеку Boost
add_boost_library(config            https://github.com/boostorg/config          boost-1.90.0)
add_boost_library(core              https://github.com/boostorg/core            boost-1.90.0)
add_boost_library(assert            https://github.com/boostorg/assert          boost-1.90.0)
add_boost_library(throw_exception   https://github.com/boostorg/throw_exception boost-1.90.0)
add_boost_library(move              https://github.com/boostorg/move            boost-1.90.0)
add_boost_library(type_traits       https://github.com/boostorg/type_traits     boost-1.90.0)
add_boost_library(predef            https://github.com/boostorg/predef          boost-1.90.0)
add_boost_library(static_assert     https://github.com/boostorg/static_assert   boost-1.90.0)
add_boost_library(winapi            https://github.com/boostorg/winapi          boost-1.90.0)
add_boost_library(detail            https://github.com/boostorg/detail          boost-1.90.0)
add_boost_library(system            https://github.com/boostorg/system          boost-1.90.0)
add_boost_library(uuid              https://github.com/boostorg/uuid            boost-1.90.0)
add_boost_library(asio              https://github.com/boostorg/asio            boost-1.90.0)
add_boost_library(beast             https://github.com/boostorg/beast           boost-1.90.0)

#FetchContent_Declare(
#        json
#        GIT_REPOSITORY https://github.com/nlohmann/json.git
#        GIT_TAG v3.11.3
#)
#FetchContent_MakeAvailable(json)

target_link_libraries(Boost_uuid INTERFACE
        Boost_config Boost_core Boost_assert Boost_throw_exception
        Boost_move Boost_type_traits Boost_predef Boost_static_assert
        Boost_winapi Boost_detail
)

target_link_libraries(Boost_asio INTERFACE
        Boost_config Boost_core Boost_assert Boost_system Boost_throw_exception
        Boost_predef Boost_detail
)


# Настройки для Asio (теперь цель Boost_asio уже создана)
target_compile_definitions(Boost_asio INTERFACE BOOST_ASIO_STANDALONE)
target_compile_definitions(Boost_asio INTERFACE BOOST_ASIO_HAS_CO_AWAIT)



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
        repositories/DAO/Converters/ChatDAOConverter.h
        services/Models/ChatMemberModel.h
        services/Models/MessageModel.h
        services/Models/ConnectionProfileModel.h
        repositories/DAO/Converters/ChatMemberDAOConverter.h
        repositories/DAO/Converters/ConnectionProfileDAOConverter.h
        repositories/DAO/Converters/MessageDAOConverter.h
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
        repositories/DAO/Converters/UserDAOConverter.h
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
        services/services/UserService.h)
target_include_directories(Morze PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/repositories/DAO
        ${CMAKE_CURRENT_SOURCE_DIR}/repositories/DAO/Converters
        ${CMAKE_CURRENT_SOURCE_DIR}/repositories/repositories
        ${CMAKE_CURRENT_SOURCE_DIR}/repositories
        ${CMAKE_CURRENT_SOURCE_DIR}/globalEnums
        ${CMAKE_CURRENT_SOURCE_DIR}/globalConverters
        ${CMAKE_CURRENT_SOURCE_DIR}/services/Models
        ${CMAKE_CURRENT_SOURCE_DIR}/services
        ${CMAKE_CURRENT_SOURCE_DIR}/exceptions
        )

target_link_libraries(Morze PRIVATE
        sqlite_orm::sqlite_orm
        sqlite3
        Boost::uuid
        Boost::asio
        nlohmann_json#::nlohmann_json
        Boost::beast
        datachannel
)

if(WIN32)
    target_link_libraries(Morze PRIVATE ws2_32)
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
            tests/repository/TestChatRepository.cpp
            repositories/repositories/ChatRepository.cpp
            repositories/repositories/ChatMemberRepository.cpp
            tests/repository/TestChatMemberRepository.cpp
            tests/TestMain.cpp
            repositories/repositories/ConnectionProfileRepository.cpp
            repositories/repositories/ConnectionProfileRepository.h
            tests/repository/TestConnectionProfileRepository.cpp
            repositories/repositories/MessageRepository.cpp
            repositories/repositories/MessageRepository.h
            tests/repository/TestMessageRepository.cpp
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
    )

    add_executable(MorzeTests ${TEST_SOURCES})

    # Добавляем пути к заголовкам основного проекта (чтобы тесты видели репозитории и модели)
    target_include_directories(MorzeTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/repositories/DAO
            ${CMAKE_CURRENT_SOURCE_DIR}/repositories/DAO/Converters
            ${CMAKE_CURRENT_SOURCE_DIR}/repositories/repositories
            ${CMAKE_CURRENT_SOURCE_DIR}/repositories
            ${CMAKE_CURRENT_SOURCE_DIR}/globalEnums
            ${CMAKE_CURRENT_SOURCE_DIR}/globalConverters
            ${CMAKE_CURRENT_SOURCE_DIR}/services/Models
            ${CMAKE_CURRENT_SOURCE_DIR}/services
            ${CMAKE_CURRENT_SOURCE_DIR}/exceptions
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
            datachannel
    )

    # Включаем CTest для интеграции с IDE
    include(CTest)
    add_test(NAME MorzeTests COMMAND MorzeTests)
endif()