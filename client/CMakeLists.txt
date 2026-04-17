cmake_minimum_required(VERSION 4.1)
project(Morze)
include(FetchContent)

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

include_directories(. ./*)

FetchContent_MakeAvailable(sqlite_orm)

set(CMAKE_CXX_STANDARD 20)

add_executable(Morze main.cpp
        repositories/migrations/Database.cpp
        repositories/migrations/Database.h
        repositories/DAO/ChatDAO.h
        repositories/DAO/ChatMemberDAO.h
        repositories/DAO/MessageDAO.h
        GlobalEnums.h
        repositories/DAO/ConnectionProfileDAO.h)
target_link_libraries(Morze PRIVATE sqlite_orm::sqlite_orm sqlite3)

