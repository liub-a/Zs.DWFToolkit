# Bundled font

`DejaVuSans.ttf` is part of the **DejaVu Fonts** project, a public-domain-friendly
derivative of the Bitstream Vera fonts. Both licenses permit embedding and
redistribution (including in commercial software) provided the fonts are not sold
by themselves and the reserved font names are respected.

- DejaVu Fonts: https://dejavu-fonts.github.io/ (license: Bitstream Vera + Arev, free)

`DroidSansFallback.ttf` is **Droid Sans Fallback** (Android Open Source Project),
used as the CJK / wide-coverage fallback so Chinese/Japanese/Korean text in DWF
drawings renders instead of showing tofu boxes. License: Apache License 2.0.

Both fonts are compiled into `zs_dwf_toolkit_native` (when built with
`ZS_DWF_WITH_FREETYPE=ON`) so text rendering needs no system font. The renderer
picks the primary (DejaVu) or fallback (Droid) face per code point.

# FreeType

When built with `ZS_DWF_WITH_FREETYPE=ON`, this project statically links
**FreeType** (fetched at configure time, https://freetype.org). FreeType is used
under the FreeType License (FTL). Required attribution:

> Portions of this software are copyright © The FreeType Project
> (https://www.freetype.org). All rights reserved.
