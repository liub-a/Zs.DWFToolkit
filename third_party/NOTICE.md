# Third-party components

Vendored in-tree and built from source (no network at build time):

| Component | Version | Path | License |
|-----------|---------|------|---------|
| FreeType  | 2.13.3  | `freetype/`        | FreeType License (FTL) / GPLv2 dual |
| libtiff   | 4.6.0   | `libtiff/`         | libtiff license (BSD-style, © Sam Leffler / SGI) |
| zlib      | 1.3.1   | `zlib/`            | zlib license (© Jean-loup Gailly / Mark Adler) |
| DejaVu Sans | —     | `fonts/DejaVuSans.ttf` | Bitstream Vera + Arev (free, embeddable) — see `fonts/NOTICE.md` |

libjpeg (IJG, libjpeg-6b API) is built from the sources the ODA toolkit bundles
under `DWFToolkit-7.7/.../w3dtk/jpeg`, so it is not separately committed.

FreeType is used under the FTL. Required attribution:

> Portions of this software are copyright © The FreeType Project
> (https://www.freetype.org). All rights reserved.

Not committed (imported locally; see scripts/setup-oda-dwftoolkit):

| Component | Path | Note |
|-----------|------|------|
| ODA-modified DWF Toolkit 7.7 | `DWFToolkit-7.7/` (gitignored) | imported from `archives/` tarball; licensing is the user's responsibility |

Remaining system build dependencies (not vendored): **libjpeg** and **zlib**
(required by the toolkit's W3dTk and by JPEG image decoding).
