# Write resources into a copy of an msstyles with Windows' own UpdateResource.
# Usage: apply_resources.ps1 -Source C:\Windows\Resources\Themes\aero\aero.msstyles `
#          -Target C:\Win8Beta\Win8Beta.msstyles -Dir C:\build
# Every file in -Dir named <TYPE>_<NAME>.<ext> replaces resource TYPE/NAME
# (NAME numeric = id, otherwise a string name), language 1033 like the originals.
param([string]$Source, [string]$Target, [string]$Dir)
$ErrorActionPreference = 'Stop'
Add-Type -Namespace W -Name R -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
public static extern IntPtr BeginUpdateResource(string file, bool deleteExisting);
[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
public static extern bool UpdateResource(IntPtr h, string type, IntPtr name, ushort lang, byte[] data, uint size);
[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode, EntryPoint="UpdateResource")]
public static extern bool UpdateResourceS(IntPtr h, string type, string name, ushort lang, byte[] data, uint size);
[DllImport("kernel32.dll", SetLastError=true)]
public static extern bool EndUpdateResource(IntPtr h, bool discard);
'@
New-Item -ItemType Directory -Force (Split-Path $Target) | Out-Null
Copy-Item $Source $Target -Force
$h = [W.R]::BeginUpdateResource($Target, $false)
if ($h -eq [IntPtr]::Zero) { throw "BeginUpdateResource failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
foreach ($f in Get-ChildItem $Dir -File) {
    $type, $name = $f.BaseName -split '_', 2
    $bytes = [IO.File]::ReadAllBytes($f.FullName)
    # Same language as the existing resource (1033), or a second copy is added
    if ($name -match '^\d+$') { $ok = [W.R]::UpdateResource($h, $type, [IntPtr][int]$name, 1033, $bytes, $bytes.Length) }
    else { $ok = [W.R]::UpdateResourceS($h, $type, $name, 1033, $bytes, $bytes.Length) }
    if (-not $ok) { [W.R]::EndUpdateResource($h, $true) | Out-Null; throw "UpdateResource $type/$name failed" }
    "PASS $type/$name ($($bytes.Length) bytes)"
}
if (-not [W.R]::EndUpdateResource($h, $false)) { throw "EndUpdateResource failed" }
"PASS wrote $Target"
