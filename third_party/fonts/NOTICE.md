# Bundled font

`DejaVuSans.ttf` is part of the **DejaVu Fonts** project, a public-domain-friendly
derivative of the Bitstream Vera fonts. Both licenses permit embedding and
redistribution (including in commercial software) provided the fonts are not sold
by themselves and the reserved font names are respected.

- DejaVu Fonts: https://dejavu-fonts.github.io/ (license: Bitstream Vera + Arev, free)

This font is compiled into `zs_dwf_toolkit_native` (when built with
`ZS_DWF_WITH_FREETYPE=ON`) so text rendering needs no system font.

# FreeType

When built with `ZS_DWF_WITH_FREETYPE=ON`, this project statically links
**FreeType** (fetched at configure time, https://freetype.org). FreeType is used
under the FreeType License (FTL). Required attribution:

> Portions of this software are copyright © The FreeType Project
> (https://www.freetype.org). All rights reserved.
