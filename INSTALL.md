# 📦 Installation Guide - MoT (Messenger of Things)

## 🖥️ Server Setup (For Administrators)

The server runs inside a Docker container.

1.  **Clone the repository** (or download the source):
    ```bash
    git clone [https://github.com/YOUR_USERNAME/messagerie_c.git](https://github.com/YOUR_USERNAME/messagerie_c.git)
    cd messagerie_c
    ```

2.  **Start the Server**:
    ```bash
    docker-compose up -d --build
    ```
    * The server listens on port **8010** (TCP).
    * Data is persisted in `./server_data`.
    * Logs are available in `./server_logs`.

---

## 💻 Client Setup (For Users)

We provide a pre-compiled static binary for Linux (64-bit).

### Option A: The "Easy" Installer (Recommended)

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
    mot                            # Connects to default server
    mot server.example.com         # Connects to a specific server
    ```

### Option B: Manual Installation

If you prefer not to use the installer script:

1.  Download `client_linux_amd64`.
2.  Make it executable:
    ```bash
    chmod +x client_linux_amd64
    ```
3.  Run it directly (ensure you have a folder for logs/sessions):
    ```bash
    ./client_linux_amd64 server.example.com
    ```


## Client Uninstall

Use the `uninstall.sh` script that is in the app directory `~/.mot/uninstall.sh` by default
```bash
./uninstall.sh
```

---

## 🛠️ Building from Source

If you want to modify the code or compile for a different architecture:

1.  **Install Dependencies**:
    * Debian/Ubuntu: `sudo apt install build-essential libncurses-dev`
2.  **Build**:
    ```bash
    make clean && make static-client
    ```
3.  **Run**:
    ```bash
    ./bin/client_linux_amd64
    ```