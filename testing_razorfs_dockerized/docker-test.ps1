# RAZOR Filesystem Docker Testing Script for Windows
# PowerShell script for Windows Docker Desktop

param(
    [Parameter(Position=0)]
    [string]$Command = "help"
)

# Color functions
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

function Show-Help {
    Write-Host "RAZOR Filesystem Docker Testing Script for Windows"
    Write-Host ""
    Write-Host "Usage: .\docker-test.ps1 [COMMAND]"
    Write-Host ""
    Write-Host "Commands:"
    Write-Host "  build          Build Docker images"
    Write-Host "  test-fuse      Test FUSE implementation (safe)"
    Write-Host "  test-kernel    Test kernel module (requires Hyper-V)"
    Write-Host "  test-all       Run all tests"
    Write-Host "  dev            Start development container"
    Write-Host "  clean          Clean Docker resources"
    Write-Host "  logs           Show container logs"
    Write-Host "  shell          Start interactive shell in container"
    Write-Host "  help           Show this help"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\docker-test.ps1 build"
    Write-Host "  .\docker-test.ps1 test-fuse"
    Write-Host "  .\docker-test.ps1 dev"
    Write-Host ""
    Write-Host "Requirements:"
    Write-Host "  - Docker Desktop for Windows"
    Write-Host "  - WSL2 backend (recommended)"
    Write-Host "  - PowerShell 5.1 or later"
}

function Test-DockerAvailable {
    try {
        docker --version | Out-Null
        docker-compose --version | Out-Null
        return $true
    }
    catch {
        Write-Error "Docker or docker-compose not found. Please install Docker Desktop."
        return $false
    }
}

function Build-Images {
    Write-Header "Building RAZOR Filesystem Docker Images"
    
    try {
        docker-compose build
        Write-Success "Docker images built successfully"
    }
    catch {
        Write-Error "Failed to build Docker images"
        Write-Host $_.Exception.Message
        exit 1
    }
}

function Test-Fuse {
    Write-Header "Testing FUSE Implementation"
    
    # Create test directory
    $testDir = "$env:TEMP\razorfs-docker-test"
    New-Item -ItemType Directory -Force -Path $testDir | Out-Null
    
    Write-Warning "Testing FUSE implementation (this is safe)"
    try {
        docker-compose run --rm razorfs-fuse
        Write-Success "FUSE test completed"
    }
    catch {
        Write-Error "FUSE test failed"
        Write-Host $_.Exception.Message
        return $false
    }
    finally {
        # Cleanup
        Remove-Item -Recurse -Force $testDir -ErrorAction SilentlyContinue
    }
    
    return $true
}

function Test-Kernel {
    Write-Header "Testing Kernel Module"
    
    Write-Warning "This requires privileged Docker containers!"
    Write-Warning "On Windows, kernel modules run in WSL2 VM, not Windows kernel."
    
    $response = Read-Host "Are you sure you want to continue? (y/N)"
    
    if ($response -ne "y" -and $response -ne "Y") {
        Write-Warning "Kernel module test cancelled"
        return $true
    }
    
    try {
        docker-compose run --rm razorfs-kernel
        Write-Success "Kernel module test completed"
        return $true
    }
    catch {
        Write-Error "Kernel module test failed"
        Write-Host $_.Exception.Message
        return $false
    }
}

function Test-All {
    Write-Header "Running All Tests"
    
    $failed = $false
    
    if (-not (Test-Fuse)) {
        $failed = $true
    }
    
    Write-Host ""
    $response = Read-Host "Run kernel module test? This requires privileged mode (y/N)"
    
    if ($response -eq "y" -or $response -eq "Y") {
        if (-not (Test-Kernel)) {
            $failed = $true
        }
    }
    else {
        Write-Warning "Skipping kernel module test"
    }
    
    if (-not $failed) {
        Write-Success "All tests passed!"
    }
    else {
        Write-Error "Some tests failed"
        exit 1
    }
}

function Start-Dev {
    Write-Header "Starting Development Container"
    
    Write-Warning "Starting interactive development environment"
    docker-compose run --rm razorfs-dev
}

function Clean-Resources {
    Write-Header "Cleaning Docker Resources"
    
    Write-Host "Stopping containers..."
    docker-compose down --remove-orphans
    
    Write-Host "Removing images..."
    docker-compose down --rmi all --volumes --remove-orphans
    
    Write-Success "Cleanup completed"
}

function Show-Logs {
    Write-Header "Container Logs"
    docker-compose logs
}

function Start-Shell {
    Write-Header "Starting Interactive Shell"
    docker-compose run --rm razorfs-dev shell
}

# Main script execution
if (-not (Test-DockerAvailable)) {
    exit 1
}

switch ($Command.ToLower()) {
    "build" { Build-Images }
    "test-fuse" { Test-Fuse }
    "test-kernel" { Test-Kernel }
    "test-all" { Test-All }
    "dev" { Start-Dev }
    "clean" { Clean-Resources }
    "logs" { Show-Logs }
    "shell" { Start-Shell }
    "help" { Show-Help }
    default {
        Write-Error "Unknown command: $Command"
        Write-Host ""
        Show-Help
        exit 1
    }
}