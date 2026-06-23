# zs-dwf-wasm (route B spike)

A **pure-Rust** W2D renderer compiled to **WebAssembly** — no ODA/C++ dependency.
This is a proof-of-concept ("route B") for rendering DWF/W2D in the browser:
parse the W2D stream in Rust and rasterize to an RGBA buffer a `<canvas>` can show.

## What works (spike scope)

- Parses the **ASCII** form of W2D (e.g. ODA's `ocean_90.w2d`): `(Color …)`,
  `(Viewport (Contour …))` for bounds, and `P <n> x,y …` polylines.
- Ports the C++ `RasterCanvas` transform + line drawing to Rust.
- Builds to a **~41 KB** wasm module (vs. the multi-MB C++/ODA stack).

## Binary W2D (partial)

Production `.w2d` are binary. The parser now handles binary framing (`{…}`
extended opcodes via their offset) and the **relative-coordinate point-set
decode** (16-bit `0x10` and 32-bit `p` polylines, with the running current-point
scheme). Proven: the sheet border of a real binary DWF page decodes and renders.

**Limitation**: a full binary stream desyncs once it hits an operand-bearing
single-byte opcode we don't model yet (set-color-binary, line weight, fills,
ellipses, etc.), so only the leading geometry renders. Reaching parity means
handling every operand-bearing opcode (a sizeable, mechanical port from
`whiptk/*.cpp`). Compressed W2D is also not handled.

## Not yet (future, to reach parity)

- Full single-byte opcode coverage (above), polygon/contour fills, ellipses,
  text glyphs, images, clipping, AA.
- DWF **package** reading (zip + sections) — currently bare `.w2d` only.

## Build & test

```bash
cargo test                       # parses + renders ocean_90.w2d (native)
cargo run --example render -- ../../tests/data/ocean_90.w2d /tmp/out.rgba 1000 800

wasm-pack build --release --target web --out-dir pkg   # -> pkg/zs_dwf_wasm.js + .wasm
# then serve the parent dir and open demo/index.html, pick an ASCII .w2d
```

## Browser API

```js
import init, { render_w2d } from "./pkg/zs_dwf_wasm.js";
await init();
const rgba = render_w2d(uint8ArrayOfW2d, width, height);   // RGBA8
ctx.putImageData(new ImageData(new Uint8ClampedArray(rgba), width, height), 0, 0);
```

Reaching full parity means porting the binary WHIP opcode parser to Rust; the
drawing layer (already proven here) is the easy part.
