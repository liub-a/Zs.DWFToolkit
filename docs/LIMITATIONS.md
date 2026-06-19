# 限制说明

## 1. 普通 DWF 转图限制

普通 DWF 的 2D 图形通常需要解释 W2D/WHIP 图形流。DWF Toolkit 可以帮助读包和资源，但完整渲染还需要：

- 坐标变换；
- 线型；
- 线宽；
- 填充；
- 字体；
- 图片资源；
- 裁剪；
- 图层；
- 打印样式。

所以本项目只提供 Native Bridge，不声明已经完成普通 DWF 完整渲染。

## 2. DWFx 兼容性

DWFx 基于 XPS/OPC，但不代表所有 DWFx 都能被通用 XPS 工具完美转换。复杂 CAD 线型、字体、图层可能存在差异。

## 3. 外部工具授权

本项目不打包 `mutool`、GhostXPS、DWF Toolkit 或 ODA SDK。你需要自行确认授权。

## 4. 产品建议

签署系统第一版建议：

```text
DWF 原文件：留档
PDF/OFD：签署和归档
DWFx：可尝试自动转预览
普通 DWF：优先要求上传配套 PDF
```
