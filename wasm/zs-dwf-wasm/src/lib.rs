//! Rust/WASM W2D renderer spike (route B): parse ASCII W2D + rasterize to RGBA,
//! with no ODA/C++ dependency. Exposes a wasm-bindgen entry point a browser can
//! call to draw a `.w2d` onto a <canvas> via ImageData.

mod canvas;
mod w2d;

use canvas::{Box2, Canvas};

/// Renders an ASCII W2D byte buffer to a width*height RGBA8 buffer.
pub fn render_w2d_to_rgba(bytes: &[u8], width: i32, height: i32) -> Vec<u8> {
    if width <= 0 || height <= 0 {
        return Vec::new();
    }
    let drawing = w2d::parse(bytes);

    let mut bounds = drawing.bounds;
    if !bounds.is_valid() {
        bounds = Box2::default();
        bounds.include(0.0, 0.0);
        bounds.include(width as f64, height as f64);
    }

    let mut c = Canvas::new(width, height, bounds);
    for (color, pts) in &drawing.polylines {
        c.draw_polyline(pts, *color);
    }
    c.to_rgba_bytes()
}

/// Number of polylines parsed (diagnostics / tests).
pub fn count_polylines(bytes: &[u8]) -> usize {
    w2d::parse(bytes).polylines.len()
}

#[cfg(target_arch = "wasm32")]
mod wasm {
    use wasm_bindgen::prelude::*;

    /// Browser entry point: returns an RGBA8 buffer suitable for
    /// `new ImageData(new Uint8ClampedArray(buf), width, height)`.
    #[wasm_bindgen]
    pub fn render_w2d(bytes: &[u8], width: i32, height: i32) -> Vec<u8> {
        super::render_w2d_to_rgba(bytes, width, height)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const OCEAN: &[u8] = include_bytes!("../../../tests/data/ocean_90.w2d");
    const BINARY: &[u8] = include_bytes!("../../../tests/data/binary_sample.w2d");

    #[test]
    fn renders_binary_w2d_non_blank() {
        let rgba = render_w2d_to_rgba(BINARY, 600, 400);
        assert_eq!(rgba.len(), 600 * 400 * 4);
        let non_white = rgba
            .chunks_exact(4)
            .filter(|p| !(p[0] > 245 && p[1] > 245 && p[2] > 245))
            .count();
        assert!(non_white > 500, "binary w2d should draw, got {non_white}");
    }

    #[test]
    fn parses_many_polylines_from_ocean() {
        let n = count_polylines(OCEAN);
        assert!(n > 1000, "expected thousands of polylines, got {n}");
    }

    #[test]
    fn renders_non_blank() {
        let rgba = render_w2d_to_rgba(OCEAN, 400, 300);
        assert_eq!(rgba.len(), 400 * 300 * 4);
        let non_white = rgba
            .chunks_exact(4)
            .filter(|p| !(p[0] > 245 && p[1] > 245 && p[2] > 245))
            .count();
        assert!(non_white > 500, "expected drawn pixels, got {non_white}");
    }
}
