# Docker Desktop with WSL2 Integration Setup Guide

This guide will help you set up Docker Desktop with WSL2 integration to resolve the "Docker command not found" error and enable you to build Docker images successfully.

## Prerequisites

1. Windows 10 version 2004 or higher (Build 19041 or higher) or Windows 11
2. WSL2 installed and configured
3. A Linux distribution installed in WSL2 (like Ubuntu)

## Step 1: Enable WSL2 on Windows

If not already done, enable WSL2 on your Windows system:

1. Open PowerShell as Administrator and run:
   ```powershell
   wsl --install
   ```

2. If WSL is already installed, ensure you're using WSL2:
   ```powershell
   wsl --set-default-version 2
   ```

3. Verify your Linux distribution is using WSL2:
   ```powershell
   wsl --list --verbose
   ```

## Step 2: Install Docker Desktop for Windows

1. Download Docker Desktop for Windows from [Docker's official website](https://www.docker.com/products/docker-desktop)
2. Run the installer and follow the installation wizard
3. During installation, make sure to select the option to enable WSL 2 based engine (this should be default in newer versions)

## Step 3: Enable WSL2 Integration in Docker Desktop

1. Launch Docker Desktop
2. Go to Settings (gear icon in top right)
3. Select "General" and ensure "Use the WSL 2 based engine" is checked
4. Go to "Resources" → "WSL Integration"
5. Enable integration with your default WSL 2 distributions
6. Optionally, enable integration with additional distributions if you have multiple

## Step 4: Install Docker CLI in Your WSL2 Distribution

In your WSL2 terminal (e.g., Ubuntu):

1. Update your package index:
   ```bash
   sudo apt update
   ```

2. Install Docker CLI (not the full Docker engine, as that's handled by Docker Desktop):
   ```bash
   sudo apt install docker.io
   ```

Alternatively, you can install just the Docker client:
```bash
sudo apt install apt-transport-https ca-certificates curl gnupg lsb-release
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt update
sudo apt install docker-ce-cli
```

## Step 5: Configure User Permissions

1. Add your user to the docker group:
   ```bash
   sudo usermod -aG docker $USER
   ```

2. Log out and log back in to your WSL2 session for the group changes to take effect

## Step 6: Configure Docker Context

Docker Desktop automatically configures a context for WSL2 integration. You can verify this by running:

```bash
docker context ls
```

You should see a "default" context that points to the Docker Desktop engine.

If the context is not properly configured, you can manually create it:
```bash
docker context create desktop-windows --docker host=tcp://localhost:2375
docker context use desktop-windows
```

## Step 7: Verify Installation

1. Restart Docker Desktop if needed
2. In your WSL2 terminal, run:
   ```bash
   docker --version
   ```

3. Test Docker functionality:
   ```bash
   docker run hello-world
   ```

## Step 8: Build Your Docker Image

Once everything is set up, you should be able to build Docker images from your WSL2 terminal:

```bash
cd /home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing
docker build -f Dockerfile.optimized -t razorfs-optimized-test .
```

## Troubleshooting Common Issues

### Issue 1: Cannot connect to the Docker daemon

If you get an error like "Cannot connect to the Docker daemon at unix:///var/run/docker.sock", try:

1. Ensure Docker Desktop is running in Windows
2. Restart Docker Desktop
3. In WSL2, run:
   ```bash
   sudo service docker start
   ```

### Issue 2: Docker command still not found

If the docker command is still not found:

1. Check if the Docker CLI is properly installed:
   ```bash
   which docker
   ```

2. If not found, add Docker to your PATH by adding this to your `~/.bashrc`:
   ```bash
   export PATH="$PATH:/usr/bin"
   ```
   Then reload your shell:
   ```bash
   source ~/.bashrc
   ```

### Issue 3: Permission denied errors

If you get permission denied errors when running Docker commands:

1. Make sure you've added your user to the docker group:
   ```bash
   sudo usermod -aG docker $USER
   ```

2. Log out and back in to your WSL2 session

### Issue 4: Docker Desktop not recognizing WSL2 distro

If Docker Desktop doesn't recognize your WSL2 distro:

1. Make sure the distro is enabled in Docker Desktop settings under "Resources" → "WSL Integration"
2. Try restarting both Docker Desktop and WSL2:
   ```bash
   wsl --shutdown
   ```
   Then restart Docker Desktop

## Testing Your Setup

After following these steps, you should be able to:

1. Run Docker commands in your WSL2 terminal
2. Build Docker images successfully
3. Run containers using Docker Desktop's WSL2 backend

This setup allows you to use Docker CLI commands directly in your WSL2 terminal while the actual Docker engine runs in Docker Desktop on Windows, providing the best of both worlds.