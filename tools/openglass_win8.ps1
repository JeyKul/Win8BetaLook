# OpenGlass settings for the Windows 8 beta look (HKCU, see OpenGlass's registry reference).
# Restarts DWM so they take effect.
$k = 'HKCU:\SOFTWARE\Microsoft\Windows\DWM'
$v = [ordered]@{
    GlassType            = 1  # Windows 7/8 glass
    CaptionButtons       = 3  # Windows 8 caption button layout
    CenterCaption        = 2  # Windows 8 title centering
    DisableModernBorders = 1  # no Windows 11 border
    RoundRectRadius      = 0  # square corners
}
foreach ($p in $v.GetEnumerator()) { Set-ItemProperty $k -Name $p.Key -Value $p.Value -Type DWord }
Stop-Process -Name dwm -Force
"PASS OpenGlass Win8 settings"
