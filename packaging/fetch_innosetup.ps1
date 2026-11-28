# Installs the installer maker (Inno Setup) into a folder of the build machine and prints
# the path of its command line compiler (ISCC.exe). Checked against a known checksum.
#   $iscc = & packaging\fetch_innosetup.ps1 -Into $env:RUNNER_TEMP
param([Parameter(Mandatory = $true)][string]$Into)
$ErrorActionPreference = 'Stop'
$version = '7.1.1'   # https://jrsoftware.org/isdl.php (7.2.0 was still a preview on 2026-11-28)
$sha256 = '8a5a6711c3766a6e5fb74f280ba0e3551e719ce94538d901fdbd1f8f9920b3f0'
$tag = 'is-' + $version.Replace('.', '_')
$exe = Join-Path $Into "innosetup-$version-x64.exe"
curl.exe -sSL --fail --max-time 180 -o $exe "https://github.com/jrsoftware/issrc/releases/download/$tag/innosetup-$version-x64.exe"
if ($LASTEXITCODE -ne 0) { throw "Inno Setup $version could not be downloaded" }
$got = (Get-FileHash -Algorithm SHA256 $exe).Hash.ToLowerInvariant()
if ($got -ne $sha256) { throw "innosetup-$version-x64.exe is not the expected file (sha256 $got)" }
$dir = Join-Path $Into 'innosetup'
$p = Start-Process -FilePath $exe -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/CURRENTUSER', "/DIR=`"$dir`"" -Wait -PassThru
if ($p.ExitCode -ne 0) { throw "Inno Setup $version could not be installed (exit code $($p.ExitCode))" }
$iscc = Join-Path $dir 'ISCC.exe'
if (-not (Test-Path $iscc)) { throw "Inno Setup $version has no ISCC.exe in $dir" }
if (-not (Test-Path (Join-Path $dir 'Languages\Japanese.isl'))) { throw "Inno Setup $version has no Japanese texts" }
$iscc
