# Compositor for Windows

The Windows implementation of Compositor, a port of the macOS Swift app. It shares the `.comp`
project format and is written in portable C++20 with a Qt6 Widgets client. See
[AGENTS.md](AGENTS.md) for the rules that apply to this tree.

## Status

Work in progress. Milestone **W0** (the `.comp` format: model, strict validation, package
read/write) is done. Milestone **W1** is underway:

- the CPU compositing core (blend modes, layer placement, masks, folders) and image codecs
  (PNG/JPEG) exist, and a `.comp` round trips end to end;
- all twelve adjustment kinds are ported: Invert, Levels, Exposure, Gradient Map, Black & White,
  Color Balance, Grain, Add Noise, Hue/Saturation and Curves run through the macOS app's own C
  kernels (Hue/Saturation through its 33-point color cube, Curves through its per-channel lookup);
  Gaussian Blur and Motion Blur are a pure C++ separable/directional Gaussian matching the macOS
  Core Image behaviour. An adjustment layer's own raster mask clips where its effect applies, and it
  reaches the whole canvas below it — folders are pass-through, so they do not bound it, matching the
  macOS app's live renderer;
- layer effects are ported: Stroke, Drop Shadow, Color Overlay, Inner Shadow, Outer Glow and Inner
  Glow, rendered on a padded copy of the layer (grown transform) with the macOS app's morphology,
  coverage and clamped-mask blur. A layer's linked mask shapes the effects; the GPU effects path and
  the result cache are not ported (the CPU path recomputes each frame);
- the Camera Raw tone and color-noise kernels match the macOS app: Shadows and Highlights use the same
  tone bump that keeps tones in order, and color noise is smoothed over each pixel's brightness rather
  than its saturation.

Milestone **W2** has started: a Qt6 Widgets desktop client under `app/win` that opens a `.comp`,
composites it, shows it on a canvas (zoom with Ctrl+wheel, drag to pan, View > Fit/Actual), lists
several documents open at once, one tab each, with remappable keyboard shortcuts (Edit > Keyboard
Shortcuts, saved per user). Fast brush/heal/clone drags are coalesced to one re-composite per frame. It shows the layers with visibility checkboxes, and
edits them: a Move tool, an opacity spinner, a Brush
(size, color, erase), and a coverage-mask selection shared by rectangle, ellipse, freehand and
polygonal Lasso and a Magic Wand, with Shift/Alt add/subtract and Select All/Deselect, Inverse, Grow,
Shrink and Feather feeding Delete, Fill and Crop to Selection. An Eyedropper picks colors off the
canvas. It also has New/Duplicate/Delete Layer, New Folder, Rename, Bring Forward/Send Backward,
flip/rotate/reset transforms, Flip Canvas, and Flatten Image — all re-compositing live and written
back to the `.comp` on save, with a snapshot-based Undo/Redo. It imports PNG/JPEG as a new layer (or
a new document), imports 8-bit RGB Photoshop files (layers, folders, masks, blend modes) as a new
document, and exports PNG or JPEG (Copy Merged to the clipboard). Adjustment layers can be created
and edited (New Adjustment Layer / Adjustment Properties), the Filter menu bakes filters into a
layer — among them Dither and Scanlines, a CRT look with line spacing, a beam thickness, wobble,
displacement into the picture's own shapes, dots, a color split and a phosphor glow — Layer Mask and
clipping-mask actions are wired up, and Image menu has Canvas Size, Image Size
and Trim, and View has a layout Grid and alignment Guides (drawn as overlays; a layer being moved
snaps to guides, the grid, the canvas edges and the other layers). A Crop tool drags a rectangle to
crop to. A linear Gradient
tool, rectangle/ellipse shape fills, a Spot Healing brush and an aligned Clone Stamp and a Blur brush are also in. The canvas renders on the GPU through Qt's OpenGL paint engine (`QOpenGLWidget`);
a Skia backend is planned but is not wired up yet — building Skia on Windows needs downloads that the
current proxy blocks, so `CanvasView` is kept behind a small interface a Skia renderer can replace.

## Layout

| Path | Contents |
| --- | --- |
| `core/model` | `.comp` manifest model, validation, package read/write (no Qt, no codecs) |
| `core/render` | CPU compositing: blend math, placement, masks, folders, flattening |
| `core/io` | PNG/JPEG decode and encode (libpng, libjpeg-turbo via vcpkg) |
| `core/pixel` | portable C pixel kernels reused verbatim from the macOS app |
| `app/win` | the Qt6 Widgets desktop client (canvas, layers panel, open/save/export) |
| `tests` | GoogleTest suites |
| `tools` | developer scripts (`sizelint.py`) |
| `third_party` | vendored header-only dependencies (nlohmann/json) |

## Build and test

Requires CMake 3.24+, a C++20 compiler, and [vcpkg](https://github.com/microsoft/vcpkg) for libpng
and libjpeg-turbo. On Windows, the CMake that ships with Visual Studio 2022 works, as does `cmake`
from PATH.

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

vcpkg installs the dependencies listed in `vcpkg.json` on first configure; GoogleTest is fetched by
CMake too, so the first build needs network access.

### Desktop client (optional)

Needs Qt6 Widgets. The prebuilt official binaries are quickest:

```sh
python -m pip install aqtinstall
python -m aqt install-qt windows desktop 6.8.0 win64_msvc2022_64 -O C:/Qt
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DCOMP_BUILD_APP=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.8.0/msvc2022_64
cmake --build build --config Debug --target compositor_win
```

Run it with the Qt runtime beside it (`windeployqt build/app/win/Debug/compositor_win.exe`) or with
`<Qt>/bin` on `PATH`. `compositor_win --open-check <project.comp>` opens, composites and prints the
size without a window, for smoke tests.

### Packaging

Build the Release app, then bundle it (Qt and the MSVC runtime are copied beside the executable and
the folder is zipped):

```sh
cmake --build build --config Release --target compositor_win
powershell -ExecutionPolicy Bypass -File app/win/package.ps1 -QtDir C:/Qt/6.8.0/msvc2022_64 -Version 1.0.0
```

The result is `dist/Compositor-<version>-win64.zip`, a self-contained bundle that runs on a
machine without Qt or Visual Studio. CI does the same in `.github/workflows/windows-app.yml` and
uploads the zip as an artifact.

## Checks

```sh
python tools/sizelint.py --root .
clang-format --dry-run --Werror --style=file <files>
```

The `clang-format` version is pinned in CI (19.1.5) so its output is stable.
