cmake_minimum_required(VERSION 3.20)

project(UILayoutEngineMin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)

# Prefer CONFIG mode (vcpkg / package managers). Fallback to module if needed.
find_package(glfw3 CONFIG REQUIRED)

add_executable(UILayoutEngineMin
  src/main.cpp
  src/gltfviewer/ui/font.cpp
  src/ui/layout.hpp
  src/ui/drawlist.hpp
  src/ui/input.hpp
  src/ui/clip_stack.hpp
  src/ui/draw_builder.hpp
  src/ui/font.hpp
  src/third_party/stb_truetype.h
)

target_include_directories(UILayoutEngineMin PRIVATE src)
target_link_libraries(UILayoutEngineMin PRIVATE OpenGL::GL glfw)

if (MSVC)
  target_compile_options(UILayoutEngineMin PRIVATE /W4)
else()
  target_compile_options(UILayoutEngineMin PRIVATE -Wall -Wextra -Wpedantic)
endif()
