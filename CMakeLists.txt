cmake_minimum_required(VERSION 3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(
    cppweb
    VERSION 0.1.0
    DESCRIPTION "Simple C++ Web"
    LANGUAGES CXX)

# ---- Dependencies ----

find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG 1.14.2
                         SYSTEM)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    SYSTEM)
FetchContent_MakeAvailable(json)

# ---- Compile options (interface targets) ----

# Warnings
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
    $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics -Weverything
        -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded
        -Wno-weak-vtables>
)

# Hardening
add_library(project_hardening INTERFACE)
target_compile_options(project_hardening INTERFACE
    -fstack-protector-strong
    -fstack-clash-protection
    -fcf-protection=full
    # -D_FORTIFY_SOURCE=3
)
target_link_options(project_hardening INTERFACE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack)

# Perfomance
add_library(project_performance INTERFACE)
target_compile_options(project_performance INTERFACE
    $<$<CONFIG:Release>: -flto> # -march=native -mtune=native
)
target_link_options(project_performance INTERFACE
    $<$<CONFIG:Release>:-flto>
)

# Sanitizers
add_library(project_sanitizers INTERFACE)
set(SAN_FLAGS "-fsanitize=address,undefined") # thread
target_compile_options(project_sanitizers INTERFACE
    $<$<CONFIG:Debug>:${SAN_FLAGS} -fno-omit-frame-pointer>)
target_link_options(project_sanitizers INTERFACE
    $<$<CONFIG:Debug>:${SAN_FLAGS}>)

# ---- Executable ----

add_executable(${PROJECT_NAME} src/main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    cpr::cpr
    nlohmann_json::nlohmann_json
    project_hardening
    project_warnings
    project_sanitizers
    project_performance)
