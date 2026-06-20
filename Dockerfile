# Multi-stage build: compiles the native ODA renderer + worker and the managed
# CLI, then assembles a small runtime image that can convert DWF/DWFx files.
#
#   docker build -t zs-dwftoolkit .
#   docker run --rm -v "$PWD:/data" zs-dwftoolkit info /data/drawing.dwf
#   docker run --rm -v "$PWD:/data" zs-dwftoolkit to-images /data/drawing.dwf --out-dir /data/out --native

# ---- build stage -----------------------------------------------------------
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends cmake g++ unzip \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Expand the ODA-modified DWF Toolkit from the tracked archive (gitignored once expanded).
RUN ./scripts/setup-oda-dwftoolkit.sh

# Native renderer + out-of-process worker. All third-party libs are vendored, so no
# extra system packages are needed. Bounded parallelism: each WhipTk TU needs ~GBs.
RUN cmake -S src/Zs.DWFToolkit.Native -B build/native \
        -DZS_DWF_ENABLE_ODA_DWFTK=ON -DZS_DWF_WITH_FREETYPE=ON -DZS_DWF_WITH_TIFF=ON \
        -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build/native -j 2 --target zs_dwf_toolkit_native zs_dwf_worker

# Managed CLI.
RUN dotnet publish samples/Zs.DWFToolkit.CliDemo -c Release -o /app

# Place the native library + worker next to the managed app for P/Invoke and the
# out-of-process renderer.
RUN cp build/native/libzs_dwf_toolkit_native.so /app/ \
 && cp build/native/zs_dwf_worker /app/

# ---- runtime stage ---------------------------------------------------------
FROM mcr.microsoft.com/dotnet/runtime:8.0 AS runtime
# mutool (MuPDF) enables DWFx/XPS image/PDF conversion; optional for plain DWF.
RUN apt-get update \
 && apt-get install -y --no-install-recommends mupdf-tools \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/ ./
ENV LD_LIBRARY_PATH=/app \
    ZS_DWF_WORKER=/app/zs_dwf_worker
ENTRYPOINT ["dotnet", "Zs.DWFToolkit.CliDemo.dll"]
CMD ["--help"]
