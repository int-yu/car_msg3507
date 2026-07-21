# flash.ps1 - Flash MSPM0G3507 via pyOCD.
# Missing tools are installed automatically before any target operation.
# Usage:
#   .\flash.ps1              Flash Debug\ArcLineTest.out and reset to run
#   .\flash.ps1 -Erase       Mass-erase MAIN then flash (NONMAIN untouched)
#   .\flash.ps1 -List        List connected debug probes
#   .\flash.ps1 -Check       Check the environment and DAPLink connection only
#   .\flash.ps1 -Reset       Reset only
#   .\flash.ps1 -Freq 500k   Override SWD clock (default: 1M)

param(
    [switch]$Erase,
    [switch]$List,
    [switch]$Check,
    [switch]$Reset,
    [switch]$UnderReset,
    [string]$Freq = "1M"
)

$target = "mspm0g3507"
$image  = Join-Path $PSScriptRoot "Debug\ArcLineTest.out"

function Find-WorkingPython {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        & $python.Source -c "import sys" 2>$null
        if ($LASTEXITCODE -eq 0) { return $python.Source }
    }

    $launcher = Get-Command py -ErrorAction SilentlyContinue
    if ($launcher) {
        & $launcher.Source -3 -c "import sys" 2>$null
        if ($LASTEXITCODE -eq 0) { return $launcher.Source }
    }

    return $null
}

function Find-PyOCD {
    param([string]$PythonExe)

    $command = Get-Command pyocd -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if (-not $PythonExe) { return $null }

    $pythonPrefix = @()
    if ([IO.Path]::GetFileName($PythonExe) -ieq "py.exe") {
        $pythonPrefix = @("-3")
    }

    $scriptDirs = @()
    $scriptDirs += & $PythonExe @pythonPrefix -c "import sysconfig; print(sysconfig.get_path('scripts'))" 2>$null
    $scriptDirs += & $PythonExe @pythonPrefix -c "import sysconfig; print(sysconfig.get_path('scripts', scheme='nt_user'))" 2>$null

    foreach ($dir in ($scriptDirs | Select-Object -Unique)) {
        if (-not $dir) { continue }
        $candidate = Join-Path $dir "pyocd.exe"
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }

    return $null
}

function Install-Python {
    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $winget) {
        Write-Host "Python and winget were not found. Install Python 3, then retry." -ForegroundColor Red
        return $null
    }

    Write-Host "Python was not found. Installing Python 3.12 for the current user..." -ForegroundColor Yellow
    & $winget.Source install --id Python.Python.3.12 --exact --scope user --accept-package-agreements --accept-source-agreements 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Python installation failed (exit $LASTEXITCODE)." -ForegroundColor Red
        return $null
    }

    $installed = Get-ChildItem -LiteralPath (Join-Path $env:LOCALAPPDATA "Programs\Python") -Filter python.exe -File -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if ($installed) { return $installed.FullName }

    return Find-WorkingPython
}

function Install-PyOCD {
    param([string]$PythonExe)

    $pythonPrefix = @()
    if ([IO.Path]::GetFileName($PythonExe) -ieq "py.exe") {
        $pythonPrefix = @("-3")
    }

    & $PythonExe @pythonPrefix -m pip --version *> $null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "pip was not found. Installing pip..." -ForegroundColor Yellow
        & $PythonExe @pythonPrefix -m ensurepip --upgrade 2>&1 | Out-Host
        if ($LASTEXITCODE -ne 0) { return $null }
    }

    Write-Host "pyOCD was not found. Installing it from PyPI..." -ForegroundColor Yellow
    & $PythonExe @pythonPrefix -m pip install pyocd --index-url https://pypi.org/simple --disable-pip-version-check --progress-bar off --timeout 30 --retries 2 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Standard installation failed; retrying as a per-user install..." -ForegroundColor Yellow
        & $PythonExe @pythonPrefix -m pip install --user pyocd --index-url https://pypi.org/simple --disable-pip-version-check --progress-bar off --timeout 30 --retries 2 2>&1 | Out-Host
        if ($LASTEXITCODE -ne 0) { return $null }
    }

    return Find-PyOCD -PythonExe $PythonExe
}

