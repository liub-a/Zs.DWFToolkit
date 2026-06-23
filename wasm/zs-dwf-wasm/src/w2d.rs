//! Tolerant parser for the **ASCII** form of W2D (e.g. ODA's ocean_90.w2d).
//! Handles enough opcodes to prove route B: `(Color r,g,b,a)`, `(Viewport (Contour
//! ...))` for bounds, and `P <n> x,y ...` polylines. Unknown parenthesised groups
//! and single tokens are skipped. Binary/compressed W2D is out of scope for the spike.

use crate::canvas::{Box2, Rgba};

#[derive(Default)]
pub struct Drawing {
    pub polylines: Vec<(Rgba, Vec<(f64, f64)>)>,
    pub bounds: Box2,
}

struct Scanner<'a> {
    b: &'a [u8],
    i: usize,
}

impl<'a> Scanner<'a> {
    fn new(b: &'a [u8]) -> Self {
        Scanner { b, i: 0 }
    }
    fn eof(&self) -> bool {
        self.i >= self.b.len()
    }
    fn peek(&self) -> u8 {
        self.b[self.i]
    }
    fn skip_ws(&mut self) {
        while !self.eof() && self.peek().is_ascii_whitespace() {
            self.i += 1;
        }
    }
    /// Reads a whitespace-delimited token (also stops at '(' / ')').
    fn token(&mut self) -> &'a [u8] {
        self.skip_ws();
        let start = self.i;
        while !self.eof() {
            let c = self.peek();
            if c.is_ascii_whitespace() || c == b'(' || c == b')' {
                break;
            }
            self.i += 1;
        }
        &self.b[start..self.i]
    }
    /// Reads a balanced parenthesised group, returning its inner bytes.
    fn paren_group(&mut self) -> &'a [u8] {
        debug_assert_eq!(self.peek(), b'(');
        let start = self.i;
        let mut depth = 0;
        while !self.eof() {
            match self.peek() {
                b'(' => depth += 1,
                b')' => {
                    depth -= 1;
                    self.i += 1;
                    if depth == 0 {
                        return &self.b[start + 1..self.i - 1];
                    }
                    continue;
                }
                _ => {}
            }
            self.i += 1;
        }
        &self.b[start..self.i]
    }
}

fn parse_point(tok: &[u8]) -> Option<(f64, f64)> {
    let s = std::str::from_utf8(tok).ok()?;
    let (a, b) = s.split_once(',')?;
    Some((a.trim().parse().ok()?, b.trim().parse().ok()?))
}

fn parse_color(inner: &[u8]) -> Option<Rgba> {
    // "Color r,g,b,a"
    let s = std::str::from_utf8(inner).ok()?;
    let rest = s.strip_prefix("Color")?.trim();
    let nums: Vec<u8> = rest
        .split(',')
        .filter_map(|p| p.trim().parse::<i64>().ok())
        .map(|n| n.clamp(0, 255) as u8)
        .collect();
    match nums.as_slice() {
        [r, g, b, a] => Some(Rgba(*r, *g, *b, *a)),
        [r, g, b] => Some(Rgba(*r, *g, *b, 255)),
        _ => None,
    }
}

pub fn parse(bytes: &[u8]) -> Drawing {
    let mut d = Drawing::default();
    let mut color = Rgba(0, 0, 0, 255);
    let mut sc = Scanner::new(bytes);

    // Skip the "(W2D V06.00)" header group if present.
    sc.skip_ws();
    if !sc.eof() && sc.peek() == b'(' {
        sc.paren_group();
    }

    while !sc.eof() {
        sc.skip_ws();
        if sc.eof() {
            break;
        }
        let c = sc.peek();
        if c == b'(' {
            let inner = sc.paren_group();
            if inner.starts_with(b"Color") {
                if let Some(rgba) = parse_color(inner) {
                    color = rgba;
                }
            } else if inner.starts_with(b"Viewport") || inner.starts_with(b"Contour") {
                // Pull every "x,y" token inside for bounds.
                let mut inner_sc = Scanner::new(inner);
                while !inner_sc.eof() {
                    let t = inner_sc.token();
                    if t.is_empty() {
                        if !inner_sc.eof() && (inner_sc.peek() == b'(' || inner_sc.peek() == b')') {
                            inner_sc.i += 1;
                        }
                        continue;
                    }
                    if let Some((x, y)) = parse_point(t) {
                        d.bounds.include(x, y);
                    }
                }
            }
            continue;
        }
        if c == b')' {
            sc.i += 1;
            continue;
        }

        let tok = sc.token();
        if tok == b"P" {
            // P <count> then <count> "x,y" points.
            let n: usize = std::str::from_utf8(sc.token())
                .ok()
                .and_then(|s| s.parse().ok())
                .unwrap_or(0);
            let mut pts = Vec::with_capacity(n);
            for _ in 0..n {
                if let Some(p) = parse_point(sc.token()) {
                    d.bounds.include(p.0, p.1);
                    pts.push(p);
                }
            }
            if pts.len() >= 2 {
                d.polylines.push((color, pts));
            }
        }
        // other single-token opcodes are skipped in this spike
    }

    d
}
