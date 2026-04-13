# Egoboo

Egoboo is a working cool 3D dungeon crawling game in the spirit of NetHack.
It supports Windows, Linux and Mac.

### License
Egoboo is made available publicly under the
[GNU GPLv3 License](https://github.com/egoboo/egoboo/LICENSE).

### Contact
Developers can usually be contacted via GitHub.

### Building from Source
Clone the repository:

```bash
git clone https://github.com/egoboo/egoboo
cd egoboo
git submodule update --init data external idlib idlib-game-engine
```

For normal Egoboo development, only the root submodules are required.
The superproject passes the top-level `idlib/` into `idlib-game-engine` during
the CMake build, so `idlib-game-engine/idlib` does not need to be initialized.

Supported build documents:

- Linux: `doc/build-linux.md`
- Windows: `doc/build-windows.md`

Quick start:

Linux:

```bash
cmake -S . -B build
cmake --build build -j4
```

Windows with Visual Studio:

```bash
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Release -j4
```

Fedora cross-build to Windows:

```bash
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/mingw-w64-x86_64.cmake"
cmake --build build-windows -j4
```

Run the cross-built Windows binary through Wine from Linux:

```bash
./run-egoboo-windows.sh
```

The Wine helper currently applies compatibility defaults for the Windows build:
`EGOBOO_DISABLE_MIPMAPS=1` and `EGOBOO_DISABLE_AUDIO=1`.

Important output directories:

- Linux binaries: `build/products/x64/bin/`
- Windows Visual Studio binaries: `build-vs/products/release/x64/bin/`
- Fedora cross-built Windows binaries: `build-windows/products/x64/bin/`

#### Appveyor CI Build Status
- [master](https://github.com/egoboo/egoboo/tree/master) branch Windows 11:
[![Build status](https://ci.appveyor.com/api/projects/status/7sjmdgolmvmv3hc1/branch/master?svg=true)](https://ci.appveyor.com/project/michaelheilmann-com/egoboo-windows/branch/master)

- [master](https://github.com/egoboo/egoboo/tree/master) branch Linux (Ubuntu):
[![Build status](https://ci.appveyor.com/api/projects/status/8u6ubxw52foc2rat/branch/master?svg=true)](https://ci.appveyor.com/project/michaelheilmann-com/egoboo-linux/branch/master)
