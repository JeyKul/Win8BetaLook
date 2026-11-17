# In the VM: build <Name>.msstyles from aero + the resource files in C:\w8\try\<Name>,
# install it with the German .mui and try to load it in the desktop session.
param([string]$Name)
$T = 'C:\Windows\Resources\Themes'
& C:\w8\apply_resources.ps1 -Source "$T\aero\aero.msstyles" -Target "$T\$Name\$Name.msstyles" -Dir "C:\w8\try\$Name" | Out-Null
New-Item -ItemType Directory -Force "$T\$Name\de-DE" | Out-Null
Copy-Item "$T\aero\de-DE\aero.msstyles.mui" "$T\$Name\de-DE\$Name.msstyles.mui" -Force
schtasks /create /tn w8style /tr "powershell -ExecutionPolicy Bypass -WindowStyle Hidden -File C:\w8\set_visual_style.ps1 -Style $T\$Name\$Name.msstyles" /sc once /st 23:59 /it /ru test /f | Out-Null
schtasks /run /tn w8style | Out-Null
Start-Sleep 7
(Get-Content C:\w8\setstyle.log -Tail 1) -replace '.*hr=', "$Name hr="
