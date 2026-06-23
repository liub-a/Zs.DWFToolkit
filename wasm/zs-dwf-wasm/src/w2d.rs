//! Parser for W2D — both the **ASCII** form and the common **binary** opcodes.
//! Geometry is rendered as polylines (point sets). Binary point sets use the WHIP
//! relative-coordinate scheme: each vertex is a delta added to a running current
//! point. Color comes from the `(Color r,g,b,a)` ASCII extended opcode.
//!
//! Handled: header, whitespace, `(…)` ASCII ext (Color + bounds, others skipped),
//! `{…}` binary ext (skipped via its 4-byte offset), and the point-set opcodes
//! `P` (ASCII), `p` (32-bit binary), `0x10` (16-bit binary), plus markers
//! `M`/`m`/`0x8D`. Unknown single-byte opcodes are skipped (best-effort; a few
//! operand-bearing opcodes we don't model can still desync a stream).
//! Compressed W2D is not handled.

use crate::canvas::{Box2, Rgba};

#[derive(Default)]
pub struct Drawing {
    pub polylines: Vec<(Rgba, Vec<(f64, f64)>)>,
    pub bounds: Box2,
}

struct P<'a> {
    b: &'a [u8],
    i: usize,
    cur_x: i64,
    cur_y: i64,
    color: Rgba,
}

