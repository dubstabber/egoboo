# Egoboo

Egoboo is an open-source 3D dungeon crawler in the spirit of NetHack.
The current maintained build documents in this repository cover Linux and
Windows.

### License
Egoboo is made available publicly under the
[GNU GPLv3 License](https://github.com/dubstabber/egoboo/LICENSE).

### Contact
Developers can usually be contacted via GitHub.

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

Older platform-specific `README.*` files are still present in the repository,
but the `doc/build-*.md` files are the current source of truth for active build
paths.

Quick start:

Linux:

```bash
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
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
