#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

dotnet build "$ROOT_DIR/src/Zs.DWFToolkit/Zs.DWFToolkit.csproj"
dotnet build "$ROOT_DIR/samples/Zs.DWFToolkit.CliDemo/Zs.DWFToolkit.CliDemo.csproj"
