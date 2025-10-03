# RAZOR Filesystem Setup Verification Script
# Verifies that the Docker setup is working correctly

function Write-Header($message) {
    Write-Host "`n=== $message ===" -ForegroundColor Blue
}

function Write-Success($message) {
    Write-Host "✓ $message" -ForegroundColor Green
}

function Write-Error($message) {
    Write-Host "✗ $message" -ForegroundColor Red
}

function Test-Files {
    Write-Header "Checking Required Files"
    
    $requiredFiles = @(
        "Dockerfile",
        "docker-compose.yml", 
        "docker-test.ps1",
        "docker-test.bat",
        "README-Windows.md"
    )
    
    $allFound = $true
    foreach ($file in $requiredFiles) {
        if (Test-Path $file) {
            Write-Success "Found: $file"
        } else {
            Write-Error "Missing: $file"
            $allFound = $false
        }
    }
    
    return $allFound
}

function Test-ParentFiles {
    Write-Header "Checking Parent Directory Structure"
    
    $parentFiles = @(
        "../fuse/Makefile",
        "../fuse/razorfs_fuse.cpp",
        "../kernel/Makefile", 
        "../kernel/razorfs_kernel.c",
        "../src/"
    )
    
    $allFound = $true
    foreach ($file in $parentFiles) {
        if (Test-Path $file) {
            Write-Success "Found: $file"
        } else {
            Write-Error "Missing: $file"
            $allFound = $false
        }
    }
    
    return $allFound
}

function Test-DockerConnection {
    Write-Header "Testing Docker Connection"
    
    try {
        docker info | Out-Null
        Write-Success "Docker daemon is accessible"
        
        $dockerVersion = docker --version
        Write-Success "Docker version: $dockerVersion"
        
        $composeVersion = docker-compose --version  
        Write-Success "Docker Compose version: $composeVersion"
        
        return $true
    } catch {
        Write-Error "Docker connection failed: $($_.Exception.Message)"
        return $false
    }
}

function Test-Syntax {
    Write-Header "Checking Configuration Syntax"
    
    try {
        # Test docker-compose syntax
        docker-compose config | Out-Null
        Write-Success "docker-compose.yml syntax is valid"
        
        # Test PowerShell script syntax
        $null = Get-Command ".\docker-test.ps1" -ErrorAction Stop
        Write-Success "docker-test.ps1 syntax is valid"
        
        return $true
    } catch {
        Write-Error "Configuration syntax error: $($_.Exception.Message)"
        return $false
    }
}

# Main verification
Write-Host "RAZOR Filesystem Docker Setup Verification" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green

$filesOk = Test-Files
$parentOk = Test-ParentFiles  
$dockerOk = Test-DockerConnection
$syntaxOk = Test-Syntax

Write-Header "Verification Summary"

if ($filesOk -and $parentOk -and $dockerOk -and $syntaxOk) {
    Write-Success "All checks passed! Setup is ready for use."
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Yellow
    Write-Host "1. Build images: .\docker-test.ps1 build" -ForegroundColor Cyan
    Write-Host "2. Test FUSE:    .\docker-test.ps1 test-fuse" -ForegroundColor Cyan
    Write-Host "3. Start dev:    .\docker-test.ps1 dev" -ForegroundColor Cyan
} else {
    Write-Error "Some checks failed. Please resolve the issues above."
    
    if (-not $filesOk) {
        Write-Host "Missing files detected. Ensure you're in the correct directory." -ForegroundColor Yellow
    }
    
    if (-not $parentOk) {
        Write-Host "Parent directory structure issues. Ensure razorfs source is available." -ForegroundColor Yellow
    }
    
    if (-not $dockerOk) {
        Write-Host "Docker issues detected. Ensure Docker Desktop is running." -ForegroundColor Yellow
    }
    
    if (-not $syntaxOk) {
        Write-Host "Configuration syntax issues detected." -ForegroundColor Yellow
    }
    
    exit 1
}