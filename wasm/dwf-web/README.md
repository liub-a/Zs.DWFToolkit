# In-browser DWF renderer (WebAssembly)

A pure front-end, offline DWF/W2D viewer: the native ODA/W2D renderer
(DwfCore · WhipTk · W3dTk · DwfToolkit + FreeType + this repo's renderer/vector-PDF)
compiled to WebAssembly. Files never leave the browser — no server, no upload.

## Build

```bash
# Needs Emscripten (emsdk or a working emcc/emcmake on PATH).
./scripts/build-wasm.sh
# -> wasm/dwf-web/zs_dwf.js + zs_dwf.wasm  (~12 MB module)
```

The build:
1. imports the ODA toolkit source (`setup-oda-dwftoolkit.sh`, which applies the
   wasm platform patch to `dwfcore/Core.h`);
2. configures with `emcmake` (the `EMSCRIPTEN` branch in the native `CMakeLists.txt`
   turns on `-DEMCC`, `-Wno-register`, FreeType cross-compile seeds, no worker);
3. builds the static archives and links them into one JS+wasm module exporting the
   C ABI (`zs_dwf_render_dwf_pdf`, `zs_dwf_render_page`, `zs_w2d_render_file`,
   `zs_dwf_get_info_json`, `zs_dwf_get_last_error`).

### Toolchain notes
- Emscripten needs Python ≥ 3.10. If your default `python3` is older, set
  `EMSDK_PYTHON` to a newer one.
- TIFF is disabled in the wasm build (its Group4 fax codec needs a `size_t`
  cross-compile nudge); everything else (lines, fills, text-as-outlines, images,
  vector PDF) works.

## Run

Serve over HTTP (the `.wasm` is fetched, so `file://` won't work):

```bash
cd wasm/dwf-web
python3 -m http.server 8080
# open http://localhost:8080
```

Drop a `.dwf`/`.w2d`, click **Read info** or **Render vector PDF**.

## Files
- `index.html`, `main.js` — the UI (committed).
- `zs_dwf.js`, `zs_dwf.wasm` — the built module (generated; git-ignored).
