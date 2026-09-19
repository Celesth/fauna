cmake_minimum_required(VERSION 3.20)

project(
    fauna
    VERSION 0.2.0
    DESCRIPTION "A local version control system"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

add_executable(fauna
    src/main.cpp
    src/repository.cpp
)

target_include_directories(fauna
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(fauna
    PRIVATE
        OpenSSL::Crypto
)

if (MSVC)
    target_compile_options(fauna PRIVATE /W4)
else()
    target_compile_options(fauna PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
