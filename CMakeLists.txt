cmake_minimum_required(VERSION 3.25)

project(libmodbus-cpp
    VERSION 0.1.0
    DESCRIPTION "Modern C++23 wrapper for libmodbus"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS REQUIRED libmodbus)

add_library(modbus_cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/modbus_connection.cpp
)

add_library(libmodbus_cpp::modbus_cpp ALIAS modbus_cpp)

target_include_directories(modbus_cpp
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${LIBMODBUS_INCLUDE_DIRS}
)

target_link_libraries(modbus_cpp
    PUBLIC
        ${LIBMODBUS_LIBRARIES}
)

target_compile_features(modbus_cpp PUBLIC cxx_std_23)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(modbus_cpp PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
elseif(MSVC)
    target_compile_options(modbus_cpp PRIVATE
        /W4
    )
endif()
