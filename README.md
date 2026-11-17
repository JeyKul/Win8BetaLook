# Win8BetaLook

The look of the Windows 8 betas (Consumer Preview, build 8250; Developer Preview,
build 8102 as a fallback) on Windows 10/11. Target: **Windows 11 25H2 Pro for
Workstations** (build 26200).

Public Microsoft builds only, and **no Microsoft files in this repository**: you
bring your own ISOs, and the tools extract what they need locally into `work/`
(gitignored).

## Plan

**Phase A: visual style port**

1. Extract `aero.msstyles` (+ `.mui`) from the 8250 ISO: `tools/extract_theme.sh`.
2. Diff its class/part/state table against the target's (the msstyles format
   changed between 8 and 10): `tools/msstyles_dump.py`, `tools/msstyles_diff.py`.
3. Rebuild a target-compatible msstyles with
   [msstyleEditor](https://github.com/nptr/msstyleEditor), mapping the 8250 assets
   onto the target's parts.
4. Load it: unsigned themes need [SecureUxTheme](https://github.com/namazso/SecureUxTheme).
   Tested in a VM snapshot first.
5. Screenshot checklist: caption buttons, window frame and glass, taskbar, start button.

**Phase B (only if A looks good): shell transplant**, in the style of
[Keel](https://github.com/bulutthecat/KeelPublic) (GPL-3.0), with 8250 as the
donor. Keel's offsets and import seams are tied to its builds (Win10 LTSC 2021
host, Win7 SP1 donor), so all of them would have to be regenerated for 8250 and
26200.

## Status

| Step | State |
|---|---|
| Target (26200.6584) msstyles extracted and dumped: 9449 records, 342 classes | ✅ |
| Dump/diff tools | ✅ |
| Test VM (Win11 25H2 Pro for Workstations, unattended) | in progress |
| 8250 ISO | waiting for the download |

## Tools

Linux host; `pip install --target tools/pydeps pefile`, plus `7z` and `wimlib-imagex`.

```
tools/extract_theme.sh <iso> <edition regex> <out dir>
    # theme files, uxtheme, DWM and shell binaries of one edition, plus a .tsv per msstyles
tools/extract_theme.sh Win11_25H2_German_x64.iso 'ProfessionalWorkstation$' work/win11
tools/extract_theme.sh 8250.iso . work/8250

tools/msstyles_dump.py <file.msstyles>            # class, part, state, property, type, size
tools/msstyles_diff.py work/8250/aero.tsv work/win11/aero.tsv '^(Window|DWMWindow|TaskBar)'
```

msstyles layout as read by the dump: `CMAP` holds the class names (UTF-16, each
padded to 8 bytes, index = class id); `VARIANT/NORMAL` holds the property records,
8 x int32 (property, type, class, part, state, short flag, reserved, size), with the
value inline when the short flag is set and otherwise `size` bytes padded to 8.

## Test VM

`vmsetup/autounattend.xml` installs Win11 25H2 Pro for Workstations without
interaction: German, no TPM/Secure Boot check (the host has no swtpm), local
account `test`/`test` with autologon, OpenSSH server. Packed with
`xorriso -as mkisofs -J -r -V UNATTEND -o unattend.iso vmsetup/autounattend.xml`
and attached as a second CD next to the Windows ISO. The product key in it is
Microsoft's public generic install key: it installs but does not activate.

## Credits

Projects this one uses or builds on:

- **[Keel](https://github.com/bulutthecat/KeelPublic)** by bulutthecat (GPL-3.0): the
  idea and method for Phase B, running an older Windows' real DWM, theme engine and
  shell on a newer kernel.
- **[msstyleEditor](https://github.com/nptr/msstyleEditor)** by nptr (MIT): editing and
  rebuilding the visual style; its reading of the msstyles format guided `msstyles_dump.py`.
- **[SecureUxTheme](https://github.com/namazso/SecureUxTheme)** by namazso (LGPL-2.1):
  loading unsigned visual styles without patching system files.
- **[pefile](https://github.com/erocarrera/pefile)** by Ero Carrera (MIT): PE resource
  parsing in the dump tool.
- **[wimlib](https://wimlib.net)** (GPL-3.0/LGPL-3.0) and **[7-Zip](https://7-zip.org)**
  (LGPL-2.1): reading the ISOs and WIM images.
- **[QEMU](https://www.qemu.org)/[libvirt](https://libvirt.org)** and
  **[EDK2 OVMF](https://github.com/tianocore/edk2)**: the test VM.

Windows, its visual styles and the Windows 8 beta artwork belong to Microsoft. This
repository contains none of their files.

## License

GPL-3.0 (see `LICENSE`), so code from Keel can be reused in Phase B.
