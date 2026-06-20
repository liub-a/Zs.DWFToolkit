# CLAUDE.md

Guidance for working in this repository.

## What this is

`Zs.DWFToolkit` is a **.NET library for DWF/DWFx editing and preview** — package
inspection, thumbnail extraction, page/format conversion, and a C++ native bridge
that rasterizes DWF/W2D vector graphics to PNG. It is a library + CLI demo.

Core principle: **never pass off an embedded thumbnail or partial raster preview as
a real page conversion.** Plain `.dwf` page rendering goes through the native
W2D/DWF renderer; when it is unavailable, conversion returns
`unsupported_dwf_rendering` instead of a misleading thumbnail. 3D-only (eModel)
DWF returns `unsupported_3d_dwf`.

## Layout

```
src/Zs.DWFToolkit/            Managed library (net8.0)
  Abstractions/              Service interfaces (reader, converter, thumbnail, native renderer)
  Models/                    DTOs + DwfErrorCodes (single error vocabulary)
  Internal/                  FileKindDetector, DwfPackage, ImageProbe, PackageEntryClassifier, ProcessRunner, SafePath
  Conversion/                DwfxExternalConverter, DwfThumbnailExtractor, SimpleImagePdfWriter
  Native/                    NativeMethods, NativeDwfRenderer (in-process), ProcessNativeDwfRenderer (out-of-process)
  Services/                  ZsDwfToolkit facade, DwfDocumentReader
src/Zs.DWFToolkit.Native/    C++ C-ABI bridge (CMake)
  src/render/               RasterCanvas (primitives + AA downsample), MinimalPng (read/write)
  src/w2d/                  W2dWhipRenderer (WHIP opcode -> canvas)
  src/dwf/                  OdaDwfAdapter (package -> per-page W2D, get_info)
  src/text/                 TextRenderer (FreeType glyphs, embedded font)
  src/image/                Group4Decoder (CCITT T.6 via libtiff)
  tools/                    zs_dwf_worker (out-of-process render executable)
  tests/                    CTest native tests
samples/Zs.DWFToolkit.CliDemo/  CLI demo (info/thumbnail/to-images/to-pdf/render-w2d)
tests/Zs.DWFToolkit.Tests/   xUnit + FluentAssertions (managed coverage)
tests/data/                  Committed regression fixtures (ocean_90.w2d, IRD-Addition.dwf)
docs/                        Design, conversion, native integration, W2D rendering, roadmap
third_party/                 Vendored sources: freetype, libtiff, zlib, fonts/DejaVuSans.ttf;
                             DWFToolkit-7.7 (ODA, gitignored; imported from archives/ tarball)
```

## Build / test / run

```bash
# Managed build + tests (always work on macOS/Linux/Windows)
dotnet build Zs.DWFToolkit.slnx
dotnet test  Zs.DWFToolkit.slnx          # 42 managed tests

# CLI demo
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- info ./drawing.dwf
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-images ./drawing.dwf --out-dir out --native

# Native renderer (real W2D). All third-party libs are vendored — only cmake/g++ needed.
# First time: import the ODA toolkit source from the tracked archive.
./scripts/setup-oda-dwftoolkit.sh
cmake -S src/Zs.DWFToolkit.Native -B build/native-oda \
      -DZS_DWF_ENABLE_ODA_DWFTK=ON -DZS_DWF_WITH_FREETYPE=ON -DZS_DWF_WITH_TIFF=ON -DZS_DWF_BUILD_TESTS=ON
cmake --build build/native-oda -j 2          # -j unbounded OOM-kills (each WhipTk TU needs ~GBs)
ctest --test-dir build/native-oda            # 5 native tests (use committed fixtures, no network)

# Container (full native + managed, ready to convert)
docker build -t zs-dwftoolkit .
```

After a native build, copy `libzs_dwf_toolkit_native.{so,dylib}` and
`zs_dwf_worker` next to the managed output (P/Invoke + the worker).

## Conventions

- **Error codes**: always use constants from `Models/DwfErrorCodes.cs`. Managed and
  native layers share this vocabulary; do not invent inline error strings.
- **native ↔ managed contract**: the native renderer returns JSON deserialized into
  `DwfRenderResult` / `DwfDocumentInfo` (`PropertyNameCaseInsensitive`). New native
  JSON fields bind by matching the C# property name. The render JSON carries a
  `transform` object → `DwfPageTransform` for mapping drawing coords to pixels.
- **Reuse**: ZIP/OPC open (incl. DWF v6 header) → `DwfPackage`; PDF assembly →
  `SimpleImagePdfWriter`; image sniffing → `ImageProbe`; process exec → `ProcessRunner`;
  path safety → `SafePath`.
- **DWFx/XPS** convert via external `mutool` (preferred) or `gxps`; **plain DWF**
  converts only through the native renderer.
- **Rendering = native C++**: pixel work lives in `RasterCanvas`; W2D opcode
  dispatch in `W2dWhipRenderer`. Keep new native JSON behind `#ifdef ZS_DWF_WITH_ODA_DWFTK`
  (the non-ODA branch must still compile and return an unsupported result).
- **Tests**: xUnit + FluentAssertions (managed); CTest (native, dependency-light
  asserts + committed-fixture regression). Behavior-named, AAA.
- **Commits**: Conventional Commits; attribution disabled globally.

## Native rendering (current)

Real first-pass W2D renderer, validated end-to-end on real DWF packages.
**Implemented**: polyline, polygon, triangle strips, contour sets (even-odd holes),
ellipses, real text glyphs (FreeType + embedded DejaVu), polymarkers, dashed/dotted
lines, hatch fills, layer visibility, viewport clipping (bbox), anti-aliasing (2×
supersample), page transform, and images (RGB/RGBA, mapped/indexed, JPEG,
bitonal/Group3X/Group4X). **Not implemented**: full macro geometry replay
(placements are marked), non-rectangular clipping, exact font matching, 3D (eModel).

## Self-contained build

FreeType, libtiff, zlib and the DejaVu font are vendored in `third_party/` and
built from source — no network, no system libs. libjpeg prefers a system
jpeg-turbo when present (`ZS_DWF_PREFER_SYSTEM_JPEG`, default ON) and otherwise
builds the libjpeg-6b sources the ODA toolkit bundles. The resulting library links
only the C/C++ runtime.

## Out-of-process rendering (isolation)

`ProcessNativeDwfRenderer` (managed) runs the native `zs_dwf_worker` via
`ProcessRunner` with the render timeout. A worker crash → `native_render_failed`;
a hang → killed → `timeout`. A bad DWF or native fault cannot take down the host.
It is a drop-in `INativeDwfRenderer` for `DwfxExternalConverter` (set
`ZS_DWF_WORKER` or place the worker beside the app).

## Where to extend rendering

- New W2D primitive: add a `WT_*` callback in `src/w2d/W2dWhipRenderer.cpp` (register
  in `process_w2d`), draw via a method on `src/render/RasterCanvas.{h,cpp}`, and add
  a native CTest assertion. Verify the ODA build still links.
- New native info field: extend `OdaDwfAdapter::get_dwf_info_json`, then the matching
  C# model property.
