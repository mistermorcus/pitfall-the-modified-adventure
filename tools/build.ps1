param(
    [string]$Compiler,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "..\artifacts")
)
$ErrorActionPreference = "Stop"
if (-not $Compiler) {
    $command = Get-Command "i686-w64-mingw32-clang.exe" -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "i686-w64-mingw32-clang.exe was not found. Pass -Compiler <path>."
    }
    $Compiler = $command.Source
}
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
& $Compiler -O2 -s -shared -o (Join-Path $OutputDirectory "PITMOD.DLL") `
    "-Wl,--out-implib,$(Join-Path $OutputDirectory 'libpitmod.dll.a')" `
    (Join-Path $root "src\pitmod.c") -lgdi32 -luser32 -lkernel32 -lcomctl32 -ldwmapi -ld3d9 -lwinmm
if ($LASTEXITCODE -ne 0) { throw "PITMOD.DLL build failed." }
& $Compiler -O2 -s -shared -o (Join-Path $OutputDirectory "WAIL32.DLL") `
    (Join-Path $root "third_party\pitfall-win11-wail\wail32.c") `
    (Join-Path $root "third_party\pitfall-win11-wail\wail32.def") `
    "-L$OutputDirectory" -lpitmod -ldsound -lwinmm -lole32
if ($LASTEXITCODE -ne 0) { throw "WAIL32.DLL build failed." }
& $Compiler -O2 -s -shared -o (Join-Path $OutputDirectory "WINMM.dll") `
    (Join-Path $root "src\winmm_proxy.c") `
    (Join-Path $root "src\winmm_proxy.def") -lkernel32
if ($LASTEXITCODE -ne 0) { throw "WINMM.dll proxy build failed." }
Write-Host "Built PITMOD.DLL, WAIL32.DLL, and WINMM.dll in $OutputDirectory"
