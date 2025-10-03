@echo off
REM Quick test for simple graphs
echo Testing simple graph generation...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress simple-graphs