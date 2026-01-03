# 📦 MoT Client - Installation Guide

We provide a pre-compiled static binary for Linux (64-bit) that works out of the box.

## 🚀 Quick Start (Official Server)

If you want to connect to the official MoT server, follow these standard steps. This will automatically configure the client for the official environment.

1.  **Download the release archive** (`mot-client-{version}.tar.gz`) from GitHub.
2.  **Extract it**:
    ```bash
    tar -xzf mot-client-{version}.tar.gz
    cd mot-client-{version}
    ```
3.  **Run the installer**:
    ```bash
    ./install.sh
    ```
4.  **Run the App**:
    You can now type `mot` from any terminal!
    ```bash
    mot
    ```

---

## ⚙️ Custom Setup (Private Servers)

If you are hosting your own server or want to connect to a different private instance, you have two ways to configure the client.

### Option A: Install & Edit (Recommended)
This method installs the binary globally but allows you to override the server details.

1.  Follow the **Quick Start** steps above to install the client.
2.  Edit the configuration file created by the installer:
    ```bash
    nano ~/.mot/config.conf
    ```
3.  Change the `server_host` and `server_port` values:
    ```ini
    server_host=192.168.1.50   # Replace with your server IP/Domain
    server_port=8010
    
    # If the server uses a custom SSL certificate (not the official one), 
    # you must specify it here, or the connection will be rejected.
    ca_path=/home/user/my-server.crt
    ```
4.  Run `mot` again.

### Option B: The "Twist" (Manual / Portable Mode)
If you do **not** run the `install.sh` script, the client behaves differently!

1.  Extract the archive but **do not** run `install.sh`.
2.  Run the binary directly from the `bin` folder:
    ```bash
    ./bin/client
    ```
3.  **The Twist:** Since the installer didn't create a config file, the client detects this is a "fresh" run and launches the **First Run Wizard** directly in the terminal.
4.  You can then interactively type your custom Server Host and Port.
5.  The client will verify the settings and save them to `~/.mot/config.conf` for you.

---

## 🗑️ Uninstall

To remove the application, configuration, and logs:

1.  Run the uninstaller script (located in the installation directory):
    ```bash
    ~/.mot/uninstall.sh
    ```
    *(Note: You may need to remove the `export PATH` line from your `~/.bashrc` manually if you added it).*

2.  **Manual Removal:**
    ```bash
    rm -rf ~/.mot
    ```