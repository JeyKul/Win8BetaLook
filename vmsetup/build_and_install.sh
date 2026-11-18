#!/bin/bash
# Build Win8Beta.msstyles from the extracted 8250/26200 files and install + apply it in the VM.
set -e
cd "$(dirname "$0")/.."
CLASSES=(DWMWindow 'TaskBarComposited::TaskBar=TaskBar' '^TaskBand2' '^TaskBar::' '^TaskBarVert::' '^TrayNotify' '^TaskBand$')
rm -rf work/try/Win8Beta
PYTHONPATH=tools/pydeps:tools python3 tools/build_style.py work/8250/files/Themes/aero/aero.msstyles \
	work/win11/files/Themes/aero/aero.msstyles work/try/Win8Beta "${CLASSES[@]}" | tail -1
vmsetup/vm.sh "New-Item -ItemType Directory -Force C:/w8/try/Win8Beta | Out-Null"
vmsetup/vm.sh put work/try/Win8Beta/* C:/w8/try/Win8Beta/
vmsetup/vm.sh "powershell -ExecutionPolicy Bypass -File C:/w8/try_style.ps1 -Name Win8Beta"
