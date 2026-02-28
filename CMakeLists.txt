cmake_minimum_required(VERSION 3.25...4.2)

project(libmodbus-cpp
    VERSION 1.4.0
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
            "  Alpine:        sudo apk add autoconf automake libtool make gcc musl-dev\n"
            "  MSYS2:         pacman -S autoconf automake libtool make gcc"
        )
    endif()

    set(LIBMODBUS_GIT_TAG master)
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
    # On Windows (MSYS2): build as static library. autoreconf must be run
    # BEFORE cmake --build (in a CI step) because ExternalProject runs commands
    # via cmake -P / execute_process(), which creates processes in the
    # Windows/MinGW context, not the MSYS2 context. This breaks aclocal's
    # MSYS2 path resolution (progtest.m4 not found). By pre-running autoreconf
    # in the MSYS2 shell, the generated configure script is ready for
    # ExternalProject to use directly.
    # On Linux: build as shared library. When cross-compiling, pass --host and CC.
    if(WIN32)
        # On Windows, ExternalProject only runs ./configure (not autogen.sh)
        # because autoreconf must be pre-run in the MSYS2 shell CI step.
        set(_LIBMODBUS_CONFIGURE_COMMAND
            ./configure --prefix=${LIBMODBUS_INSTALL_PREFIX} --enable-static --disable-shared
        )
        set(_LIBMODBUS_BUILD_COMMAND make)
        set(_LIBMODBUS_INSTALL_COMMAND make install)
    else()
        set(_LIBMODBUS_CONFIGURE_COMMAND ./autogen.sh COMMAND ./configure --prefix=${LIBMODBUS_INSTALL_PREFIX})
        # Note: CMAKE_CROSSCOMPILING may be FALSE for Linux-to-Linux cross-compilation
        # (same OS name, different architecture), so also compare processor types.
        if(CMAKE_CROSSCOMPILING OR NOT "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "${CMAKE_HOST_SYSTEM_PROCESSOR}")
            # Derive the GNU triplet from the C compiler name (e.g. aarch64-linux-gnu-gcc-14 → aarch64-linux-gnu)
            get_filename_component(_cc_name ${CMAKE_C_COMPILER} NAME)
            string(REGEX REPLACE "-gcc.*$" "" _cross_triplet "${_cc_name}")
            list(APPEND _LIBMODBUS_CONFIGURE_COMMAND
                --host=${_cross_triplet}
                "CC=${CMAKE_C_COMPILER}"
            )
        endif()
        set(_LIBMODBUS_BUILD_COMMAND make)
        set(_LIBMODBUS_INSTALL_COMMAND make install)
    endif()

    ExternalProject_Add(libmodbus_external
        SOURCE_DIR      ${libmodbus_SOURCE_DIR}
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ${_LIBMODBUS_CONFIGURE_COMMAND}
        BUILD_COMMAND     ${_LIBMODBUS_BUILD_COMMAND}
        INSTALL_COMMAND   ${_LIBMODBUS_INSTALL_COMMAND}
        UPDATE_COMMAND    ""
    )

    # On Windows, link libmodbus statically (no DLL management needed).
    # On Linux/Unix, use the shared library.
    if(WIN32)
        set(LIBMODBUS_IMPORTED_LIBRARY ${LIBMODBUS_INSTALL_PREFIX}/lib/libmodbus.a)
        add_library(libmodbus::libmodbus STATIC IMPORTED GLOBAL)
        set_target_properties(libmodbus::libmodbus PROPERTIES
            IMPORTED_LOCATION ${LIBMODBUS_IMPORTED_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${LIBMODBUS_INSTALL_PREFIX}/include
            INTERFACE_LINK_LIBRARIES ws2_32
        )
    else()
        set(LIBMODBUS_IMPORTED_LIBRARY ${LIBMODBUS_INSTALL_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}modbus${CMAKE_SHARED_LIBRARY_SUFFIX})
        add_library(libmodbus::libmodbus SHARED IMPORTED GLOBAL)
        set_target_properties(libmodbus::libmodbus PROPERTIES
            IMPORTED_LOCATION ${LIBMODBUS_IMPORTED_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${LIBMODBUS_INSTALL_PREFIX}/include
        )
    endif()

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
    set(CPACK_PACKAGE_VENDOR "Markus Werle")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern C++23 wrapper for libmodbus")
    set(CPACK_PACKAGE_CONTACT "daixtrose")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Markus Werle")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_VERBATIM_VARIABLES YES)

    # Standard naming with architecture: <pkg>_<ver>_<arch>.deb / <pkg>-<ver>-<rel>.<arch>.rpm
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_RELEASE "1")

    # Override RPM strip command for cross-compilation
    if(NOT "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "${CMAKE_HOST_SYSTEM_PROCESSOR}")
        set(CPACK_RPM_SPEC_MORE_DEFINE "%define __strip ${CMAKE_STRIP}")
        # Explicit architecture for cross-compiled packages
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
            set(CPACK_RPM_PACKAGE_ARCHITECTURE "aarch64")
        else()
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
            set(CPACK_RPM_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
        endif()
        # Include architecture in TGZ filename for disambiguation
        set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-linux-${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    if(WIN32)
        set(CPACK_GENERATOR ZIP)
    elseif(APPLE)
        set(CPACK_GENERATOR TGZ)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(CPACK_GENERATOR TGZ DEB RPM)
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS TRUE)
    else()
        set(CPACK_GENERATOR TGZ)
    endif()

    include(CPack)
endif()
