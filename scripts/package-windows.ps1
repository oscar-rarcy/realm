$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$bin = Join-Path $root "bin"
$out = Join-Path $bin "realm-windows.zip"

$files = Get-ChildItem -Path $bin -File | Where-Object {
    $_.Name -eq "realm.exe" -or $_.Extension -eq ".dll"
}

if (-not $files) {
    throw "No package files found in $bin"
}

Compress-Archive -Force -Path $files.FullName -DestinationPath $out
Write-Host "Wrote $out"
