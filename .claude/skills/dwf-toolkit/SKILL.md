---
name: dwf-toolkit
description: Work on the Zs.DWFToolkit DWF/DWFx edit-and-preview library — build/test commands, the managed↔native C-ABI contract, the error-code vocabulary, ODA/W2D rendering boundaries, and how to extend rendering. Use when editing this repo's managed library, CLI, native bridge, or tests.
---

# Zs.DWFToolkit

.NET 8 library for **DWF/DWFx editing and preview**: package inspection, thumbnail
extraction, DWFx/XPS conversion via external tools, and a C++ native bridge that
rasterizes DWF/W2D vector graphics. Managed code is fully cross-platform; the real
native renderer needs the ODA toolkit and builds on Linux/CI.

## Golden rule

Never treat an embedded thumbnail or partial raster preview as a real page
conversion. Plain `.dwf` rendering goes through the native renderer only; when it is
unavailable, return `unsupported_dwf_rendering` — never a thumbnail standing in for a page.

## Commands

```bash
dotnet build Zs.DWFToolkit.slnx                 # managed build
dotnet test  Zs.DWFToolkit.slnx                 # 34+ xUnit tests
dotnet test  Zs.DWFToolkit.slnx --collect:"XPlat Code Coverage"

dotnet run --project samples/Zs.DWFToolkit.CliDemo -- info ./file.dwfx
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-images ./file.dwfx --out-dir out --dpi 200
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- to-pdf ./file.dwfx --out out.pdf
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- render-w2d ./sheet.w2d --out page.png --native

./scripts/build-native.sh                        # default native (stub W2D)
cmake -S src/Zs.DWFToolkit.Native -B build/native-oda -DZS_DWF_ENABLE_ODA_DWFTK=ON
cmake --build build/native-oda --target zs_dwf_toolkit_native --config Release
```

## Architecture

- `Services/ZsDwfToolkit` — facade over reader + thumbnail + converter.
- `Services/DwfDocumentReader` — ZIP/OPC inspection; discovers XPS fixed pages,
  W2D pages, then W3D (3D) pages; attaches thumbnails.
- `Conversion/DwfxExternalConverter` — DWFx/XPS via `mutool`/`gxps`; plain DWF via
  native renderer → optional image→PDF assembly.
- `Conversion/SimpleImagePdfWriter` — dependency-free PDF from JPEG / RGB-or-gray PNG.
- `Native/NativeDwfRenderer` + `Native/NativeMethods` — P/Invoke to the C ABI.
- `src/Zs.DWFToolkit.Native` — C++: `zs_dwf_toolkit_native.cpp` (ABI),
  `dwf/OdaDwfAdapter` (ODA package reader + get_info), `w2d/W2dWhipRenderer` (WHIP
  opcode → canvas), `render/RasterCanvas` + `render/MinimalPng`.

## Contracts & conventions

- **Error codes**: only constants from `Models/DwfErrorCodes.cs`. Managed + native
  share this vocabulary (e.g. `unsupported_dwf_rendering`, `unsupported_3d_dwf`,
  `w2d_render_failed`, `native_library_not_found`). No inline error strings.
- **JSON binding**: native returns JSON → `DwfRenderResult` / `DwfDocumentInfo`
  (`PropertyNameCaseInsensitive`). Match the C# property name to add a field. The
  render JSON's `transform` object maps to `DwfPageTransform` (drawing coords → pixels).
- **Reuse**: `ImageProbe` (sniff), `ProcessRunner` (exec), `SafePath` (paths),
  `SimpleImagePdfWriter` (PDF), `PackageEntryClassifier` (entry roles).
- **Tests**: xUnit + FluentAssertions, AAA, behavior names; fixtures hand-built in
  `tests/.../TestData.cs` (PNG/JPEG/BMP headers + OPC zips, no platform codec).
- **Commits**: Conventional Commits; attribution disabled.

## Rendering maturity

| W2D primitive | Status |
|---|---|
| polyline / polygon / ellipse | done |
| page transform (scale/offset/flipY) | done |
| text | placeholder box (no real glyphs) |
| image | raw RGB/RGBA blitted; mapped/Group4/JPEG → gray placeholder |
| clipping, line/fill/hatch patterns, layers, 3D | not implemented |

Output is preview/edit grade, not archival. ODA path builds on Linux/CI; macOS
default build compiles the native renderer as a stub returning `unsupported_dwf_rendering`.

## Extending

- **New W2D primitive**: add a `WT_*` callback in `w2d/W2dWhipRenderer.cpp`, register
  it in `process_w2d`, and draw via a method on `render/RasterCanvas`.
- **New native info field**: extend `OdaDwfAdapter::get_dwf_info_json`, then add the
  matching property on the C# model.
- Keep new native JSON emission behind `#ifdef ZS_DWF_WITH_ODA_DWFTK`; the non-ODA
  branch must still compile and return an unsupported result.
