# native-prebuilt

Checked-in native binaries (`libzs_dwf_toolkit_native.*` + `zs_dwf_worker`) per
runtime identifier. `dotnet pack` packs these into `runtimes/<rid>/native/`, so
**building the package needs no native compile** — the native code changes rarely.

```
native-prebuilt/
  linux-x64/    libzs_dwf_toolkit_native.so   zs_dwf_worker
  linux-arm64/  libzs_dwf_toolkit_native.so   zs_dwf_worker
  osx-arm64/    libzs_dwf_toolkit_native.dylib zs_dwf_worker
  osx-x64/      (add when built)
  win-x64/      (add when built)
```

## When to rebuild (the "switch")

Only when the **native** code (`src/Zs.DWFToolkit.Native` or the ODA/W2D path)
changes. On each target platform:

```bash
scripts/build-native-release.sh        # builds + writes native-prebuilt/<rid>/
git add native-prebuilt/<rid> && git commit -m "rebuild native <rid>"
```

Linux binaries come from CI (`native-oda` matrix, x64 + arm64): download the
`native-oda-<rid>` artifacts and drop them here.

Managed-only changes do **not** require a rebuild — just `dotnet pack`.
