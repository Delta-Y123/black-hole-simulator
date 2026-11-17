param(
    [string]$BuildDir = 'build',
    [switch]$Clean,
    [switch]$TestsOnly,
    [switch]$NoCuda
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# Locate the Visual C++ environment. Set BH_VCVARS to override.
$vcvars = $env:BH_VCVARS
if (-not $vcvars) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -property installationPath
        if ($vsPath) { $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat' }
    }
}
if (-not $vcvars -or -not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found. Install Visual Studio Build Tools or set BH_VCVARS."
}

# Locate Qt 6. Set BH_QT_PREFIX to override.
$qt = $env:BH_QT_PREFIX
if (-not $qt) {
    $candidates = @('D:/Qt/6.8.3/msvc2022_64', 'C:/Qt/6.8.3/msvc2022_64')
    foreach ($c in $candidates) {
        if (Test-Path "$c/lib/cmake/Qt6") { $qt = $c; break }
    }
}
if (-not $qt -or -not (Test-Path "$qt/lib/cmake/Qt6")) {
    throw "Qt 6 not found. Install it or set BH_QT_PREFIX to the msvc2022_64 prefix."
}

# Locate the CUDA toolkit. Set BH_CUDA_ROOT to override.
$cuda = $env:BH_CUDA_ROOT
if (-not $cuda) {
    $candidates = @(
        'C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.0',
        'D:/downloads'
    )
    foreach ($c in $candidates) {
        if (Test-Path "$c/bin/nvcc.exe") { $cuda = $c; break }
    }
}

$buildPath = Join-Path $root $BuildDir
if ($Clean -and (Test-Path $buildPath)) { Remove-Item $buildPath -Recurse -Force }

$targets = if ($TestsOnly) { '--target bh_tests' } else { '' }
$cudaFlag = if ($NoCuda) { 'OFF' } else { 'ON' }

$cmakeLine = "cmake -S . -B `"$buildPath`" -G Ninja -DCMAKE_BUILD_TYPE=Release" +
    " -DBH_ENABLE_CUDA=$cudaFlag -DCMAKE_PREFIX_PATH=`"$qt`" -DQT_PREFIX=`"$qt`""
if (-not $NoCuda -and $cuda) {
    $cmakeLine += " -DCUDAToolkit_ROOT=`"$cuda`""
}

$lines = @(
    "call `"$vcvars`" >nul 2>&1",
    "cd /d `"$root`"",
    $cmakeLine,
    "if errorlevel 1 exit /b 1",
    "cmake --build `"$buildPath`" --parallel $targets"
)

$tmp = Join-Path $env:TEMP ("bh_build_" + [guid]::NewGuid().ToString('N') + ".bat")
Set-Content -Path $tmp -Value ($lines -join "`r`n") -Encoding ASCII
try {
    & cmd.exe /c $tmp
    exit $LASTEXITCODE
} finally {
    Remove-Item $tmp -ErrorAction SilentlyContinue
}
