# RAZOR Filesystem Docker Setup Script for Windows
# Run this first to set up the testing environment

param(
    [switch]$SkipChecks
)

function Write-Header($message) {
    Write-Host "`n=== $message ===" -ForegroundColor Blue
    Write-Host ""
}

function Write-Success($message) {
    Write-Host "✓ $message" -ForegroundColor Green
}

function Write-Warning($message) {
    Write-Host "⚠ $message" -ForegroundColor Yellow
}

function Write-Error($message) {
    Write-Host "✗ $message" -ForegroundColor Red
}

function Test-Prerequisites {
    Write-Header "Checking Prerequisites"
    
    $allGood = $true
    
    # Check PowerShell version
    $psVersion = $PSVersionTable.PSVersion
    if ($psVersion.Major -ge 5) {
        Write-Success "PowerShell $($psVersion.Major).$($psVersion.Minor) found"
    } else {
        Write-Error "PowerShell 5.0 or later required, found $($psVersion.Major).$($psVersion.Minor)"
        $allGood = $false
    }
    
    # Check Docker
    try {
        $dockerVersion = docker --version
        Write-Success "Docker found: $dockerVersion"
    } catch {
        Write-Error "Docker not found. Please install Docker Desktop for Windows"
        Write-Host "Download from: https://www.docker.com/products/docker-desktop" -ForegroundColor Cyan
        $allGood = $false
    }
    
    # Check docker-compose
    try {
        $composeVersion = docker-compose --version
        Write-Success "Docker Compose found: $composeVersion"
    } catch {
        Write-Error "Docker Compose not found. Please install Docker Desktop for Windows"
        $allGood = $false
    }
    
    # Check Docker daemon
    try {
        docker info | Out-Null
        Write-Success "Docker daemon is running"
    } catch {
        Write-Error "Docker daemon is not running. Please start Docker Desktop"
        $allGood = $false
    }
    
    # Check WSL2
    try {
        $wslVersion = wsl --status
        if ($wslVersion -like "*WSL 2*" -or $wslVersion -like "*Default Version: 2*") {
            Write-Success "WSL2 is available"
        } else {
            Write-Warning "WSL2 may not be the default. Consider enabling WSL2 backend in Docker Desktop"
        }
    } catch {
        Write-Warning "WSL not found or not configured. Docker will use Hyper-V backend"
    }
    
    return $allGood
}

function Show-NextSteps {
    Write-Header "Setup Complete - Next Steps"
    
    Write-Host "1. Build Docker images:" -ForegroundColor Yellow
    Write-Host "   .\docker-test.ps1 build" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "2. Test FUSE implementation (safe):" -ForegroundColor Yellow
    Write-Host "   .\docker-test.ps1 test-fuse" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "3. Start development environment:" -ForegroundColor Yellow
    Write-Host "   .\docker-test.ps1 dev" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "4. See all available commands:" -ForegroundColor Yellow
    Write-Host "   .\docker-test.ps1 help" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "Documentation:" -ForegroundColor Yellow
    Write-Host "   README-Windows.md - Complete Windows guide" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Warning "Important: Kernel module testing requires privileged containers and should"
    Write-Warning "only be used on test systems. Start with FUSE testing which is safe."
}

function Test-DockerBuild {
    Write-Header "Testing Docker Build"
    
    try {
        Write-Host "Building RAZOR filesystem Docker images..."
        docker-compose build --no-cache
        Write-Success "Docker build completed successfully"
        return $true
    } catch {
        Write-Error "Docker build failed: $($_.Exception.Message)"
        return $false
    }
}

# Main execution
Write-Host "RAZOR Filesystem Docker Setup for Windows" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green

if (-not $SkipChecks) {
    if (-not (Test-Prerequisites)) {
        Write-Error "Prerequisites check failed. Please install missing components and run again."
        exit 1
    }
    
    Write-Host ""
    $response = Read-Host "Prerequisites look good. Build Docker images now? (Y/n)"
    if ($response -eq "" -or $response -eq "Y" -or $response -eq "y") {
        if (-not (Test-DockerBuild)) {
            Write-Error "Docker build failed. Check the error messages above."
            exit 1
        }
    }
}

Show-NextSteps

Write-Success "Setup complete! You can now test the RAZOR filesystem in Docker."