@echo off
REM Test metadata graph generation
echo Generating RAZORFS metadata performance graphs...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress python3 /razorfs/razorfs_windows_testing/metadata_graph_generator.py