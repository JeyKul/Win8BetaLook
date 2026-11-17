# Apply a visual style directly (what ThemeTool/ThemeToolSharp do): uxtheme's
# SetSystemVisualStyle, ordinal 65. Run in the user's desktop session; logs to -Log.
# Works on unactivated test VMs, where Settings refuses to change themes.
param([string]$Style, [string]$Color = 'NormalColor', [string]$Size = 'NormalSize', [string]$Log = 'C:\w8\setstyle.log')
Add-Type -Namespace W -Name U -MemberDefinition @'
[DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibrary(string f);
[DllImport("kernel32.dll")] public static extern IntPtr GetProcAddress(IntPtr m, IntPtr ordinal);
public delegate int SetStyle([MarshalAs(UnmanagedType.LPWStr)] string file, [MarshalAs(UnmanagedType.LPWStr)] string color, [MarshalAs(UnmanagedType.LPWStr)] string size, int flags);
'@
$p = [W.U]::GetProcAddress([W.U]::LoadLibrary('uxtheme.dll'), [IntPtr]65)
$f = [Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer($p, [W.U+SetStyle])
$hr = $f.Invoke($Style, $Color, $Size, 0)
"$(Get-Date -f s) SetSystemVisualStyle($Style) hr=0x{0:X8}" -f $hr | Add-Content $Log
