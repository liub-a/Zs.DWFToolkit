#!/usr/bin/env bash
set -euo pipefail
if ! command -v dotnet >/dev/null 2>&1; then
  echo "dotnet not found; install .NET 8 SDK first."
  exit 1
fi
dotnet build Zs.DWFToolkit.slnx
if [ $# -gt 0 ]; then
  dotnet run --project samples/Zs.DWFToolkit.CliDemo -- info "$1"
fi
