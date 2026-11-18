# Install the Win8 beta look on this PC (run as administrator from the package folder).
# Needs, installed first: SecureUxTheme (unsigned styles), OpenGlass (frames, glass),
# ExplorerPatcher (Windows 10 taskbar), Windhawk + mods/themed-taskbar-background.wh.cpp.
# Undo: uninstall_win8beta.ps1.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$T = "$env:WINDIR\Resources\Themes"
if (-not (Test-Path "$here\Win8Beta.msstyles")) { throw "Win8Beta.msstyles missing next to this script" }
$build = [int](Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').CurrentBuild
if ($build -ne 26200) { "note: built against 26200, this PC is $build; if the style misbehaves it needs a rebuild" }

# Back to Aero first if an older Win8Beta is active (the active file is locked)
& "$here\set_visual_style.ps1" -Style "$T\aero\aero.msstyles" -Log "$env:TEMP\win8beta.log"
New-Item -ItemType Directory -Force "$T\Win8Beta" | Out-Null
Copy-Item "$here\Win8Beta.msstyles" "$T\Win8Beta\" -Force
# The language file comes from this PC's own Aero, in every language it has
Get-ChildItem "$T\aero" -Directory | ? { Test-Path "$($_.FullName)\aero.msstyles.mui" } | % {
    New-Item -ItemType Directory -Force "$T\Win8Beta\$($_.Name)" | Out-Null
    Copy-Item "$($_.FullName)\aero.msstyles.mui" "$T\Win8Beta\$($_.Name)\Win8Beta.msstyles.mui" -Force
}
$s = Get-Content "$T\aero.theme" -Raw
$s = $s -replace '(?m)^DisplayName=.*$', 'DisplayName=Windows 8 Beta' -replace '(?m)^Path=%ResourceDir%\\Themes\\Aero\\Aero.msstyles', 'Path=%ResourceDir%\Themes\Win8Beta\Win8Beta.msstyles'
Set-Content "$T\Win8Beta.theme" $s -Encoding Unicode

# Windows mode dark (the taskbar uses the style's normal classes), apps light like the CP
$P = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'
Set-ItemProperty $P -Name SystemUsesLightTheme -Value 0 -Type DWord
Set-ItemProperty $P -Name AppsUseLightTheme -Value 1 -Type DWord
Set-ItemProperty $P -Name EnableTransparency -Value 1 -Type DWord

# OpenGlass: Windows 8 caption layout and centering, square corners, Windows 7 glass mix
$D = 'HKCU:\SOFTWARE\Microsoft\Windows\DWM'
$v = [ordered]@{ GlassType = 1; CaptionButtons = 3; CenterCaption = 2; DisableModernBorders = 1; RoundRectRadius = 0
                 ColorizationColorBalanceOverride = 8; ColorizationAfterglowBalanceOverride = 43; ColorizationBlurBalanceOverride = 49 }
foreach ($p in $v.GetEnumerator()) { Set-ItemProperty $D -Name $p.Key -Value $p.Value -Type DWord }

& "$here\set_visual_style.ps1" -Style "$T\Win8Beta\Win8Beta.msstyles" -Log "$env:TEMP\win8beta.log"
Get-Content "$env:TEMP\win8beta.log" -Tail 1
Stop-Process -Name dwm -Force -ErrorAction SilentlyContinue
"Done. Restart Explorer (or sign out) for the taskbar."
