# diagnostics.ps1
# Run RAZOR filesystem diagnostics from Windows PowerShell

Write-Host "RAZOR Filesystem Operations Diagnostics" -ForegroundColor Green
Write-Host "=====================================" -ForegroundColor Green
Write-Host ""

Write-Host "Running diagnostics..." -ForegroundColor Yellow

# Run the diagnostics in the Docker container
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress diagnostics

Write-Host ""
Write-Host "Copying results to Windows..." -ForegroundColor Yellow

# Copy results from Docker volume to Windows
docker run --rm -v razorfs-stress-results:/results -v ${PWD}/results:/windows-results busybox sh -c "cp /results/razor_operations_diagnostics.csv /windows-results/ 2>/dev/null; cp /results/diagnostics_system_info.txt /windows-results/ 2>/dev/null; echo 'Files copied successfully'"

Write-Host "Diagnostics complete!" -ForegroundColor Green
Write-Host "Results are in the .\results directory:" -ForegroundColor Yellow
Get-ChildItem -Path ".\results" -Filter "*diagnostics*" | Format-Table Name, Length, LastWriteTime