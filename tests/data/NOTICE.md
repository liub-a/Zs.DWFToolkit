# Test data

Sample drawings used by the native regression tests. Both are public sample files
distributed with the Open Design Alliance / Autodesk DWF Toolkit and mirrored in
public GitHub repositories (e.g. AvalonHolographics/DWFToolkit,
kveretennicov/dwf-toolkit). They are included here only as read-only test
fixtures.

| File | Kind | Use |
|------|------|-----|
| `ocean_90.w2d` | bare 2D W2D stream (`(W2D V06.00)`) | `ocean_regression_tests` — primitive counts + non-white pixel band |
| `IRD-Addition.dwf` | 2D DWF v6 package, 11 ePlot pages | `dwf_package_tests` — full package→W2D→render path |

If redistribution of these specific samples is ever a concern, replace them with
your own DWF fixtures and update the test paths.
