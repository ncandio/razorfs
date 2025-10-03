# RAZORFS Windows Test Runner (PowerShell)
# Run from: C:\Users\liber\Desktop\Testing-Razor-FS

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "  RAZORFS Testing - Windows Docker" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Set paths
$RESULTS_DIR = Join-Path $PSScriptRoot "results"
$CHARTS_DIR = Join-Path $PSScriptRoot "charts"
$DATA_DIR = Join-Path $PSScriptRoot "data"

# Create output directories
New-Item -ItemType Directory -Force -Path $RESULTS_DIR | Out-Null
New-Item -ItemType Directory -Force -Path $CHARTS_DIR | Out-Null
New-Item -ItemType Directory -Force -Path $DATA_DIR | Out-Null

Write-Host "Building Docker image..." -ForegroundColor Yellow
docker build -t razorfs-test -f Dockerfile .

if ($LASTEXITCODE -ne 0) {
    Write-Host "Docker build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Running benchmarks..." -ForegroundColor Yellow
docker run --rm --privileged `
    -v "${PSScriptRoot}:/testing" `
    -v "${RESULTS_DIR}:/results" `
    -v "${CHARTS_DIR}:/charts" `
    -v "${DATA_DIR}:/data" `
    razorfs-test bash /testing/benchmark-windows.sh

if ($LASTEXITCODE -ne 0) {
    Write-Host "Benchmark execution failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Generating graphs..." -ForegroundColor Yellow
docker run --rm `
    -v "${PSScriptRoot}:/testing" `
    -v "${DATA_DIR}:/data" `
    -v "${CHARTS_DIR}:/charts" `
    razorfs-test gnuplot /testing/visualize-windows.gnuplot

if ($LASTEXITCODE -ne 0) {
    Write-Host "Graph generation failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Tests Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Results:  $RESULTS_DIR" -ForegroundColor White
Write-Host "  Graphs:   $CHARTS_DIR" -ForegroundColor White
Write-Host "  Data:     $DATA_DIR" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# Open results folder
Start-Process explorer.exe $CHARTS_DIR
