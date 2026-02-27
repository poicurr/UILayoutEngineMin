# UILayoutEngineMin

Minimal "CPU-only layout engine → UiDrawList(IR) → draw" sanity project.

This is intentionally tiny:
- Layout is computed on CPU (Row/Column + Fixed/Fill).
- Output is `UiDrawListOwned` with `UiVertex/UiDrawCmd` close to the renderer contract.
- Rendering uses legacy OpenGL immediate mode (GL 2.1 compatibility) via GLFW, mainly to visualize and validate:
  - px coordinate convention (top-left origin)
  - scissor behavior
  - command splitting / draw order

> The goal is not production rendering; it’s a contract smoke-test.

## Dependencies

- CMake >= 3.20
- C++20 compiler
- OpenGL (system)
- GLFW

### Installing GLFW (examples)
- vcpkg: `vcpkg install glfw3`
- Homebrew: `brew install glfw`
- Ubuntu: `sudo apt-get install libglfw3-dev`

This template uses:
```cmake
find_package(glfw3 CONFIG REQUIRED)
```
So prefer a package manager that provides a CMake config package (vcpkg, etc.).

## Build

```bash
cmake -B build -S . -G "Ninja Multi-Config" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE="C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build -j --config Release
```

Run:
```bash
./build/Release/UILayoutEngineMin
```

## Screenshot
![UILayoutEngineMin screenshot](https://raw.githubusercontent.com/poicurr/resources/main/UILayoutEngineMin/screenshot.png)

## What you should see

A window showing nested panels laid out using a simple Column/Row model.
Some nodes enable clipping to validate scissor behavior.

Keys:
- `R` : rebuild the UI tree (same structure) and regenerate drawlist
- `Esc`: exit

## Notes

- The drawlist is indexed-only and uses a 1x1 white texture conceptually, but the sample multiplies color directly (texture is a no-op in this prototype).
- The renderer contract in your engine may require `baseVertex/firstIndex/indexCount + scissor`. This sample follows that shape.
