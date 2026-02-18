cmake_minimum_required(VERSION 3.16)
project(RetroCRTDashboard VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find raylib
find_package(raylib CONFIG REQUIRED)

# Add executable
add_executable(${PROJECT_NAME}
    main.cpp
    theme.cpp
    dashboard.cpp
    ui_menus.cpp
    terminal.cpp
    config.cpp
    system_monitor.cpp
    speedtest.cpp
)

# Link raylib
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# Copy resources to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)

# Set working directory to executable location for IDE debugging
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
