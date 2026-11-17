#!/bin/bash
# Pull the theme files out of a Windows ISO and dump the msstyles tables.
# Usage: extract_theme.sh <iso> <edition regex> <out dir>
#   extract_theme.sh Win11_25H2_German_x64.iso 'ProfessionalWorkstation$' work/win11
#   extract_theme.sh 8250.iso . work/8250          (first image when there is one)
# Files stay in <out dir> (gitignored): nothing from Microsoft gets committed.
set -euo pipefail
ISO=$(realpath "$1"); RE=$2; OUT=$(realpath -m "$3")
HERE=$(dirname "$(realpath "$0")")
mkdir -p "$OUT/files"
# install.wim, or install.esd on some builds
W=$(7z l -slt "$ISO" | awk -F' = ' '/^Path = sources\/install\.(wim|esd)$/ && !w {w = $2} END {print w}')
[ -n "$W" ] || { echo "no install.wim/esd in $ISO"; exit 1; }
[ -f "$OUT/${W##*/}" ] || 7z e -y -o"$OUT" "$ISO" "$W" >/dev/null
WIM=$OUT/${W##*/}
# Pick the image whose Edition ID matches
IDX=$(wimlib-imagex info "$WIM" | awk -v re="$RE" '/^Index:/ {i = $2} /^Edition ID:/ && $3 ~ re && !f {f = i} END {print f}')
[ -n "$IDX" ] || IDX=1
wimlib-imagex info "$WIM" "$IDX" | grep -E "^(Name|Edition ID|Build|Service Pack Build):"
# Theme files, uxtheme, the DWM and shell pieces a later transplant would look at
wimlib-imagex extract "$WIM" "$IDX" /Windows/Resources/Themes \
	/Windows/System32/uxtheme.dll /Windows/System32/themeui.dll /Windows/System32/themeservice.dll \
	/Windows/System32/dwm.exe /Windows/System32/uDWM.dll /Windows/System32/dwmcore.dll \
	/Windows/explorer.exe /Windows/System32/ExplorerFrame.dll /Windows/System32/shell32.dll \
	--dest-dir="$OUT/files" --no-acls --nullglob 2>&1 | tail -1
for m in "$OUT"/files/Themes/aero/*.msstyles; do
	PYTHONPATH="$HERE/pydeps" python3 "$HERE/msstyles_dump.py" "$m" > "$OUT/$(basename "$m" .msstyles).tsv"
	echo "$(basename "$m"): $(wc -l < "$OUT/$(basename "$m" .msstyles).tsv") records, $(cut -f1 "$OUT/$(basename "$m" .msstyles).tsv" | sort -u | wc -l) classes"
done
