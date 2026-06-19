# Roadmap

## V0.1 当前骨架

- C# Facade；
- Info Reader；
- Thumbnail Extractor；
- DWFx external converter；
- Native ABI Stub；
- CLI demo。

## V0.2 建议下一步

- 用真实 DWF 样本测试包结构；
- 完善 manifest/section/resource 解析；
- 增加页面尺寸识别；
- 改善缩略图选择策略；
- 增加图片合成 PDF 的托管实现。

## V0.3 Native 渲染第一版

- 接 DWF Toolkit；
- 解析 ePlot sections；
- 实现线、折线、矩形、多边形；
- 输出 PNG；
- 返回坐标变换。

## V0.4 渲染增强

- 文字；
- 字体 fallback；
- 图片资源；
- 填充；
- 裁剪；
- 线型；
- 大图瓦片。

## V1.0 产品化

- Worker 队列；
- 超时隔离；
- 容器部署；
- 样本回归测试；
- DWF + PDF/OFD 签署链路。
