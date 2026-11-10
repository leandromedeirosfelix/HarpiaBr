# (local: Harpia) The Harpia Builder package folder, from this checkout's HEAD (what scripts/make_harpia_setup.py packs into
# HarpiaSetup.exe). On the release PC; nothing here is downloaded.
#   <Out>\HarpiaBuilder.exe (+ .NET, self-contained), .NET's licence and third-party notices (dotnet-*), LICENSE
#   <Out>\tools\{gpu,host,nx2,scripts,third_party}   git archive HEAD, + the prebuilt a64emit/zbic libraries, SDL2.dll
#   <Out>\tools\{python,emsdk,cpm,wgpu-native}       the build tools, from -From (required: an earlier Builder package's tools folder)
#   <Out>\tools\third_party\streamline              Streamline's MIT headers and licence only (DLSS compiled in; no NVIDIA DLL)
#   <Out>\tools\work                                Harpia's play profile (hot functions, order, PGO profile, icon)
#   <Out>\tools\{LICENSES,NOTICE.md,CREDITS*.md}    the repository's licence texts, notice and credits
# Left out: Unicorn (Python tests only, GPLv2), Tcl/Tk, the duplicate lld-link.exe (made from emsdk's lld on first use),
# VMA's sample, wgpu's patch files (the DLL shipped is upstream's, unmodified), NVIDIA's DLLs, llvm-mingw. Added: LLVM's
# licence (emsdk/upstream), FFmpeg's LGPL-2.1, source and relinking notice and NVIDIA's nv-codec-headers MIT notice.
# (no machine's own paths: the work folder beside the checkout, the tools of an earlier Builder package given with -From, the
# Streamline SDK as build.py takes it, RX_STREAMLINE_SDK)
param([string]$Out = (Join-Path (Split-Path (Split-Path $PSScriptRoot)) "work\builder-pkg\HarpiaBuilder"),
      [Parameter(Mandatory = $true)][string]$From,
      [string]$Streamline = $env:RX_STREAMLINE_SDK,
      [string]$Work = (Join-Path (Split-Path (Split-Path $PSScriptRoot)) "work"),
      [string]$DotnetPacks = "$env:USERPROFILE\.nuget\packages")
