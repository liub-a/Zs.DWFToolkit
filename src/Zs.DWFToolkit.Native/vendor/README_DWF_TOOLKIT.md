# Vendor DWF Toolkit Placement

Do not commit Autodesk/ODA DWF Toolkit source or binaries into this repository unless your project license explicitly permits it.

Suggested layout after obtaining the toolkit legally:

```text
src/Zs.DWFToolkit.Native/vendor/DWFToolkit-7.7/
├── develop/
├── samples/
├── lib/
└── include/
```

Then update `src/Zs.DWFToolkit.Native/CMakeLists.txt` to include and link:

```cmake
target_include_directories(zs_dwf_toolkit_native PRIVATE vendor/DWFToolkit-7.7/...)
target_link_libraries(zs_dwf_toolkit_native PRIVATE DwfToolkit DwfCore)
```

Implementation targets:

1. `zs_dwf_get_info_json`: open DWF package, enumerate ePlot/eModel sections, page resources, thumbnails.
2. `zs_dwf_render_page`: locate the page graphics resource, interpret W2D/WHIP stream, draw into Skia/Cairo, encode image.

The current native project is only an ABI stub so the C# integration can be completed early.
