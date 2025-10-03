@echo off
echo ========================================
echo RAZORFS O(log n) Complexity Test Suite
echo ========================================
echo.
echo Running quick O(log n) complexity test...
echo This will test filesystem performance with up to 10,000 files
echo.

docker run --rm -v razorfs-stress-results:/results --cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined razorfs_windows_testing-razorfs-stress bash -c "cd /razorfs && chmod +x ./razorfs_windows_testing/ologn_complexity_test.sh && ./razorfs_windows_testing/ologn_complexity_test.sh quick"

echo.
echo Test completed. Run analyze-ologn-results.bat to generate graphs.