function Test-DAPLinkConnection {
    param([string]$PyOCDExe)

    $probeOutput = & $PyOCDExe list --probes --no-header 2>&1 | Out-String
    $probeExitCode = $LASTEXITCODE
    if ($probeExitCode -ne 0) {
        Write-Host "Unable to enumerate debug probes (exit $probeExitCode)." -ForegroundColor Red
        return $false
    }

    $dapLines = @($probeOutput -split "`r?`n" | Where-Object {
        $_ -match "(?i)(CMSIS-DAP|DAPLink)"
    })

    if ($dapLines.Count -eq 0) {
        Write-Host "DAPLink/CMSIS-DAP probe not found." -ForegroundColor Red
        Write-Host "Connect the probe by USB, then check SWDIO, SWCLK, GND and target power." -ForegroundColor Yellow
        return $false
    }

    if ($dapLines.Count -gt 1) {
        Write-Host "More than one DAPLink/CMSIS-DAP probe is connected." -ForegroundColor Red
        Write-Host "Disconnect extra probes to avoid flashing the wrong target." -ForegroundColor Yellow
        $dapLines | ForEach-Object { Write-Host "  $($_.Trim())" -ForegroundColor Yellow }
        return $false
    }

    Write-Host "DAPLink detected: $($dapLines[0].Trim())" -ForegroundColor DarkGray
    return $true
}

Write-Host "Checking flash environment..." -ForegroundColor DarkGray
$pythonExe = Find-WorkingPython
$pyocdExe = Find-PyOCD -PythonExe $pythonExe

if (-not $pyocdExe) {
    if (-not $pythonExe) { $pythonExe = Install-Python }
    if (-not $pythonExe) { exit 2 }

    $pyocdExe = Install-PyOCD -PythonExe $pythonExe
    if (-not $pyocdExe) {
        Write-Host "pyOCD installation failed." -ForegroundColor Red
        exit 2
    }
}

$packOutput = & $pyocdExe pack show 2>&1 | Out-String
$packExitCode = $LASTEXITCODE
$hasTargetPack = ($packExitCode -eq 0) -and ($packOutput -match "TexasInstruments\.MSPM0(G_DFP|G1X0X_G3X0X_DFP)")

if (-not $hasTargetPack) {
    Write-Host "MSPM0G3507 device support was not found. Installing the CMSIS Device Pack..." -ForegroundColor Yellow
    Write-Host "The first Pack index download can take several minutes." -ForegroundColor DarkGray
    & $pyocdExe pack install $target
    if ($LASTEXITCODE -ne 0) {
        Write-Host "MSPM0G3507 Device Pack installation failed (exit $LASTEXITCODE)." -ForegroundColor Red
        exit 2
    }
}

Write-Host "Flash environment is ready." -ForegroundColor DarkGray

$connect = @()
if ($UnderReset) { $connect = @("--connect", "under-reset") }

if ($List)  { & $pyocdExe list; exit $LASTEXITCODE }
if (-not (Test-DAPLinkConnection -PyOCDExe $pyocdExe)) { exit 3 }
if ($Check) { Write-Host "Environment and DAPLink checks passed." -ForegroundColor Green; exit 0 }
if ($Reset) { & $pyocdExe reset -t $target -f $Freq; exit $LASTEXITCODE }

if (-not (Test-Path -LiteralPath $image)) {
    Write-Host "Image not found: $image -- build it in CCS first." -ForegroundColor Red
    exit 1
}

if ($Erase) {
    Write-Host "Mass-erasing MAIN region (NONMAIN untouched)..." -ForegroundColor Yellow
    & $pyocdExe erase -t $target --chip -f $Freq @connect
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERASE FAILED (exit $LASTEXITCODE)." -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

Write-Host "Flashing $image (SWD $Freq) ..." -ForegroundColor Cyan
& $pyocdExe flash -t $target --format elf -f $Freq @connect $image
if ($LASTEXITCODE -ne 0) {
    Write-Host "FLASH FAILED (exit $LASTEXITCODE)." -ForegroundColor Red
    Write-Host "Retry with -Freq 500k or 100k. If NRST is wired, also try -UnderReset." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

& $pyocdExe reset -t $target -f $Freq
if ($LASTEXITCODE -ne 0) {
    Write-Host "Flash succeeded, but reset failed (exit $LASTEXITCODE)." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

Write-Host "Done. Chip reset and running new firmware." -ForegroundColor Green
