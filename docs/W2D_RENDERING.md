# W2D Rendering

`Zs.DWFToolkit` now contains a first-pass native W2D renderer. It is intentionally small and dependency-light: the renderer uses the ODA-modified Autodesk DWF Toolkit / WHIP Toolkit to parse W2D opcodes, then paints a subset of 2D primitives into an in-memory RGBA canvas and writes PNG directly.

This is **not yet a full CAD/DWF renderer**. It is the first production-oriented step that replaces the earlier unsafe raster/thumbnail fallback behavior.

## Build requirements

Native W2D rendering requires the ODA-modified DWF Toolkit source to be available at:

```text
third_party/DWFToolkit-7.7
```

Then build the native library with:

```bash
cmake -S src/Zs.DWFToolkit.Native -B build/native-oda -DZS_DWF_ENABLE_ODA_DWFTK=ON
cmake --build build/native-oda --target zs_dwf_toolkit_native --config Release
```

The default native build still works without ODA, but in that mode W2D/DWF rendering returns `unsupported_dwf_rendering`.

## Exposed native APIs

```c
int zs_w2d_render_file(
    const char* input_path,
    const char* output_path,
    int width_px,
    int height_px,
    int dpi,
    char* output_json,
    int output_json_size);
```

```c
int zs_dwf_render_page(
    const char* input_path,
    int page_index,
    const char* output_path,
    int width_px,
    int height_px,
    int dpi,
    char* output_json,
    int output_json_size);
```

`zs_dwf_render_page` can render a plain `.w2d` stream directly. For `.dwf` packages it tries to locate the ePlot section, extract its 2D W2D graphics resource, and then render that W2D stream.

## CLI usage

Render a raw W2D stream:

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- \
  render-w2d sample.w2d --out page.png --width 2400 --height 1600
```

Render a DWF page through the normal conversion entry:

```bash
dotnet run --project samples/Zs.DWFToolkit.CliDemo -- \
  to-images sample.dwf --out-dir out --native --width 2400 --height 1600
```

## Currently implemented primitives

The first renderer supports:

| W2D object | Current behavior |
|---|---|
| `WT_View` | Used to collect page bounds |
| `WT_Polyline` | Drawn as solid lines |
| `WT_Polygon` | Filled and outlined |
| `WT_Outline_Ellipse` | Drawn as ellipse outline |
| `WT_Filled_Ellipse` | Filled ellipse |
| `WT_Text` | Real glyphs via FreeType when built with `ZS_DWF_WITH_FREETYPE` and a system font is found; otherwise a placeholder box |
| `WT_Image` | RGB/RGBA, color-mapped/indexed (palette), and JPEG decoded to the page rect; bitonal/Group3X/Group4 fax draw a gray placeholder |
| `WT_Viewport` | Clips subsequent drawing to the axis-aligned bounding box of the boundary contour |
| `WT_Line_Pattern` | Non-solid polyline strokes rendered as approximate dash/dot cadences |

The PNG writer is implemented internally and writes RGBA PNG with uncompressed DEFLATE blocks. This keeps the native renderer independent from libpng/zlib packaging concerns.

## Important limitations

The renderer is still a minimal W2D renderer. It does **not** yet fully support:

- exact font matching/metrics (FreeType renders ASCII glyphs from a system font; the original CAD font, kerning, rotation and non-ASCII are not matched);
- bitonal / Group3X / Group4 fax images (`WT_PNG_Group4_Image`) — RGB/RGBA, color-mapped and JPEG are decoded; fax encodings still draw a gray placeholder;
- non-rectangular clipping (viewport clips are approximated by their bounding box);
- exact line-pattern tables (dash/dot are approximated), fill patterns, hatch patterns, pen patterns;
- layer visibility controls;
- macro draw/definition expansion;
- exact AutoCAD plot style reproduction;
- antialiasing;
- 3D DWF/eModel rendering.

Because of these limitations, output is suitable for early validation and sample-driven renderer development, not final legal/archival conversion yet.

## Validation status

In the current development environment:

- Native build without ODA: passed.
- Native build with ODA source and `ZS_DWF_ENABLE_ODA_DWFTK=ON`: passed on Linux and produced `libzs_dwf_toolkit_native.so`.
- C# managed build was not executed because the environment does not have the .NET SDK installed.
- No real customer W2D/DWF samples were available in the environment, so visual fidelity needs sample-based validation.

## Next implementation steps

Recommended order:

1. Decode compressed/mapped images (`WT_PNG_Group4_Image`, JPEG, color-mapped); raw RGB/RGBA is already blitted.
2. Add proper text rendering using FreeType, Skia, Cairo, or another font backend.
3. Add clipping/viewport state and transform stack support.
4. Add line styles, fill styles and hatch patterns.
5. Add layer and visibility filtering.
6. Add sample regression tests: W2D input + expected PNG checksum / visual diff.
7. Add PDF output from native-rendered images through the existing managed PDF assembly path.
