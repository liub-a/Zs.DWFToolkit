# DWFToolkit-7.7

这里用于放置 ODA 修改版 Autodesk DWF Toolkit 7.7 源码。

本次我已经在本地生成了源码展开版压缩包：`Zs.DWFToolkit-source-included.zip`，其中该目录已包含 1688 个源文件。

由于当前 GitHub 连接器不支持把本地 45MB / 1600+ 文件目录批量展开上传到公开仓库，我已完成以下仓库侧调整：

- 删除已提交的 `artifacts/native/libzs_dwf_toolkit_native.so`；
- 修改 `.gitignore`，允许提交 `third_party/DWFToolkit-7.7/`；
- 保留 Native CMake 的 `ZS_DWF_ENABLE_ODA_DWFTK` 开关。

你本地拿到 `Zs.DWFToolkit-source-included.zip` 后，可以直接执行：

```bash
unzip Zs.DWFToolkit-source-included.zip
cd Zs.DWFToolkit
git add third_party/DWFToolkit-7.7 third_party/README.md
git commit -m "chore: vendor ODA DWF Toolkit source"
git push origin main
```

或者继续用导入脚本：

```bash
./scripts/setup-oda-dwftoolkit.sh /path/to/DWFToolkit-7.7-src-ODA.zip
git add third_party/DWFToolkit-7.7
git commit -m "chore: vendor ODA DWF Toolkit source"
```

> 注意：仓库是 public，提交前建议确认 Autodesk/ODA DWF Toolkit 的再分发许可边界。
