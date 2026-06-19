# CLAUDE.md

Guidance for working in this repository.

## What this is

`Zs.DWFToolkit` is a **.NET library for DWF/DWFx editing and preview** — package
inspection, thumbnail extraction, page/format conversion, and a C++ native bridge
that rasterizes DWF/W2D vector graphics for preview overlays. It is a library +
CLI demo, not an application or a signing system.

Core principle: **never pass off an embedded thumbnail or partial raster preview as
a real page conversion.** Plain `.dwf` page rendering requires the native W2D/DWF
renderer; when it is unavailable, conversion returns `unsupported_dwf_rendering`
instead of a misleading thumbnail.

## Layout

```
src/Zs.DWFToolkit/            Managed library (net8.0)
  Abstractions/              Service interfaces (reader, converter, thumbnail, native renderer)
  Models/                    DTOs + DwfErrorCodes (single error vocabulary)
  Internal/                  FileKindDetector, ImageProbe, PackageEntryClassifier, ProcessRunner, SafePath
  Conversion/                DwfxExternalConverter, DwfThumbnailExtractor, SimpleImagePdfWriter
  Native/                    P/Invoke bindings (NativeMethods) + NativeDwfRenderer
  Services/                  ZsDwfToolkit facade, DwfDocumentReader
src/Zs.DWFToolkit.Native/    C++ C-ABI bridge (CMake); W2D renderer + ODA adapter
samples/Zs.DWFToolkit.CliDemo/  CLI demo (info/thumbnail/to-images/to-pdf/render-w2d)
tests/Zs.DWFToolkit.Tests/   xUnit + FluentAssertions (managed coverage)
docs/                        Design, conversion, native integration, W2D rendering, roadmap
third_party/DWFToolkit-7.7/  ODA-modified DWF Toolkit source (gitignored; imported locally)
```

## Build / test / run

```bash
# Managed build (always works on macOS/Linux/Windows)
dotnet build Zs.DWFToolkit.slnx

# Tests (+ coverage)
dotnet test Zs.DWFToolkit.slnx
dotnet test Zs.DWFToolkit.slnx --collect:"XPlat Code Coverage"

# CLI demo
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- info ./drawing.dwfx
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-images ./drawing.dwfx --out-dir out --dpi 200
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-pdf ./drawing.dwfx --out out.pdf

# Native bridge — default (stub W2D, no real DWF rendering)
./scripts/build-native.sh

# Native bridge — real W2D rendering via ODA toolkit (Linux/CI; needs third_party/DWFToolkit-7.7)
cmake -S src/Zs.DWFToolkit.Native -B build/native-oda -DZS_DWF_ENABLE_ODA_DWFTK=ON
cmake --build build/native-oda --target zs_dwf_toolkit_native --config Release
```

After a native build, copy `libzs_dwf_toolkit_native.{so,dylib}` /
`zs_dwf_toolkit_native.dll` next to the managed output so P/Invoke can load it.

## Conventions

- **Error codes**: always use constants from `Models/DwfErrorCodes.cs`. Managed and
  native layers share this vocabulary; do not invent inline error strings.
- **native ↔ managed contract**: the native renderer returns JSON deserialized into
  `DwfRenderResult` / `DwfDocumentInfo` (`PropertyNameCaseInsensitive`). New native
  JSON fields bind by matching the C# property name. The render JSON carries a
  `transform` object → `DwfPageTransform` for mapping drawing coords to pixels.
- **Reuse**: PDF assembly → `SimpleImagePdfWriter`; image sniffing → `ImageProbe`;
  process exec → `ProcessRunner`; path safety → `SafePath`.
- **DWFx/XPS** convert via external `mutool` (preferred) or `gxps`; **plain DWF**
  converts only through the native renderer.
- **Tests**: xUnit + FluentAssertions, AAA layout, behavior-named. Fixtures are
  hand-built in `TestData.cs` (no platform image codec dependency).
- **Commits**: Conventional Commits; attribution disabled globally.

## Native rendering boundary (important)

The native W2D renderer is a **first-pass** renderer, not a full CAD renderer.
Implemented: polyline, polygon, ellipse (outline/filled), text placeholder box,
raw RGB/RGBA images, page transform. **Not yet**: real glyphs, compressed/mapped
images, clipping/viewport, line/fill/hatch patterns, layers, 3D (eModel). See
`docs/W2D_RENDERING.md` and `docs/ROADMAP.md`. Output is for preview/edit
validation, not archival-grade conversion. The ODA path (`ZS_DWF_ENABLE_ODA_DWFTK`)
builds on Linux/CI; the default macOS build compiles the renderer as a stub.

## Where to extend rendering

- New W2D primitive: add a `WT_*` callback in
  `src/Zs.DWFToolkit.Native/src/w2d/W2dWhipRenderer.cpp` (register in `process_w2d`),
  draw via a primitive in `src/render/RasterCanvas.{h,cpp}`.
- New native info field: extend `OdaDwfAdapter::get_dwf_info_json`, then the matching
  C# model property.
