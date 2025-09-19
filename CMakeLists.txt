cmake_minimum_required(VERSION 3.22.1 FATAL_ERROR)

project(mp_os LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(VCPKG_TARGET_TRIPLET x64-linux)
set(CMAKE_TOOLCHAIN_FILE "/home/lausniko/vcpkg/scripts/buildsystems/vcpkg.cmake")

message(STATUS "Using VCPKG toolchain: ${CMAKE_TOOLCHAIN_FILE}")

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

# Fetch nlohmann_json
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Поиск зависимостей
find_package(Boost 1.71.0 REQUIRED COMPONENTS system)  # Crow требует Boost >= 1.71
find_package(Crow REQUIRED)

# Подключаем поддиректории
add_subdirectory(allocator)
add_subdirectory(arithmetic)
add_subdirectory(associative_container)
add_subdirectory(common)
add_subdirectory(logger)
