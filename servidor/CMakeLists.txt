cmake_minimum_required(VERSION 3.16)
project(servidor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# so a biblioteca do drogon, sem exemplos, sem o drogon_ctl e sem testes
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_CTL      OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    drogon
    GIT_REPOSITORY https://github.com/drogonframework/drogon.git
    GIT_TAG        v1.9.13
)
FetchContent_MakeAvailable(drogon)

add_executable(servidor
    src/main.cpp
    src/bd.cpp
    src/senha.cpp
)

# drogon pro servidor web, pqxx/pq pro acesso sincrono, argon2 pro hash de senha
target_link_libraries(servidor PRIVATE drogon pqxx pq argon2)
