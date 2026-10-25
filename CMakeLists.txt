cmake_minimum_required(VERSION 3.20)
project(nerv VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows and global definitions
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc /MP)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)

# 1. GLFW
message(STATUS "Configuring GLFW...")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# 2. Dear ImGui
message(STATUS "Configuring Dear ImGui...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# Build ImGui as a static library
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)
target_link_libraries(imgui_lib PUBLIC glfw)

# 3. nlohmann/json
message(STATUS "Configuring nlohmann_json...")
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# 4. Catch2 for tests
message(STATUS "Configuring Catch2...")
set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

# GLAD library
add_library(glad_lib STATIC
    external/glad/src/glad.c
)
target_include_directories(glad_lib PUBLIC
    external/glad/include
)

# NERV Core Library
file(GLOB_RECURSE NERV_HEADERS
    "include/*.hpp"
)
file(GLOB_RECURSE NERV_SOURCES
    "src/app/*.cpp"
    "src/platform/*.cpp"
    "src/telemetry/*.cpp"
    "src/model/*.cpp"
    "src/graph/*.cpp"
    "src/rendering/*.cpp"
    "src/history/*.cpp"
    "src/events/*.cpp"
    "src/ui/*.cpp"
)

add_library(nerv_core STATIC
    ${NERV_SOURCES}
    ${NERV_HEADERS}
)

target_include_directories(nerv_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(nerv_core PUBLIC
    glad_lib
    imgui_lib
    glfw
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(nerv_core PUBLIC
        opengl32
        psapi
        iphlpapi
        pdh
        shell32
    )
endif()

# Copy Assets to Binary directory
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/assets
    COMMENT "Copying assets to runtime directory"
)

# Main Executable
add_executable(nerv
    src/main.cpp
)
target_link_libraries(nerv PRIVATE nerv_core)
add_dependencies(nerv copy_assets)

# Validation Tool
add_executable(validate_telemetry
    tools/validate_telemetry.cpp
)
target_link_libraries(validate_telemetry PRIVATE nerv_core)

# Unit & Integration Tests
file(GLOB_RECURSE TEST_SOURCES
    "tests/*.cpp"
)

add_executable(nerv_tests
    ${TEST_SOURCES}
)
target_link_libraries(nerv_tests PRIVATE nerv_core Catch2::Catch2WithMain)
add_dependencies(nerv_tests copy_assets)

enable_testing()
add_test(NAME AllNervTests COMMAND nerv_tests)
