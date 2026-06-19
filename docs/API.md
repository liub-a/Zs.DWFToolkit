# API 使用说明

## 1. Facade

最简单方式使用 `ZsDwfToolkit`：

```csharp
var toolkit = new ZsDwfToolkit();
```

## 2. ReadInfoAsync

```csharp
var info = await toolkit.ReadInfoAsync("a.dwfx");
```

返回：

```csharp
DwfDocumentInfo
```

包含：

- `Kind`：Dwf/Dwfx/Xps；
- `IsZipBased`；
- `Pages`；
- `Entries`；
- `Properties`。

## 3. ExtractBestThumbnailAsync

```csharp
var thumb = await toolkit.ExtractBestThumbnailAsync("a.dwf", "thumb.jpg");
```

## 4. ConvertToImagesAsync

```csharp
var result = await toolkit.ConvertToImagesAsync("a.dwfx", "./images", new DwfRenderOptions
{
    Dpi = 200,
    ImageFormat = "png",
    Timeout = TimeSpan.FromSeconds(60)
});
```

## 5. ConvertToPdfAsync

```csharp
var result = await toolkit.ConvertToPdfAsync("a.dwfx", "a.pdf", new DwfRenderOptions
{
    MutoolPath = "/usr/local/bin/mutool"
});
```

## 6. 自定义依赖

```csharp
var reader = new DwfDocumentReader();
var extractor = new DwfThumbnailExtractor();
var native = new NativeDwfRenderer();
var converter = new DwfxExternalConverter(reader, extractor, native);
var toolkit = new ZsDwfToolkit(reader, extractor, converter);
```
