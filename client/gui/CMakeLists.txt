cmake_minimum_required(VERSION 3.16)

project(Morze_ui VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Core Network QuickControls2 WebSockets)

if(TARGET sqlite3)
    set(GUI_SQLITE_TARGET sqlite3)
else()
    find_package(SQLite3 REQUIRED)
    set(GUI_SQLITE_TARGET SQLite::SQLite3)
endif()

if(NOT TARGET sqlite_orm::sqlite_orm)
    include(FetchContent)
    FetchContent_Declare(
        sqlite_orm
        GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
        GIT_TAG v1.9.1
    )
    FetchContent_GetProperties(sqlite_orm)
    if(NOT sqlite_orm_POPULATED)
        FetchContent_Populate(sqlite_orm)
    endif()
    add_library(sqlite_orm INTERFACE)
    target_include_directories(sqlite_orm INTERFACE ${sqlite_orm_SOURCE_DIR}/include)
    add_library(sqlite_orm::sqlite_orm ALIAS sqlite_orm)
endif()

if(NOT TARGET Boost::asio OR NOT TARGET Boost::uuid)
    include(FetchContent)
    FetchContent_Declare(
        boost_headers
        URL https://archives.boost.io/release/1.90.0/source/boost_1_90_0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(boost_headers)
    add_library(Boost_headers INTERFACE)
    target_include_directories(Boost_headers INTERFACE ${boost_headers_SOURCE_DIR})
    add_library(Boost::uuid ALIAS Boost_headers)
    add_library(Boost::asio ALIAS Boost_headers)
endif()


qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appMorze_ui
    main.cpp
    ClientBridge.cpp
    ../repositories/dbContext/Database.cpp
    ../repositories/repositories/ChatRepository.cpp
    ../repositories/repositories/ChatMemberRepository.cpp
    ../repositories/repositories/MessageRepository.cpp
    ../repositories/repositories/ConnectionProfileRepository.cpp
    ../services/services/ChatService.cpp
    ../services/services/ChatMemberService.cpp
    ../services/services/MessageService.cpp
    ../services/services/ConnectionProfileService.cpp
    ../services/services/SignalingService.cpp
    ../services/services/WebRTCService.cpp
    ../services/services/CanaryChecker.cpp
)

qt_add_qml_module(appMorze_ui
    URI Morze_ui
    QML_FILES
        Main.qml
    RESOURCES
        img/chats.svg
        img/search.svg
        img/info.svg
        img/send.svg
        img/close.svg
        img/logo.svg
        img/morze.svg
        img/sets.png
        img/refresh.svg
        img/ping.svg
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appMorze_ui PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appMorze_ui
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(NOT TARGET LibDataChannel::LibDataChannel)
    find_package(LibDataChannel REQUIRED)
endif()

target_link_libraries(appMorze_ui
    PRIVATE Qt6::Quick Qt6::Core Qt6::Network Qt6::QuickControls2 Qt6::WebSockets ${GUI_SQLITE_TARGET} sqlite_orm::sqlite_orm Boost::uuid Boost::asio LibDataChannel::LibDataChannel
)

target_include_directories(appMorze_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../repositories
    ${CMAKE_CURRENT_SOURCE_DIR}/../repositories/DAO
    ${CMAKE_CURRENT_SOURCE_DIR}/../repositories/DAO/converters
    ${CMAKE_CURRENT_SOURCE_DIR}/../repositories/repositories
    ${CMAKE_CURRENT_SOURCE_DIR}/../repositories/dbContext
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/Models
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/services
    ${CMAKE_CURRENT_SOURCE_DIR}/../globalEnums
    ${CMAKE_CURRENT_SOURCE_DIR}/../globalConverters
    ${CMAKE_CURRENT_SOURCE_DIR}/../exceptions
)

if (WIN32)
    target_link_libraries(appMorze_ui PRIVATE dwmapi ws2_32)
endif()

install(TARGETS appMorze_ui
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
