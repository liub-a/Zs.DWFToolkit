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

## V0.4 渲染增强（基本完成）

- 图片资源 — done（RGB/RGBA/mapped/indexed/JPEG/bitonal/Group3X/Group4X 全解码，仅 PNG_Group4 opcode 占位）；
- 文字 — done（FreeType 真字形，字体内嵌）；
- 填充 — done（实心 + hatch + 轮廓集 even-odd 带洞）；
- 线型 — done（实线 + 虚线/点线）；
- 图层可见性 — done；
- 三角带 / 标记 / 宏放置 — done（宏几何完整 replay 除外）；
- 抗锯齿 — done（2× 超采样）；
- 裁剪 — 部分（viewport bbox；非矩形裁剪待办）。

> **端到端验证**：完整管线在真实 2D DWF 包（11 页 ePlot 工程图）上验证 ——
> 托管读 DWF 6.x 包 → `OdaDwfAdapter` 逐页抽 W2D → `W2dWhipRenderer` 渲染 →
> 11 张 1200×900 PNG，标题栏/表格/真字形/抗锯齿正确。
> 自包含构建：FreeType/libtiff/zlib 源码入库 + libjpeg(系统优先/6b 回退)，
> 产物仅依赖 libSystem+libc++。

## V1.0 产品化

- Worker 队列；
- 超时隔离；
- 容器部署；
- 样本回归测试；
- DWF + PDF/OFD 签署链路。
