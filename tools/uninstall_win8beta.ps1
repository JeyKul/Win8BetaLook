# Back to stock Aero; removes the Win8Beta files and the OpenGlass values set by the installer.
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$T = "$env:WINDIR\Resources\Themes"
& "$here\set_visual_style.ps1" -Style "$T\aero\aero.msstyles" -Log "$env:TEMP\win8beta.log"
Remove-Item "$T\Win8Beta", "$T\Win8Beta.theme" -Recurse -Force -ErrorAction SilentlyContinue
$D = 'HKCU:\SOFTWARE\Microsoft\Windows\DWM'
'GlassType','CaptionButtons','CenterCaption','DisableModernBorders','RoundRectRadius','ColorizationColorBalanceOverride',
'ColorizationAfterglowBalanceOverride','ColorizationBlurBalanceOverride' | % { Remove-ItemProperty $D -Name $_ -ErrorAction SilentlyContinue }
Stop-Process -Name dwm -Force -ErrorAction SilentlyContinue
"Removed. Uninstall OpenGlass, SecureUxTheme, ExplorerPatcher and the Windhawk mod separately if you want."
