# Development Guide

[Back to README](../README.md)

## Get the Source

The source repository is declared in [`lpm.toml`](../lpm.toml):

```sh
git clone --recurse-submodules https://github.com/LILYGO-UI/gallery.git lilygo-ui-gallery
cd lilygo-ui-gallery
```

For an existing checkout, initialize the pinned dependencies:

```sh
git submodule update --init --recursive
```

AppKit is consumed directly from `third_party/cm0-appkit`; the simulator does
not require a separate AppKit installation. Both presets use its bundled
LVGL 9.5.0 archive with LVGL network fetching disabled. Run the remaining
project commands from the repository root.

## Host Dependencies

Use CMake 3.21 or later (including CTest and CPack), Python 3.11 or later for
LPM, and a C/C++ compiler with C++17 support. The presets use Unix Makefiles.
AppKit requires `pkg-config`, `patch`, SDL2 for the host display, and FreeType
for font rendering. libjpeg-turbo development headers and libraries are required
for Gallery's JPEG decoder. See [image formats](02-images-and-storage.md#image-formats-and-thumbnails)
for the supported JPEG formats.

### Debian 12 / Ubuntu 24.04 or Later

```sh
sudo apt update
sudo apt install build-essential cmake git pkg-config patch libsdl2-dev \
  libfreetype6-dev python3 python3-venv python3-pip
```

For libjpeg-turbo support:

```sh
sudo apt install libjpeg62-turbo-dev
```

On older distributions, upgrade CMake and Python to the minimum versions above.

### macOS

