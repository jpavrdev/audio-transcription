cmake_minimum_required(VERSION 3.16)
project(transcritor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# so queremos a biblioteca do whisper, sem os exemplos, testes e servidor
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
    GIT_TAG        v1.9.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(whisper)

add_executable(transcritor src/main.cpp)
target_link_libraries(transcritor PRIVATE whisper)
