#!/bin/bash
# Build a DWMWindow variant here, try loading it in the VM.  Usage: trystyle.sh <name> [parts]
set -e
cd "$(dirname "$0")/.."
N=$1; shift
rm -rf work/try/$N
PYTHONPATH=tools/pydeps:tools python3 tools/build_dwmwindow.py work/8250/files/Themes/aero/aero.msstyles work/win11/files/Themes/aero/aero.msstyles work/try/$N "$@" >/dev/null
vmsetup/vm.sh "New-Item -ItemType Directory -Force C:/w8/try/$N | Out-Null"
vmsetup/vm.sh put work/try/$N/* C:/w8/try/$N/
echo "$(vmsetup/vm.sh "powershell -ExecutionPolicy Bypass -File C:/w8/try_style.ps1 -Name $N") parts=${1:-all}"
