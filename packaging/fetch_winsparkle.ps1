# Fetches the update library (WinSparkle) for a Windows build and prints the folder to
# give to CMake as BACKHAUL_WINSPARKLE_DIR. The file is checked against a known
# checksum, so a changed download stops the build instead of ending up in the program.
#   $dir = & packaging\fetch_winsparkle.ps1 -Into $env:RUNNER_TEMP
param([Parameter(Mandatory = $true)][string]$Into)
$ErrorActionPreference = 'Stop'
$version = '0.9.4'   # https://github.com/vslavik/winsparkle/releases
$sha256 = '6037df37fc263bd1650a1c4949681a9d40ffe991d01f35892a406cb5d103c976'
$zip = Join-Path $Into "WinSparkle-$version.zip"
curl.exe -sSL --fail --retry 3 --retry-all-errors --max-time 180 -o $zip "https://github.com/vslavik/winsparkle/releases/download/v$version/WinSparkle-$version.zip"
if ($LASTEXITCODE -ne 0) { throw "WinSparkle $version could not be downloaded" }
$got = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLowerInvariant()
if ($got -ne $sha256) { throw "WinSparkle-$version.zip is not the expected file (sha256 $got)" }
$out = Join-Path $Into 'winsparkle'
Expand-Archive -Path $zip -DestinationPath $out -Force
$dir = Join-Path $out "WinSparkle-$version"
foreach ($f in 'include\winsparkle.h', 'x64\Release\WinSparkle.lib', 'x64\Release\WinSparkle.dll') {
  if (-not (Test-Path (Join-Path $dir $f))) { throw "WinSparkle $version has no $f" }
}
$dir
