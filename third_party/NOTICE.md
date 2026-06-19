# Third-party components

Vendored in-tree and built from source (no network at build time):

| Component | Version | Path | License |
|-----------|---------|------|---------|
| FreeType  | 2.13.3  | `freetype/`        | FreeType License (FTL) / GPLv2 dual |
| libtiff   | 4.6.0   | `libtiff/`         | libtiff license (BSD-style, © Sam Leffler / SGI) |
| zlib      | 1.3.1   | `zlib/`            | zlib license (© Jean-loup Gailly / Mark Adler) |
| DejaVu Sans | —     | `fonts/DejaVuSans.ttf` | Bitstream Vera + Arev (free, embeddable) — see `fonts/NOTICE.md` |

libjpeg: by default the build uses the system libjpeg/jpeg-turbo when available
(faster, SIMD). When none is found — or with `-DZS_DWF_PREFER_SYSTEM_JPEG=OFF` — it
falls back to building the libjpeg-6b sources the ODA toolkit bundles under
`DWFToolkit-7.7/.../w3dtk/jpeg`, so the build still needs no system jpeg.

FreeType is used under the FTL. Required attribution:

> Portions of this software are copyright © The FreeType Project
> (https://www.freetype.org). All rights reserved.

Not committed (imported locally; see scripts/setup-oda-dwftoolkit):

| Component | Path | Note |
|-----------|------|------|
| ODA-modified DWF Toolkit 7.7 | `DWFToolkit-7.7/` (gitignored) | imported from `archives/` tarball; licensing is the user's responsibility |

Remaining system build dependencies (not vendored): **libjpeg** and **zlib**
(required by the toolkit's W3dTk and by JPEG image decoding).
