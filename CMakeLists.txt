cmake_minimum_required(VERSION 3.20)

project(ui_layout_glfw_min LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)

# Prefer CONFIG mode (vcpkg / package managers). Fallback to module if needed.
find_package(glfw3 CONFIG REQUIRED)

add_executable(ui_layout_glfw_min
  src/main.cpp
  src/ui/ui_layout.hpp
  src/ui/ui_drawlist.hpp
)

target_include_directories(ui_layout_glfw_min PRIVATE src)
target_link_libraries(ui_layout_glfw_min PRIVATE OpenGL::GL glfw)

if (MSVC)
  target_compile_options(ui_layout_glfw_min PRIVATE /W4)
else()
  target_compile_options(ui_layout_glfw_min PRIVATE -Wall -Wextra -Wpedantic)
endif()
