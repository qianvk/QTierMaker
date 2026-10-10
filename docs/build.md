# Build Guide

QTierMaker is a C++20 desktop application built with CMake, Qt 6, and VkUI. The checked-in CMake
presets are platform-neutral: `debug` and `release` describe build intent, while each developer's Qt
installation remains local.

## Requirements

- CMake 3.24 or newer.
- Ninja.
- A C++20 compiler: Apple Clang on macOS or MSVC 2022 on Windows.
- Qt 6.10.1 with Core, Gui, Widgets, Svg, Network, Concurrent, Test, LinguistTools, and
  Qt Image Formats.
- Git submodules initialized for `third_party/vkui`.

QTierMaker currently targets macOS arm64 and Windows x64. VkUI supplies the native window layer on
both platforms.

## Get The Source

```bash
git clone --recurse-submodules https://github.com/qianvk/QTierMaker.git
cd QTierMaker
```

For an existing checkout:

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

## Select Qt

`CMakePresets.json` deliberately contains no `CMAKE_PREFIX_PATH`. A Qt installation is specific to
one machine and must not be committed to a shared preset.

Generated files such as `CMakeCache.txt`, `compile_commands.json`, and Qt Creator's user settings
will still contain absolute compiler and Qt paths. That is expected: they describe one local build
tree and are ignored by Git.

For a one-off shell session, expose Qt through the environment.

macOS:

```bash
export CMAKE_PREFIX_PATH="$HOME/Qt/6.10.1/macos"
```

Windows PowerShell, opened from a Visual Studio 2022 developer environment:

```powershell
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.10.1\msvc2022_64"
```

Alternatively, copy `CMakeUserPresets.json.example` to `CMakeUserPresets.json`, replace the
placeholder with an absolute Qt path, and configure with `local-debug` or `local-release`.
`CMakeUserPresets.json` is ignored by Git.

## Configure, Build, And Test

Debug:

```bash
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
```

Release:

```bash
cmake --preset release
cmake --build --preset release --parallel
ctest --preset release
```

When using a local user preset, only the configure command changes:

```bash
cmake --preset local-debug
cmake --build --preset local-debug --parallel
ctest --preset local-debug
```

Both configure presets generate `compile_commands.json`. The repository `.clangd` file points to
`build/debug`, so clangd resolves Qt, VkUI, generated MOC headers, and project headers after the
first Debug configure.

## Qt Creator

1. Initialize the Git submodules.
2. Open the repository's `CMakeLists.txt` in Qt Creator.
3. Select a Qt 6.10.1 Desktop Kit for the host platform.
4. Select the `debug` or `release` configure preset.

The Kit supplies the local Qt installation. A "no suitable Kit" message usually means the Qt
version, compiler, or CMake/Ninja tool is not registered under **Preferences > Kits**; it is not a
reason to add an absolute Qt path to the shared preset.

## Create An Installer

Build Release first, then run CPack from the generated configuration.

macOS arm64:

```bash
cmake --preset release -DQTM_BUILD_TESTS=OFF -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build --preset release --parallel
cpack --config build/release/CPackConfig.cmake -G DragNDrop
```

Windows x64, from a Visual Studio 2022 developer PowerShell with NSIS installed:

```powershell
cmake --preset release -DQTM_BUILD_TESTS=OFF
cmake --build --preset release --parallel
cpack --config build/release/CPackConfig.cmake -G NSIS
```

Release automation and update-package details are documented in [release.md](release.md).
