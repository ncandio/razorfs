import subprocess
import os
import datetime
import shutil

def run_command(command, description, cwd=None, check_return=True):
    print(f"\n--- {description} ---")
    print(f"Executing: {command}")
    process = subprocess.Popen(command, shell=True, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    
    stdout_lines = []
    stderr_lines = []

    # Stream output for transparency
    while True:
        stdout_line = process.stdout.readline()
        stderr_line = process.stderr.readline()
        if stdout_line:
            print(f"STDOUT: {stdout_line.strip()}")
            stdout_lines.append(stdout_line)
        if stderr_line:
            print(f"STDERR: {stderr_line.strip()}")
            stderr_lines.append(stderr_line)
        if not stdout_line and not stderr_line and process.poll() is not None:
            break

    return_code = process.wait()

    if check_return and return_code != 0:
        print(f"ERROR: Command failed with exit code {return_code}")
        print("Please review the output above for details.")
        exit(1)
    return return_code, "".join(stdout_lines), "".join(stderr_lines)

def main():
    print("--- RAZORFS Automation Script ---")
    print("This script automates the benchmark, graph generation, README update, and syncing process.")

    project_root = os.getcwd()
    windows_benchmark_output_dir = "/mnt/c/Users/liber/Desktop/Testing-Razor-FS/benchmarks"
    readme_graphs_dir = os.path.join(project_root, "readme_graphs")
    readme_file = os.path.join(project_root, "README.md")
    versioned_results_base = os.path.join(project_root, "benchmarks", "versioned_results")

    # 1. Get Latest Git Commit Info
    print("\n--- Step 1: Getting latest Git commit information ---")
    git_command = "git log -n 1 --pretty=format:\"%H %ad\""
    return_code, stdout, stderr = run_command(git_command, "Fetching Git commit details")
    
    commit_info = stdout.strip().split(' ', 1)
    commit_sha = commit_info[0]
    commit_date = commit_info[1]
    print(f"Latest Commit SHA: {commit_sha}")
    print(f"Commit Date: {commit_date}")

    # 2. Run Filesystem Benchmarks
    print("\n--- Step 2: Running Filesystem Benchmarks ---")
    benchmark_script = os.path.join(project_root, "docker_test_infrastructure", "benchmark_filesystems.sh")
    run_command(benchmark_script, "Executing benchmark suite (this may take several minutes)")

    # 3. Generate Enhanced Graphs
    print("\n--- Step 3: Generating Enhanced Graphs ---")
    generate_graphs_script = os.path.join(project_root, "docker_test_infrastructure", "generate_enhanced_graphs.sh")
    # The script itself was modified to use an absolute path for RESULTS_DIR, so running it directly should be fine.
    run_command(generate_graphs_script, "Generating publication-quality graphs")

    # 4. Update README.md Graphs
    print("\n--- Step 4: Updating README.md with new graphs ---")
    # Copy newly generated PNGs from Windows output to readme_graphs/
    source_graphs_pattern = os.path.join(windows_benchmark_output_dir, "graphs", "*.png")
    copy_command = f"cp {source_graphs_pattern} {readme_graphs_dir}/"
    run_command(copy_command, f"Copying PNGs from {source_graphs_pattern} to {readme_graphs_dir}")
    print("README.md image links now point to the newly copied graphs.")

    # 5. Create Versioned Benchmark Archive
    print("\n--- Step 5: Archiving benchmark results to a versioned folder ---")
    tag_name = f"{commit_sha}_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}"
    versioned_dir_path = os.path.join(versioned_results_base, tag_name)
    
    os.makedirs(versioned_dir_path, exist_ok=True)
    print(f"Created versioned directory: {versioned_dir_path}")

    # Copy contents of the Windows benchmark output to the versioned folder
    # Use shutil.copytree for directories, but need to handle existing dest
    for item in os.listdir(windows_benchmark_output_dir):
        s = os.path.join(windows_benchmark_output_dir, item)
        d = os.path.join(versioned_dir_path, item)
        if os.path.isdir(s):
            shutil.copytree(s, d, dirs_exist_ok=True)
        else:
            shutil.copy2(s, d)
    print(f"Copied all benchmark output from {windows_benchmark_output_dir} to {versioned_dir_path}")

    # 6. Sync to Windows
    print("\n--- Step 6: Syncing all changes to Windows ---")
    sync_script = os.path.join(project_root, "testing", "sync-to-windows.sh")
    run_command(sync_script, "Executing sync to Windows script")

    print("\n--- RAZORFS Automation Script Completed ---")

if __name__ == "__main__":
    main()
