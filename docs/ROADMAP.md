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

## V0.3 Native 渲染第一版（部分完成）

- 接 DWF Toolkit（ODA 适配器，`ZS_DWF_ENABLE_ODA_DWFTK`）— done；
- 解析 ePlot sections（`OdaDwfAdapter::get_dwf_info_json`，枚举 sections→pages）— done；
- 实现线、折线、矩形、多边形 — done；
- 输出 PNG（MinimalPng）— done；
- 返回坐标变换（render JSON `transform` → `DwfPageTransform`）— done。

## V0.4 渲染增强（进行中）

- 图片资源 — 部分完成（原始 RGB/RGBA blit；mapped/Group4/JPEG 仍为灰底占位）；
- 文字 — 占位框（未实现真字形）；
- 字体 fallback — 待办；
- 填充 / 线型 / 裁剪 / 图层 — 待办；
- 大图瓦片 — 待办。

> Native ODA 路径在 Linux/CI 编译验证；macOS 默认构建把 native 编译为返回
> `unsupported_dwf_rendering` 的桩。所有 `WT_Image` / section API 已对照
> `third_party/DWFToolkit-7.7` 真实头文件核对。

## V1.0 产品化

- Worker 队列；
- 超时隔离；
- 容器部署；
- 样本回归测试；
- DWF + PDF/OFD 签署链路。