$ErrorActionPreference = "Stop"
if (-not $Streamline -or -not (Test-Path (Join-Path $Streamline "include\sl.h"))) { throw "set -Streamline (or RX_STREAMLINE_SDK) to the Streamline SDK folder: include\sl.h" }
[System.Diagnostics.Process]::GetCurrentProcess().PriorityClass = "BelowNormal"
$src = Split-Path $PSScriptRoot
$notices = "$src\builder\package-notices"
if (Test-Path $Out) { Remove-Item $Out -Recurse -Force }
$pub = "$Out.publish"
if (Test-Path $pub) { Remove-Item $pub -Recurse -Force }
& dotnet publish "$src\builder\HarpiaBuilder\HarpiaBuilder.csproj" -c Release -o $pub --nologo -v q
if ($LASTEXITCODE) { throw "dotnet publish failed" }
Move-Item $pub $Out
Copy-Item "$src\LICENSE" "$Out\LICENSE"
foreach ($pack in @(@("microsoft.netcore.app.runtime.win-x64", "dotnet"), @("microsoft.windowsdesktop.app.runtime.win-x64", "dotnet-desktop"))) {
    $rt = Get-ChildItem "$DotnetPacks\$($pack[0])" -Directory | Sort-Object Name | Select-Object -Last 1
    # (the WindowsDesktop pack names its licence "LICENSE", no extension: copied as dotnet-desktop-LICENSE.TXT)
    foreach ($f in "LICENSE.TXT", "LICENSE", "THIRD-PARTY-NOTICES.TXT") { if (Test-Path -LiteralPath "$($rt.FullName)\$f" -PathType Leaf) { Copy-Item -LiteralPath "$($rt.FullName)\$f" "$Out\$($pack[1])-$(if ($f -eq "LICENSE") { "LICENSE.TXT" } else { $f })" } }
    if (-not (Test-Path "$Out\$($pack[1])-LICENSE.TXT")) { throw "no licence for $($pack[0])" }
}
$tools = "$Out\tools"
New-Item -ItemType Directory -Force $tools | Out-Null
$tar = "$Out.tar"
& git -C $src archive --format=tar -o $tar HEAD gpu host nx2 scripts third_party LICENSES NOTICE.md CREDITS.md CREDITS.en.md
if ($LASTEXITCODE) { throw "git archive failed" }
& tar -xf $tar -C $tools; Remove-Item $tar
(& git -C $src rev-parse --short HEAD) | Set-Content "$tools\version.txt" -Encoding ascii
foreach ($f in "a64-kit\lift\a64emit.dll", "a64-kit\lift\a64emit.lib", "zbic\zbic.dll", "zbic\zbic.lib") { Copy-Item "$From\third_party\$f" "$tools\third_party\$f" -Force }
Copy-Item "$src\third_party\sdl2\SDL2.dll" "$tools\third_party\sdl2\SDL2.dll" -Force   # (the one the development build ships)
foreach ($d in "python", "emsdk", "cpm", "wgpu-native") {
    & robocopy "$From\$d" "$tools\$d" /E /NFL /NDL /NJH /NJS /NP /MT:8 /XD totk-patches | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy $d failed ($LASTEXITCODE)" }
}
# (left out, see the header)
$py = "$tools\python"
Remove-Item "$py\Lib\site-packages\unicorn", "$py\Lib\site-packages\unicorn-*.dist-info" -Recurse -Force -ErrorAction SilentlyContinue
foreach ($f in "tcl90.dll", "tcl9tk90.dll", "_tkinter.pyd", "libtommath.dll") { Remove-Item "$py\DLLs\$f" -ErrorAction SilentlyContinue }
Remove-Item "$tools\cpm\vulkanmemoryallocator\*\bin\VmaSample_*.exe" -ErrorAction SilentlyContinue
# (added: the licences of what the package carries)
Copy-Item "$notices\LLVM-LICENSE.TXT" "$tools\emsdk\upstream\LICENSE.TXT"
foreach ($ff in Get-ChildItem "$tools\cpm\ffmpeg" -Directory) {
    Copy-Item "$notices\FFmpeg-COPYING.LGPLv2.1" "$($ff.FullName)\COPYING.LGPLv2.1"
    Copy-Item "$notices\FFmpeg-SOURCE.txt" "$($ff.FullName)\SOURCE.txt"
    Copy-Item "$notices\nv-codec-headers-MIT.txt" "$($ff.FullName)\nv-codec-headers-MIT.txt"
}
New-Item -ItemType Directory -Force "$tools\third_party\streamline" | Out-Null
& robocopy "$Streamline\include" "$tools\third_party\streamline\include" /E /NFL /NDL /NJH /NJS /NP | Out-Null
Copy-Item "$Streamline\license.txt" "$tools\third_party\streamline\license.txt"
New-Item -ItemType Directory -Force "$tools\work\image" | Out-Null
foreach ($f in "harpia_pieces.prof", "harpia.res", "hot_funcs.txt", "hot_order.txt") { Copy-Item "$Work\$f" "$tools\work\$f" }
Copy-Item "$Work\image\chunks.txt" "$tools\work\image\chunks.txt"
# (checks: no NVIDIA library, no llvm-mingw, the Streamline headers there)
$nv = @(Get-ChildItem $Out -Recurse -File | Where-Object { $_.Extension -eq ".dll" -and $_.Name -match "^(nvngx|sl\.|NvLowLatency)" })
if ($nv.Count) { throw "NVIDIA DLLs in the package: $($nv.FullName -join ', ')" }
if (Test-Path "$tools\llvm-mingw") { throw "llvm-mingw in the package" }
if (-not (Test-Path "$tools\third_party\streamline\include\sl.h")) { throw "no Streamline headers: DLSS would be compiled out" }
$size = (Get-ChildItem $Out -Recurse -File | Measure-Object Length -Sum).Sum
"package: $Out  version $(Get-Content "$tools\version.txt")  $([math]::Round($size / 1GB, 2)) GB"
Get-ChildItem $tools -Directory | ForEach-Object { "{0,-14} {1,8:0} MB" -f $_.Name, ((Get-ChildItem $_.FullName -Recurse -File -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum / 1MB) }
