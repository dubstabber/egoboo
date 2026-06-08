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

Linux:

```bash
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Windows cross-build with `mingw-w64`:

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
