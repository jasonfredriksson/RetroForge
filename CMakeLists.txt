cmake_minimum_required(VERSION 3.16)
project(RetroCRTDashboard VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find raylib
find_package(raylib CONFIG REQUIRED)

# Source files (shared across all platforms)
set(SOURCES
    main.cpp
    theme.cpp
    dashboard.cpp
    ui_menus.cpp
    terminal.cpp
    config.cpp
    system_monitor.cpp
    system_monitor_posix.cpp
    speedtest.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Platform-specific link libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
elseif(APPLE)
    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(IOKIT_LIB IOKit REQUIRED)
    find_library(COREVIDEO_LIB CoreVideo REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        raylib
        ${COCOA_LIB} ${IOKIT_LIB} ${COREVIDEO_LIB}
    )
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        raylib
        pthread dl m
    )
    # GL is needed by raylib on Linux
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
endif()

# Copy resources to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)

# Set working directory to executable location for IDE debugging
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
