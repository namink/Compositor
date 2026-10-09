# Vendored third-party code

Small, header-only dependencies committed here so the core builds without a package manager.

- `nlohmann/json.hpp` — JSON.parse/serialize, used for `.comp` manifests. MIT license.
  Version 3.11.3, the single-header release. Update by replacing the file; do not edit it.

Heavier dependencies (Qt6, Skia, libpng, libtiff, libheif, LibRaw, ONNX Runtime) are **not**
vendored. They are declared in `../vcpkg.json` and resolved by vcpkg when those modules land.

Everything under this directory is exempt from `tools/sizelint.py`.
