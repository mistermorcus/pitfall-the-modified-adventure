param(
    [string]$PayloadDirectory = (Join-Path $PSScriptRoot "payload"),
    [string]$OutputName = "Pitfall-Modified-Setup.exe"
)
$ErrorActionPreference = "Stop"
$project = $PayloadDirectory
$compiler = "C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if (-not (Test-Path -LiteralPath $compiler)) {
    throw "The .NET Framework C# compiler was not found."
}
foreach ($name in @("PITMOD.DLL", "WAIL32.DLL", "WINMM.dll", "PITMOD.INI", "THIRD_PARTY_NOTICES.txt")) {
    if (-not (Test-Path -LiteralPath (Join-Path $project $name))) {
        throw "Missing installer payload: $name"
    }
}
$output = Join-Path $PSScriptRoot $OutputName
$pitmod = Join-Path $project "PITMOD.DLL"
$wail = Join-Path $project "WAIL32.DLL"
$winmm = Join-Path $project "WINMM.dll"
$ini = Join-Path $project "PITMOD.INI"
$notices = Join-Path $project "THIRD_PARTY_NOTICES.txt"
& $compiler /nologo /target:winexe /platform:anycpu /optimize+ `
    "/out:$output" `
    /reference:System.dll /reference:System.Core.dll `
    /reference:System.Drawing.dll /reference:System.Windows.Forms.dll `
    /reference:Microsoft.CSharp.dll `
    "/resource:$pitmod,PitfallInstaller.PITMOD.DLL" `
    "/resource:$wail,PitfallInstaller.WAIL32.DLL" `
    "/resource:$winmm,PitfallInstaller.WINMM.dll" `
    "/resource:$ini,PitfallInstaller.PITMOD.INI" `
    "/resource:$notices,PitfallInstaller.THIRD_PARTY_NOTICES.txt" `
    (Join-Path $PSScriptRoot "PitfallInstaller.cs")
if ($LASTEXITCODE -ne 0) { throw "Installer build failed." }
Get-Item -LiteralPath $output |
    Select-Object FullName, Length, LastWriteTime
