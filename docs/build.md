# Build Guide

QTierMaker uses CMake, Qt 6, C++20, and VkUI.

## Dependencies

- CMake 3.24 or newer.
- Ninja or another CMake generator.
- Qt 6.10.1 with Core, Gui, Widgets, Svg, Network, Concurrent, Test, LinguistTools, and the private
  Core/Gui/Widgets modules required by VkUI's integrated native window implementation.
- VkUI initialized under `third_party/`. Its integrated window module provides native frameless
  behavior on macOS and Windows.

## Configure

```bash
git submodule update --init --recursive
cmake -S . -B build/default -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.x.x/platform"
```

## Build and Test

```bash
cmake --build build/default
ctest --test-dir build/default --output-on-failure
```

## User Presets

Copy `CMakeUserPresets.json.example` to `CMakeUserPresets.json` and replace the placeholder prefix paths. Do not commit the user preset.
