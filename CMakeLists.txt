cmake_minimum_required(VERSION 3.25)

project(libmodbus-cpp
    VERSION 0.1.0
    DESCRIPTION "Modern C++23 wrapper for libmodbus"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(ExternalProject)

option(LIBMODBUS_SKIP_TOOL_CHECK "Skip checking autotools prerequisites for libmodbus FetchContent build" OFF)

find_program(AUTORECONF_EXECUTABLE autoreconf)
find_program(MAKE_EXECUTABLE NAMES make gmake)

if(NOT LIBMODBUS_SKIP_TOOL_CHECK AND (NOT AUTORECONF_EXECUTABLE OR NOT MAKE_EXECUTABLE))
    message(FATAL_ERROR
        "Building libmodbus from git via FetchContent requires autotools prerequisites.\n"
        "Missing tools:\n"
        "  autoreconf: ${AUTORECONF_EXECUTABLE}\n"
        "  make: ${MAKE_EXECUTABLE}\n\n"
        "Install prerequisites, then re-run CMake. Examples:\n"
        "  Debian/Ubuntu: sudo apt update && sudo apt install -y autoconf automake libtool make gcc\n"
        "  Fedora/RHEL:   sudo dnf install -y autoconf automake libtool make gcc\n"
        "  Arch Linux:    sudo pacman -S --needed autoconf automake libtool make gcc\n"
        "  openSUSE:      sudo zypper install -y autoconf automake libtool make gcc\n"
        "  Alpine:        sudo apk add autoconf automake libtool make gcc musl-dev"
    )
endif()

set(LIBMODBUS_GIT_TAG v3.1.12)
set(LIBMODBUS_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/_deps/libmodbus-install)

file(MAKE_DIRECTORY ${LIBMODBUS_INSTALL_PREFIX}/include/modbus)
file(MAKE_DIRECTORY ${LIBMODBUS_INSTALL_PREFIX}/lib)

FetchContent_Declare(libmodbus
    GIT_REPOSITORY https://github.com/stephane/libmodbus.git
    GIT_TAG ${LIBMODBUS_GIT_TAG}
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(libmodbus)
if(NOT libmodbus_POPULATED)
    FetchContent_Populate(libmodbus)
endif()

ExternalProject_Add(libmodbus_external
    SOURCE_DIR ${libmodbus_SOURCE_DIR}
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./autogen.sh
    COMMAND ./configure --prefix=${LIBMODBUS_INSTALL_PREFIX}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    UPDATE_COMMAND ""
)

set(LIBMODBUS_IMPORTED_LIBRARY ${LIBMODBUS_INSTALL_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}modbus${CMAKE_SHARED_LIBRARY_SUFFIX})

add_library(libmodbus::libmodbus SHARED IMPORTED GLOBAL)
set_target_properties(libmodbus::libmodbus PROPERTIES
    IMPORTED_LOCATION ${LIBMODBUS_IMPORTED_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${LIBMODBUS_INSTALL_PREFIX}/include/modbus
)

add_dependencies(libmodbus::libmodbus libmodbus_external)

add_library(modbus_cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/modbus_connection.cpp
)

add_library(libmodbus_cpp::modbus_cpp ALIAS modbus_cpp)

target_include_directories(modbus_cpp
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(modbus_cpp
    PUBLIC
        libmodbus::libmodbus
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
