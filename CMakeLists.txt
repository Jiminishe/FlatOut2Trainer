cmake_minimum_required(VERSION 3.20)
project(FlatOut2Trainer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC-specific settings
if(MSVC)
    add_compile_options(/W4 /WX /EHsc /MP /bigobj)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    # Disable specific warnings
    add_compile_options(/wd4201 /wd4244 /wd4267 /wd4305 /wd4800 /wd4996)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -fPIC)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external/imgui)
include_directories(${CMAKE_SOURCE_DIR}/external/imgui/backends)

# ImGui sources
set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_dx9.cpp
)

# Trainer DLL (injected into game)
add_library(FlatOut2TrainerDLL SHARED
    src/dll_main.cpp
    src/hooking.cpp
    src/pattern_scanner.cpp
    src/memory.cpp
    src/cheats.cpp
    src/menu.cpp
    src/game_structs.cpp
    src/config.cpp
    src/hotkey.cpp
    src/cheat_state.cpp
    ${IMGUI_SOURCES}
)

target_link_libraries(FlatOut2TrainerDLL PRIVATE
    d3d9
    dinput8
    dxgi
    psapi
    version
)

# Launcher EXE (finds game, launches, injects)
add_executable(FlatOut2Trainer
    src/launcher_main.cpp
    src/launcher.cpp
    src/injector.cpp
    src/game_finder.cpp
)

target_link_libraries(FlatOut2Trainer PRIVATE)

# Install targets
install(TARGETS FlatOut2TrainerDLL FlatOut2Trainer
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)

# Copy ImGui ini to build dir for development
add_custom_command(TARGET FlatOut2Trainer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/external/imgui/misc/cpp/imgui.ini
        $<TARGET_FILE_DIR:FlatOut2Trainer>/imgui.ini
)

# Enable testing
enable_testing()