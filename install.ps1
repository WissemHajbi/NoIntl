# install.ps1 — Build nointl and register it as a global command
# Usage: .\install.ps1

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoDir   = $PSScriptRoot
$BinDir    = "$env:USERPROFILE\bin"
$ExeName   = "nointl.exe"
$ExeDest   = "$BinDir\$ExeName"

# ── 1. Compile ────────────────────────────────────────────────────────────────
Write-Host "`nBuilding nointl..." -ForegroundColor Cyan

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    Write-Error "gcc not found. Please install GCC (e.g. via MSYS2/MinGW or Scoop: scoop install gcc)."
    exit 1
}

Push-Location $RepoDir
gcc -Wall -Wextra -std=c99 -O2 -o $ExeName `
    main.c data_structs.c directory.c file_reader.c text_parser.c
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed."
    exit 1
}
Pop-Location

Write-Host "Build successful." -ForegroundColor Green

# ── 2. Install to ~/bin ───────────────────────────────────────────────────────
Write-Host "Installing to $ExeDest..." -ForegroundColor Cyan

New-Item -ItemType Directory -Force -Path $BinDir | Out-Null
Copy-Item -Force "$RepoDir\$ExeName" $ExeDest

# ── 3. Add ~/bin to user PATH (permanent) ────────────────────────────────────
$currentPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($currentPath -notlike "*$BinDir*") {
    [Environment]::SetEnvironmentVariable("PATH", "$currentPath;$BinDir", "User")
    Write-Host "Added $BinDir to your user PATH." -ForegroundColor Green
} else {
    Write-Host "$BinDir is already in your PATH." -ForegroundColor Gray
}

Write-Host ""
Write-Host "Done! Open a new PowerShell window and run:" -ForegroundColor Green
Write-Host "  nointl <path-to-your-nextjs-project>" -ForegroundColor Yellow