impl<'a> P<'a> {
    fn eof(&self) -> bool {
        self.i >= self.b.len()
    }
    fn u8(&mut self) -> u8 {
        let v = self.b[self.i];
        self.i += 1;
        v
    }
    fn i16(&mut self) -> i64 {
        let v = i16::from_le_bytes([self.b[self.i], self.b[self.i + 1]]);
        self.i += 2;
        v as i64
    }
    fn i32(&mut self) -> i64 {
        let v = i32::from_le_bytes([
            self.b[self.i],
            self.b[self.i + 1],
            self.b[self.i + 2],
            self.b[self.i + 3],
        ]);
        self.i += 4;
        v as i64
    }
    fn u32(&mut self) -> u32 {
        let v = u32::from_le_bytes([
            self.b[self.i],
            self.b[self.i + 1],
            self.b[self.i + 2],
            self.b[self.i + 3],
        ]);
        self.i += 4;
        v
    }
    fn skip_ws(&mut self) {
        while !self.eof() && self.b[self.i].is_ascii_whitespace() {
            self.i += 1;
        }
    }
    /// WHIP count: 1 byte; 0 -> followed by a u16, count = 256 + u16.
    fn read_count(&mut self) -> usize {
        if self.i >= self.b.len() {
            return 0;
        }
        let c = self.u8();
        if c != 0 {
            c as usize
        } else if self.i + 2 <= self.b.len() {
            let s = u16::from_le_bytes([self.b[self.i], self.b[self.i + 1]]);
            self.i += 2;
            256 + s as usize
        } else {
            0
        }
    }
    /// Reads a balanced ASCII group starting at `(`; returns its inner bytes.
    fn paren_group(&mut self) -> &'a [u8] {
        let start = self.i;
        let mut depth = 0;
        while !self.eof() {
            match self.b[self.i] {
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
    /// De-relativizes `count` delta points (each added to the running current point).
    fn relative_points(&mut self, count: usize, bits32: bool, d: &mut Drawing) -> Vec<(f64, f64)> {
        let mut pts = Vec::with_capacity(count);
        for _ in 0..count {
            if (bits32 && self.i + 8 > self.b.len()) || (!bits32 && self.i + 4 > self.b.len()) {
                break;
            }
            let (dx, dy) = if bits32 {
                (self.i32(), self.i32())
            } else {
                (self.i16(), self.i16())
            };
            self.cur_x += dx;
            self.cur_y += dy;
            pts.push((self.cur_x as f64, self.cur_y as f64));
            d.bounds.include(self.cur_x as f64, self.cur_y as f64);
        }
        pts
    }
}

fn parse_color(inner: &[u8]) -> Option<Rgba> {
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

fn parse_point_ascii(tok: &[u8]) -> Option<(f64, f64)> {
    let s = std::str::from_utf8(tok).ok()?;
    let (a, b) = s.split_once(',')?;
    Some((a.trim().parse().ok()?, b.trim().parse().ok()?))
}

pub fn parse(bytes: &[u8]) -> Drawing {
    let mut d = Drawing::default();
    let mut p = P {
        b: bytes,
        i: 0,
        cur_x: 0,
        cur_y: 0,
        color: Rgba(0, 0, 0, 255),
    };

    // Skip the "(W2D V06.00)" header group.
    p.skip_ws();
    if !p.eof() && p.b[p.i] == b'(' {
        p.paren_group();
    }

    while !p.eof() {
        // NB: do not skip whitespace blindly here — 0x09/0x0A/0x0D/0x20 can also be
        // opcodes; but in practice inter-opcode whitespace is common in W2D. We only
        // eat it when the next non-ws byte is a structural one.
        let c = p.b[p.i];
        match c {
            b'(' => {
                let inner = p.paren_group();
                if inner.starts_with(b"Color") {
                    if let Some(rgba) = parse_color(inner) {
                        p.color = rgba;
                    }
                } else if inner.starts_with(b"Viewport") || inner.starts_with(b"Contour") {
                    for tok in inner.split(|b| b.is_ascii_whitespace() || *b == b'(' || *b == b')') {
                        if let Some((x, y)) = parse_point_ascii(tok) {
                            d.bounds.include(x, y);
                        }
                    }
                }
            }
            b'{' => {
                // binary extended opcode: 4-byte LE offset (to matching '}') + 2-byte id.
                let brace = p.i;
                p.i += 1;
                if p.i + 4 <= p.b.len() {
                    let off = p.u32() as usize;
                    let target = brace + off;
                    p.i = target.min(p.b.len());
                    if p.i < p.b.len() && p.b[p.i] == b'}' {
                        p.i += 1;
                    }
                } else {
                    p.i = p.b.len();
                }
            }
            b')' => p.i += 1,
            b' ' | b'\t' | b'\r' | b'\n' => p.i += 1,
            b'P' => {
                // ASCII polyline: 'P' count then count "x,y" absolute points.
                p.i += 1;
                p.skip_ws();
                let n: usize = {
                    let s = p.i;
                    while !p.eof() && !p.b[p.i].is_ascii_whitespace() {
                        p.i += 1;
                    }
                    std::str::from_utf8(&p.b[s..p.i]).ok().and_then(|x| x.parse().ok()).unwrap_or(0)
                };
                let mut pts = Vec::with_capacity(n);
                for _ in 0..n {
                    p.skip_ws();
                    let s = p.i;
                    while !p.eof() && !p.b[p.i].is_ascii_whitespace() && p.b[p.i] != b'(' {
                        p.i += 1;
                    }
                    if let Some(pt) = parse_point_ascii(&p.b[s..p.i]) {
                        d.bounds.include(pt.0, pt.1);
                        pts.push(pt);
                    }
                }
                if pts.len() >= 2 {
                    let col = p.color;
                    d.polylines.push((col, pts));
                }
            }
            b'p' => {
                p.i += 1;
                let n = p.read_count();
                let pts = p.relative_points(n, true, &mut d);
                if pts.len() >= 2 {
                    let col = p.color;
                    d.polylines.push((col, pts));
                }
            }
            0x10 => {
                p.i += 1;
                let n = p.read_count();
                let pts = p.relative_points(n, false, &mut d);
                if pts.len() >= 2 {
                    let col = p.color;
                    d.polylines.push((col, pts));
                }
            }
            0x8D => {
                // 16-bit polymarker: consume points (drawn as a degenerate run).
                p.i += 1;
                let n = p.read_count();
                let _ = p.relative_points(n, false, &mut d);
            }
            _ => p.i += 1, // unknown single-byte opcode: best-effort skip
        }
    }

    d
}
