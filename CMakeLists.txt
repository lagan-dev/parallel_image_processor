cmake_minimum_required(VERSION 3.16)

project(ParallelImageProcessor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

set(SOURCES
    src/main.cpp
    src/image.cpp
)

add_library(filters_lib src/filters.cpp)
target_include_directories(filters_lib PUBLIC include)

add_executable(engine ${SOURCES})
target_include_directories(engine PRIVATE include)
target_link_libraries(engine PRIVATE filters_lib)

enable_testing()
add_subdirectory(test)