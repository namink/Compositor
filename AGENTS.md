# Compositor cross-platform core (Windows first)

This repository is the Windows implementation of Compositor. It is a **separate codebase** from the
macOS Swift app; it does not import Swift or Apple frameworks. The macOS app (upstream
`robbietilton/Compositor`) remains the reference for behaviour while the port is validated.

## What lives here

- `core/model` — the `.comp` project format: manifest model, strict validation, package read/write.
- `core/pixel` — the portable C pixel kernels, reused verbatim from the macOS app's renderer.
- `app/win` — the Qt6 Widgets desktop client.

## Language and build

- C++20, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- CMake + GoogleTest. Heavy third-party code (Qt6, Skia, libpng, LibRaw, …) arrives later via vcpkg;
  `nlohmann/json` is vendored under `third_party/` because it is small and header-only.
- Build: `cmake -S . -B build -G Ninja` then `cmake --build build` then `ctest --test-dir build`.

## Rules that are enforced, not requested

These are checked by CI (see `.github/workflows/windows.yml`) and by `tools/sizelint.py`:

1. **No oversized files.** Soft limit 400 lines, hard limit 600 lines for `.cpp/.hpp/.h`. UI files
   under `app/win` hard-limit at 500. Vendored code (`third_party/`, `core/pixel/`) is exempt.
   Anything over the hard limit fails the build unless listed in `.sizelint-allowlist` with a reason.
2. **`clang-format` clean.** Style is `.clang-format` (Google base, 4-space indent, 120 columns).
3. **`clang-tidy`** (`.clang-tidy`) is advisory until the tree is clean, then becomes blocking. Its
   checks are `bugprone-*`, `performance-*`, `modernize-*` and the size limits.
4. **Tests pass.**

## Code style

- American spelling everywhere ("color", not "colour"), matching the macOS app.
- Files `snake_case.cpp` / `snake_case.hpp`; types `PascalCase`; functions, variables and members
  `snake_case`; constants `kPascalCase`. Namespaces: `compositor::model`, `compositor::render`, …
- `#pragma once`, include-what-you-use, no `using namespace` in headers, no implementation in a
  header except templates.
- RAII only: no bare `new`/`delete`. No mutable global state.
- Comments explain **why**, not **what**; match the density of the macOS sources.

## Correctness rules for the project format

- The `.comp` format is the contract with the macOS app and with AI agents that write projects.
  Its specification is the upstream macOS app's `docs/project-format.md`; when a change alters what
  is saved, update that spec **and** `kCurrentFormatVersion` here.
- Validation is strict: unknown versions, illegal paths, missing assets, oversized data and bad
  metadata reject the whole package rather than loading a partial document.
- All coordinates are document pixels, y down. Colors and compositing are sRGB (non-linear),
  premultiplied alpha, matching the macOS app. Do not "fix" this to linear light.
