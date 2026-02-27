cmake_minimum_required(VERSION 3.25...4.2)

project(libmodbus-cpp
    VERSION 1.1.0
    DESCRIPTION "Modern C++23 wrapper for libmodbus"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(LIBMODBUS_USE_SYSTEM "Use installed libmodbus via pkg-config" OFF)
option(LIBMODBUS_SKIP_TOOL_CHECK "Skip checking autotools prerequisites for libmodbus FetchContent build" OFF)
option(LIBMODBUS_CPP_ENABLE_INSTALL "Enable install and CMake package export rules" ON)
option(LIBMODBUS_CPP_ENABLE_CPACK "Enable CPack packaging support" ${PROJECT_IS_TOP_LEVEL})

if(LIBMODBUS_USE_SYSTEM)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBMODBUS REQUIRED IMPORTED_TARGET libmodbus)
    if(NOT TARGET libmodbus::libmodbus)
        add_library(libmodbus::libmodbus INTERFACE IMPORTED)
        target_link_libraries(libmodbus::libmodbus INTERFACE PkgConfig::LIBMODBUS)
    endif()
    set(LIBMODBUS_TARGET libmodbus::libmodbus)
else()
    include(FetchContent)
    include(ExternalProject)

    find_program(AUTORECONF_EXECUTABLE autoreconf)
    find_program(MAKE_EXECUTABLE NAMES make gmake)

    if(NOT LIBMODBUS_SKIP_TOOL_CHECK AND (NOT AUTORECONF_EXECUTABLE OR NOT MAKE_EXECUTABLE))
        message(FATAL_ERROR
            "Building libmodbus from source requires autotools prerequisites.\n"
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

    # Download at configure time via FetchContent (no CMakeLists.txt in
    # libmodbus, so FetchContent_MakeAvailable just populates the source tree).
    FetchContent_Declare(libmodbus
        GIT_REPOSITORY https://github.com/stephane/libmodbus.git
        GIT_TAG        ${LIBMODBUS_GIT_TAG}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(libmodbus)

    # Build the autotools project at build time via ExternalProject.
    ExternalProject_Add(libmodbus_external
        SOURCE_DIR      ${libmodbus_SOURCE_DIR}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ./autogen.sh
        COMMAND ./configure --prefix=${LIBMODBUS_INSTALL_PREFIX}
        BUILD_COMMAND     make
        INSTALL_COMMAND   make install
        UPDATE_COMMAND    ""
    )

    set(LIBMODBUS_IMPORTED_LIBRARY ${LIBMODBUS_INSTALL_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}modbus${CMAKE_SHARED_LIBRARY_SUFFIX})

    add_library(libmodbus::libmodbus SHARED IMPORTED GLOBAL)
    set_target_properties(libmodbus::libmodbus PROPERTIES
        IMPORTED_LOCATION ${LIBMODBUS_IMPORTED_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${LIBMODBUS_INSTALL_PREFIX}/include
    )

    add_dependencies(libmodbus::libmodbus libmodbus_external)
    set(LIBMODBUS_TARGET libmodbus::libmodbus)
endif()

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
    ${LIBMODBUS_TARGET}
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

if(LIBMODBUS_CPP_ENABLE_INSTALL)
    set(LIBMODBUS_CPP_CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/libmodbus_cpp)

    install(
        TARGETS modbus_cpp
        EXPORT libmodbus_cppTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        EXPORT libmodbus_cppTargets
        NAMESPACE libmodbus_cpp::
        DESTINATION ${LIBMODBUS_CPP_CONFIG_INSTALL_DIR}
        FILE libmodbus_cppTargets.cmake
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/libmodbus_cppConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/libmodbus_cppConfig.cmake
        INSTALL_DESTINATION ${LIBMODBUS_CPP_CONFIG_INSTALL_DIR}
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/libmodbus_cppConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/libmodbus_cppConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/libmodbus_cppConfigVersion.cmake
        DESTINATION ${LIBMODBUS_CPP_CONFIG_INSTALL_DIR}
    )
endif()

if(LIBMODBUS_CPP_ENABLE_CPACK)
    set(CPACK_PACKAGE_NAME libmodbus-cpp)
    set(CPACK_PACKAGE_VENDOR "libmodbus_cpp")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern C++23 wrapper for libmodbus")
    set(CPACK_PACKAGE_CONTACT "")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Markus Werle")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

    if(WIN32)
        set(CPACK_GENERATOR ZIP)
    elseif(APPLE)
        set(CPACK_GENERATOR TGZ)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(CPACK_GENERATOR TGZ DEB)
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS TRUE)

        find_program(RPMBUILD_EXECUTABLE rpmbuild)
        if(RPMBUILD_EXECUTABLE)
            list(APPEND CPACK_GENERATOR RPM)
        else()
            message(STATUS "rpmbuild not found: RPM package generation disabled")
        endif()
    else()
        set(CPACK_GENERATOR TGZ)
    endif()

    include(CPack)
endif()