Install the Xcode Command Line Tools if needed, then install dependencies with
[Homebrew](https://brew.sh/). Complete Homebrew's shell setup so its tools are
on `PATH`:

```sh
xcode-select --install
brew install git cmake pkg-config patch sdl2 freetype python
```

For libjpeg-turbo support:

```sh
brew install jpeg-turbo
```

### LPM

CMake invokes `lpm metadata --format cmake` during configuration. Install LPM
in a project-local environment using Python 3.11 or later:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install "git+https://github.com/LILYGO-UI/lpm.git"
lpm --version
```

Reactivate the environment with `. .venv/bin/activate` in each new terminal
before running `lpm`. CMake searches `.venv/bin` as well as `PATH`. An existing
installation can be selected with `-DLPM_EXECUTABLE=/absolute/path/to/lpm`
during configuration.

Check the host environment before building:

```sh
cmake --version
python --version
pkg-config --modversion sdl2 freetype2
lpm metadata --format cmake
```

## Desktop Simulator

```sh
lpm start --foreground
```

This configures and builds `host-simulator`, then runs the app in the foreground.
The Debug build enables tests and is stored in `build/host-simulator`.
Running the SDL2 simulator requires a graphical desktop session. To reuse an
existing build, run `lpm start --no-build --foreground`.

Gallery uses local files and can run offline. An empty host library loads the
repository samples automatically. See [images and storage](02-images-and-storage.md)
for custom directories and the distinction between packaged samples and fallback
samples.

## Tests and Rendering

Configure and build the host simulator, then run all tests:

```sh
lpm test
```

| Test | Coverage |
| --- | --- |
| `gallery-core` | Directory scanning, newest-first ordering, selection, deletion, read-only samples, and host fallback |
| `gallery-view-model` | Collection filtering, user commands, grid/viewer state, and reactive Subject updates |
| `gallery-image` | PNG/JPEG decoding, matching PNG/PPM pixels, cover/contain scaling, and error backgrounds |
| `gallery-render-portrait` | Rendering and complete UI flows at `568 x 1232` |
| `gallery-render-landscape` | Rendering and complete UI flows at `1232 x 568` |

The render tests cover album filtering, filmstrip navigation, vertical swipes,
closing and reopening details, deletion confirmation, read-only samples, and
returning to the same grid scroll position. They also verify focus and scrolling
with 24 photos, resizing to compact displays, and keeping the selected filmstrip
thumbnail visible as the display narrows.

Snapshots are written under `build/host-simulator/` with
`gallery-portrait.ppm` or `gallery-landscape.ppm` as the base filename:

| Filename suffix | View |
| --- | --- |
| No extra suffix | Photo grid |
| `.albums.ppm` | Album overview |
| `.viewer.ppm` | Full-screen photo viewer |
| `.details.ppm` | Photo details |
| `.details-compact.ppm` | Photo details at `320 x 240` |
| `.delete.ppm` | Deletion confirmation |
| `.full.ppm` | Full library with 24 photos |
| `.host-samples.ppm` | Automatic host sample fallback |

Suffixes are appended to the complete base filename, for example
`gallery-portrait.ppm.viewer.ppm`. Inspect both orientations after UI changes;
passing assertions alone does not verify the visual layout. Follow the
[UI development guidelines](01-user-interface.md) for acceptance requirements.
Example screens are in [`assets/previews`](../assets/previews).

### Render the Sample Gallery

After building the host tests, the renderer can use the packaged PPM examples:

```sh
LILYGO_GALLERY_RENDER_SAMPLE_DIR="$PWD/assets/gallery" \
  build/host-simulator/gallery-render-ui 1232 568 /tmp/gallery.ppm assets/gallery/forest.jpg
```

Use `568 1232` for portrait output. The arguments are width, height, snapshot
path, and JPEG fixture path. `LILYGO_GALLERY_RENDER_SAMPLE_DIR` is a test-renderer
setting, separate from the app's `LILYGO_GALLERY_SAMPLE_DIR`. The renderer copies
`sunrise.ppm`, `forest.ppm`, and `night.ppm` from this directory into a temporary
test library and exercises its normal UI flow, producing additional snapshots
beside the requested output.

## CM0 Device Build

Device builds require a separate AArch64 Linux cross compiler on the development
machine. Skip this setup when only building the simulator.

**Debian / Ubuntu:**

```sh
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

**Apple Silicon macOS:**

```sh
brew install aarch64-unknown-linux-gnu
```

The BSP toolchain expects these executable names on `PATH`:

```sh
aarch64-linux-gnu-gcc --version
aarch64-linux-gnu-g++ --version
```

See [CM0BspBuilder](https://github.com/LILYGO-UI/CM0BspBuilder) for SDK host and
compiler requirements. The BSP provides target headers and libraries but does
not install the host cross compiler.

Build and package with:

```sh
lpm pack
```

The `cm0-cross` preset targets CM0 Raspberry Pi OS on AArch64, uses a Release
build, disables host tests, and sets the installation prefix to `/usr`.
The [toolchain wrapper](../cmake/cm0-bsp-toolchain.cmake) downloads CM0 BSP
`0.1.0`, verifies its pinned SHA-256 checksum, and extracts the sysroot to
`.cache/cm0-bsp/0.1.0/sysroot` on the first configuration. The download requires
access to GitHub; later configurations reuse the cache. Set
`-DCM0_BSP_CACHE_DIR=/absolute/path/to/cache` during configuration to change the
cache location. A sibling CM0BspBuilder checkout is not required.

Cross compilation needs target FreeType, DRM, and libjpeg-turbo runtime libraries
in the SDK sysroot. The LVGL decoder also needs a matching target development
package. If the BSP has only `libjpeg.so.62`, extract the matching Debian
`libjpeg-turbo` development package and configure with:

```sh
cmake --preset cm0-cross \
  -DLIBJPEG_TURBO_DEV_ROOT=/absolute/path/to/libjpeg-turbo-dev
```

`LIBJPEG_TURBO_DEV_ROOT` may contain `usr/include` and
`usr/lib/aarch64-linux-gnu` directly. CMake links the target library from that
root or from the BSP sysroot; it never links a host library into the CM0
executable. Configuration stops with an explicit error when the headers or
target library are missing.

CPack writes an ARM64 `.deb` to `dist/`. The package installs the executable,
launcher entry and icon, desktop entry, AppStream metadata, the gallery sample
import source, and shared-font license notices. During installation the Debian
maintainer script copies supported sample images into the target user's
`$HOME/Pictures` without overwriting existing files. The target system must provide `libc6`,
`libstdc++6`, `libdrm2`, `libfreetype6`, and `lilygo-ui-appkit-dev >= 0.1.0`.
The package requires `libjpeg62-turbo` at runtime. AppKit supplies
the shared runtime fonts on the device. Package identity and version are
maintained in [`lpm.toml`](../lpm.toml).

## Direct CMake Commands

LPM wraps the following steps. They can also be run directly when debugging
build configuration; LPM is still required to read project metadata.

Host build and tests:

```sh
cmake --preset host-simulator
cmake --build --preset host-simulator --parallel
ctest --preset host-simulator
```

Device build and packaging:

```sh
cmake --preset cm0-cross
cmake --build --preset cm0-cross --parallel
cpack --config build/cm0-cross/CPackConfig.cmake -B dist
```

## Troubleshooting

| Symptom | Action |
| --- | --- |
| `lpm is required to read project metadata` | Install LPM in `.venv`, put it on `PATH`, or set `LPM_EXECUTABLE`. |
| `LilyGoUI` or bundled LVGL cannot be found | Initialize the pinned AppKit submodule with `git submodule update --init --recursive`. |
| SDL2 or FreeType is missing | Install host development packages; for cross builds, check target FreeType and DRM in the sysroot. |
| Homebrew libraries are not found | Configure with `cmake --preset host-simulator -DCMAKE_PREFIX_PATH="$(brew --prefix)"`. |
| BSP download fails | Check access to GitHub releases and available disk space, then rerun configuration. |
| Photos do not appear, or only samples appear | Check source directory overrides, permissions, supported formats, and the 24-item limit; press refresh. See [images and storage](02-images-and-storage.md). |
| A JPEG cannot be opened | Check for corrupt or oversized data. Gallery uses LVGL's libjpeg-turbo decoder and accepts baseline and progressive JPEGs; rebuild if the target development package is missing. |
| A sample cannot be deleted | Samples, including files selected by a sample-directory override, are intentionally read-only. |
