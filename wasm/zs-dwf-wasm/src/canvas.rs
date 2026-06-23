//! Minimal RGBA raster canvas — a Rust port of the C++ RasterCanvas transform and
//! line drawing, enough to prove the route-B (Rust/WASM) rendering path.

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Rgba(pub u8, pub u8, pub u8, pub u8);

#[derive(Clone, Copy, Default)]
pub struct Box2 {
    pub min_x: f64,
    pub min_y: f64,
    pub max_x: f64,
    pub max_y: f64,
    valid: bool,
}

impl Box2 {
    pub fn include(&mut self, x: f64, y: f64) {
        if !self.valid {
            self.min_x = x;
            self.max_x = x;
            self.min_y = y;
            self.max_y = y;
            self.valid = true;
        } else {
            self.min_x = self.min_x.min(x);
            self.min_y = self.min_y.min(y);
            self.max_x = self.max_x.max(x);
            self.max_y = self.max_y.max(y);
        }
    }
    pub fn is_valid(&self) -> bool {
        self.valid
    }
}

pub struct Canvas {
    width: i32,
    height: i32,
    scale: f64,
    off_x: f64,
    off_y: f64,
    bounds: Box2,
    pub pixels: Vec<Rgba>,
}

impl Canvas {
    pub fn new(width: i32, height: i32, bounds: Box2) -> Self {
        let logical_w = (bounds.max_x - bounds.min_x).max(1.0);
        let logical_h = (bounds.max_y - bounds.min_y).max(1.0);
        let scale = (width as f64 / logical_w).min(height as f64 / logical_h);
        let off_x = (width as f64 - logical_w * scale) * 0.5;
        let off_y = (height as f64 - logical_h * scale) * 0.5;
        Canvas {
            width,
            height,
            scale,
            off_x,
            off_y,
            bounds,
            pixels: vec![Rgba(255, 255, 255, 255); (width * height) as usize],
        }
    }

    fn to_pixel(&self, x: f64, y: f64) -> (f64, f64) {
        (
            self.off_x + (x - self.bounds.min_x) * self.scale,
            self.off_y + (self.bounds.max_y - y) * self.scale,
        )
    }

    fn set(&mut self, x: i32, y: i32, c: Rgba) {
        if x < 0 || y < 0 || x >= self.width || y >= self.height {
            return;
        }
        self.pixels[(y * self.width + x) as usize] = c;
    }

    pub fn draw_polyline(&mut self, pts: &[(f64, f64)], color: Rgba) {
        for w in pts.windows(2) {
            let (ax, ay) = self.to_pixel(w[0].0, w[0].1);
            let (bx, by) = self.to_pixel(w[1].0, w[1].1);
            self.line(ax, ay, bx, by, color);
        }
    }

    fn line(&mut self, x0: f64, y0: f64, x1: f64, y1: f64, c: Rgba) {
        // Integer Bresenham.
        let (mut x0, mut y0) = (x0.round() as i32, y0.round() as i32);
        let (x1, y1) = (x1.round() as i32, y1.round() as i32);
        let dx = (x1 - x0).abs();
        let dy = -(y1 - y0).abs();
        let sx = if x0 < x1 { 1 } else { -1 };
        let sy = if y0 < y1 { 1 } else { -1 };
        let mut err = dx + dy;
        loop {
            self.set(x0, y0, c);
            if x0 == x1 && y0 == y1 {
                break;
            }
            let e2 = 2 * err;
            if e2 >= dy {
                err += dy;
                x0 += sx;
            }
            if e2 <= dx {
                err += dx;
                y0 += sy;
            }
        }
    }

    /// Returns the canvas as a flat RGBA byte buffer (row-major, top-left origin).
    pub fn to_rgba_bytes(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(self.pixels.len() * 4);
        for p in &self.pixels {
            out.extend_from_slice(&[p.0, p.1, p.2, p.3]);
        }
        out
    }
}
