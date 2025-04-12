cmake_minimum_required(VERSION 3.12)

# Handle Boost policy warning
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD) # Use new Boost finding behavior
endif()

# CMP0076: target_sources() need to convert relative paths to absolute
# Needed for header files in include directories
if(POLICY CMP0076)
    cmake_policy(SET CMP0076 NEW)
endif()

project(character_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OS detection
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Boost configuration for 1.67
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
# Find Boost 1.67 for AL
find_package(Boost 1.67 REQUIRED COMPONENTS
    system
    thread
    serialization
    context
    fiber
    )
# Checking Boost found
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found. Ensure Boost 1.67 is installed")
endif()
message(STATUS "Found Boost: ${Boost_VERSION}")

# MySQL detection (OS-specific)
if(WIN32)
    # Usual MySQL Windows installation paths
    set(MYSQL_ROOT "C:/Program Files/MySQL/MySQL Server 8.0" CACHE PATH "MySQL Server install directory")

    find_path(MYSQL_INCLUDE_DIR
        NAMES mysql.h
        PATHS
        "${MYSQL_ROOT}/include"
        "$ENV{PROGRAMFILES}/MySQL/MySQL Server */include"
        "$ENV{PROGRAMFILES(x86)}/MySQL/MySQL Server */include"
        DOC "Path to MySQL include directory on Windows"
        )

    find_library(MYSQL_LIBRARY
        NAMES libmysql libmysqlclient
        PATHS
        "${MYSQL_ROOT}/lib"
        "${MYSQL_ROOT}/lib/vs14"
        "$ENV{PROGRAMFILES}/MySQL/MySQL Server */lib"
        "$ENV{PROGRAMFILES(x86)}/MySQL/MySQL Server */lib"
        DOC "Path to MySQL client library on Windows"
        )

    # Additional DLL handling on Windows
    find_file(MYSQL_DLL
        NAMES libmysql.dll mysqlclient.dll
        PATHS
        "${MYSQL_ROOT}/bin"
        "$ENV{PROGRAMFILES}/MySQL/MySQL Server */bin"
        "$ENV{PROGRAMFILES(x86)}/MySQL/MySQL Server */bin"
        DOC "Path to MySQL DLL for runtime"
        )

    if(MYSQL_DLL)
        # Copy DLL to build directory
        configure_file(${MYSQL_DLL} ${CMAKE_BINARY_DIR}/ COPYONLY)
        message(STATUS "Found MySQL DLL: ${MYSQL_DLL}")
    endif()

    # Windows socket library
    set(EXTRA_LIBS ws2_32)
else()
    # Linux MySQL paths
    find_path(MYSQL_INCLUDE_DIR
        NAMES mysql/mysql.h
        PATHS
        /usr/include/mysql
        /usr/local/include/mysql
        )

    find_library(MYSQL_LIBRARY
        NAMES mysqlclient mariadbclient
        PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
        )

    set(EXTRA_LIBS pthread)
endif()

if(WIN32)
    # Checking for windows
    if(NOT MYSQL_INCLUDE_DIR)
        message(FATAL_ERROR "MySQL include directory not found. On Windows, install MySQL Community Server")
    endif()

    if(NOT MYSQL_LIBRARY)
        message(FATAL_ERROR "MySQL client library not found. On Windows, install MySQL Connector/C")
    endif()
else()
    # Checking for Linux
    if(NOT MYSQL_INCLUDE_DIR)
        message(FATAL_ERROR "MySQL include directory not found. Install libmysqlclient-dev or similar package")
    endif()

    if(NOT MYSQL_LIBRARY)
        message(FATAL_ERROR "MySQL client library not found. Install libmysqlclient or similar package")
    endif()
endif()

message(STATUS "Found MySQL includes: ${MYSQL_INCLUDE_DIR}")
message(STATUS "Found MySQL library: ${MYSQL_LIBRARY}")

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIR}
    include
)

# Single instance lock configuration
if(UNIX)
    add_definitions(-DLINUX_SINGLE_INSTANCE)
elseif(WIN32)
    add_definitions(-DWIN_SINGLE_INSTANCE)
endif()

# target with sources and headers
add_executable(server)

target_sources(server PRIVATE
    src/main.cpp
    src/database_manager.cpp
    src/session_manager.cpp
    src/server_instance.cpp
    )

target_sources(server PUBLIC
    include/protocol.h
    include/database_manager.h
    include/session_manager.h
    include/server_instance.h
    )

# Include directories
target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(server PRIVATE
    ${Boost_LIBRARIES}
    ${MYSQL_LIBRARY}
    # Contains ws2_32 on Windows, pthread on Linux
    ${EXTRA_LIBS}
)

# Post-build steps for Windows
if(WIN32)
    # Copy MySQL DLL to output directory
    add_custom_command(TARGET server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYSQL_DLL}"
            $<TARGET_FILE_DIR:server>
        COMMENT "Copying MySQL DLL to output directory"
    )

    # Install the DLL when 'make install' is run
    install(FILES ${MYSQL_DLL} DESTINATION bin)
endif()

# Installation
install(TARGETS server DESTINATION bin)
