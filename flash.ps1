# flash.ps1 - Flash MSPM0G3507 via pyOCD (bypasses CCS/DSLite non-ASCII username bug)
# Usage:
#   .\flash.ps1              Flash Debug\ArcLineTest.out and reset to run
#   .\flash.ps1 -Erase       Mass-erase MAIN then flash (does NOT touch NONMAIN, safe)
#   .\flash.ps1 -List        Just list connected debug probes
#   .\flash.ps1 -Reset       Just reset, no flashing
#   .\flash.ps1 -Freq 500k   Override SWD clock (default 1M; lower = more stable on cheap probes)

param(
    [switch]$Erase,
    [switch]$List,
    [switch]$Reset,
    [switch]$UnderReset,   # cheap DAPLink can't halt a running core; needs RST wired to MCU NRST
    [string]$Freq = "1M"
)

$target = "mspm0g3507"
$image  = Join-Path $PSScriptRoot "Debug\ArcLineTest.out"

# --connect under-reset holds the core in reset so the running firmware never
# interferes with flashing (mimics what the XDS110 pre-halt does).
$connect = @()
if ($UnderReset) { $connect = @("--connect", "under-reset") }

if ($List)  { pyocd list; return }
if ($Reset) { pyocd reset -t $target -f $Freq; return }

if (-not (Test-Path $image)) {
    Write-Host "Image not found: $image  -- build it in CCS first" -ForegroundColor Red
    exit 1
}

if ($Erase) {
    Write-Host "Mass-erasing MAIN region (NONMAIN untouched)..." -ForegroundColor Yellow
    pyocd erase -t $target --chip -f $Freq @connect
}

Write-Host "Flashing $image  (SWD $Freq) ..." -ForegroundColor Cyan
pyocd flash -t $target --format elf -f $Freq @connect $image
if ($LASTEXITCODE -ne 0) {
    Write-Host "FLASH FAILED (exit $LASTEXITCODE)." -ForegroundColor Red
    Write-Host "Tips: use the XDS110 probe (reliable), or retry with -Freq 500k / 100k." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

pyocd reset -t $target -f $Freq
Write-Host "Done. Chip reset and running new firmware." -ForegroundColor Green
