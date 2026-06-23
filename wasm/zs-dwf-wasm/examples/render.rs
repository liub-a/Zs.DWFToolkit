//! Renders a .w2d to a raw RGBA file for visual inspection:
//!   cargo run --example render -- <in.w2d> <out.rgba> <w> <h>

use std::io::Write;

fn main() {
    let a: Vec<String> = std::env::args().collect();
    let inp = &a[1];
    let outp = &a[2];
    let w: i32 = a[3].parse().unwrap();
    let h: i32 = a[4].parse().unwrap();
    let bytes = std::fs::read(inp).unwrap();
    let rgba = zs_dwf_wasm::render_w2d_to_rgba(&bytes, w, h);
    std::fs::File::create(outp).unwrap().write_all(&rgba).unwrap();
    eprintln!("wrote {} bytes ({w}x{h})", rgba.len());
}
