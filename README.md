# Egoboo

Egoboo is an open-source 3D dungeon crawler in the spirit of NetHack.
The current maintained build documents in this repository cover Linux and
Windows.

Current project direction:

- move the remaining mixed C/C++ runtime toward C++
- make native Windows compilation work as a first-class target instead of treating Wine as the practical fallback
- make Linux-hosted Windows cross-compilation work as a first-class target too, not just native Windows builds
- keep Linux builds, native Windows builds, and Linux-hosted Windows builds as similar as practical in structure and tooling
- keep the Windows build and tooling path fully open source, which means Visual Studio-specific workflows are not part of the maintained direction
- reduce portability debt, old dependency friction, and routine C++ warning noise as part of the refactor
- improve the current buggy and incomplete runtime toward a stable, modular, maintainable baseline

### License
Egoboo is made available publicly under the
[GNU GPLv3 License](https://github.com/dubstabber/egoboo/blob/master/LICENSE).

### Contact
Developers can usually be contacted via GitHub.

### Dependencies

The build needs a C/C++ toolchain, CMake, `pkg-config`, and the SDL2 + OpenGL
development packages. `PhysFS` does **not** need to be installed separately — it
is vendored and built by CMake through `idlib-game-engine`. `ninja` and `ccache`
are optional but recommended for fast incremental rebuilds.

The real dependency contract is `idlib-game-engine/library/CMakeLists.txt`; the
lists below are the practical per-distro equivalents.

**Fedora**

```bash
sudo dnf install \
  cmake ninja-build ccache gcc gcc-c++ make pkgconf-pkg-config git \
  SDL2-devel SDL2_image-devel SDL2_mixer-devel SDL2_ttf-devel \
  mesa-libGL-devel
```

**Ubuntu / Debian**

```bash
sudo apt install \
  cmake ninja-build ccache build-essential pkg-config git \
  libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev \
  libgl1-mesa-dev
```

**Arch Linux**

```bash
sudo pacman -S --needed \
  cmake ninja ccache base-devel pkgconf git \
  sdl2 sdl2_image sdl2_mixer sdl2_ttf \
  mesa
```

(On Arch the `sdl2*` packages already ship their development headers, so there
are no separate `-dev`/`-devel` packages.)

**Windows (mingw-w64, cross-built from Linux)**

The maintained Windows path is a Fedora-hosted cross-build with `mingw-w64`. The
Windows-target SDL libraries come from the vendored `external/mingw` bundle, so
you only need the cross-toolchain itself:

```bash
# Fedora host
sudo dnf install \
  mingw64-gcc mingw64-gcc-c++ mingw64-binutils mingw64-winpthreads-static

# Ubuntu/Debian host
sudo apt install mingw-w64

# Arch host
sudo pacman -S --needed mingw-w64-gcc
```

Verify the cross-compilers are on `PATH`:

```bash
which x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++
```

Native Windows compilation (e.g. via an MSYS2 mingw-w64 environment) is a future
first-class target; see `doc/build-windows.md` for current status and known
runtime issues.

### Building from Source
Clone the repository:

```bash
git clone https://github.com/dubstabber/egoboo
cd egoboo
git submodule update --init data external idlib idlib-game-engine
```

For normal Egoboo development, only the root submodules are required.
The superproject passes the top-level `idlib/` into `idlib-game-engine` during
the CMake build, so `idlib-game-engine/idlib` does not need to be initialized.

Supported build documents:

- Linux: `doc/build-linux.md`
- Windows: `doc/build-windows.md`

Older platform-specific `README.*` files have been quarantined to `doc/legacy/`.
The `doc/build-*.md` files are the current source of truth for active build
paths.

Quick start:

Linux (Ninja + ccache recommended; pick a `-j` for your core count):

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

`ctest` runs serially by default, which is what you want here — several
fixtures share writable user-data paths and fail spuriously under high
`ctest -j`. Keep test runs serial for a trustworthy baseline.

Windows cross-build with `mingw-w64`:

```bash
cmake -S . -B build-windows -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake"
cmake --build build-windows -j8
```

Run the cross-built Windows binary through Wine from Linux:

```bash
./run-egoboo-windows.sh
```

The Wine helper currently applies compatibility defaults for the Windows build:
`EGOBOO_DISABLE_MIPMAPS=1` and `EGOBOO_DISABLE_AUDIO=1`. This helper is a temporary compatibility path, not the long-term Windows support goal.

The current Wine-run Windows build is still not a good gameplay target due to font-atlas initialization failure and audio loading crashes under Wine.

Important output directories:

- Linux binaries: `build/products/x64/bin/`
- Fedora cross-built Windows binaries: `build-windows/products/x64/bin/`

### Running From The Source Tree

Preferred Linux launch path:

```bash
./run-egoboo.sh
```

This uses the repository `data/` checkout directly and keeps generated runtime
data under `.egoboo-runtime/` in the repository root.

### Content Validator

The repository now includes a lightweight non-UI validation tool for content
and parser smoke checks:

```bash
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"
```

Fast single-module smoke check:

```bash
./build/products/x64/bin/egoboo-content-validator \
  --data-dir "$PWD/data" \
  --module test.mod
```

### CI Notes

Legacy AppVeyor configuration files are still checked in as
`appveyor-linux.yml` and `appveyor-windows.yml`, but they are not the primary
source of truth for the current build workflow. Prefer the local build
documents and commands above.
