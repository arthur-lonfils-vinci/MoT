# 📦 MoT - Server & Release Management Guide

This documentation covers the administration of the **Messenger of Things (MoT)** server, including secure deployment with Docker and creating official client releases.

---

## 🔐 1. Configuration & Security

Before running the server or building the client, you must configure the environment variables.

1.  **Create your `.env` file**:
    Copy the template to a real `.env` file (which is git-ignored).
    ```bash
    cp .env.template .env
    ```

2.  **Edit `.env`**:
    Open the file and set the following critical variables:

    ```ini
    # Network Configuration
    OFFICIAL_HOST=server-mot.yourdomain.com  # Domain clients will connect to
    OFFICIAL_PORT=8010                       # Port exposed by the server

    # Security (CHANGE THESE!)
    DB_KEY=YourSuperSecretLongRandomString   # Encryption key for the database
    
    # Certificate Files (Local paths)
    CERT_FILE=server.crt
    KEY_FILE=server.key
    ```

### 🔑 SSL Certificates
The system relies on SSL for secure communication and client verification (Certificate Pinning).

* **Production:** Place your valid `server.crt` and `server.key` in the project root.
* **Development/Self-Signed:** Generate them automatically:
    ```bash
    ./scripts/gencert.sh
    ```
    > **⚠️ Important:** The `server.crt` file is **embedded** into the client binary during compilation. If you change the server certificate, you **must** recompile and redistribute the client.

---

## 🖥️ 2. Server Deployment (Docker)

The server is designed to run in a container, auto-configuring itself based on your `.env` file.

### Prerequisites
* Docker & Docker Compose installed.
* Ports `8010` (or your custom port) open on the firewall.

### Start the Server
1.  Ensure your `.env`, `server.crt`, and `server.key` are present.
2.  Run Docker Compose:
    ```bash
    docker-compose up -d --build
    ```

### Management
* **Logs:** View server activity and encryption status.
    ```bash
    docker-compose logs -f
    ```
* **Data:**
    * Database is persisted in: `./server_data/messagerie.db`
    * Logs are stored in: `./server_logs/`
* **Stopping:**
    ```bash
    docker-compose down
    ```

---

## 🛠️ 3. Building Client Releases

As an administrator, you build the "Official Release" which comes pre-configured to connect to your server.

### Why use the release script?
The `setup_release.sh` script performs magic steps you shouldn't skip:
1.  **Embeds the Certificate:** Converts `server.crt` into a C header so the client trusts your server automatically.
2.  **Bakes Configuration:** Injects your `OFFICIAL_HOST` and `OFFICIAL_PORT` from `.env` into the installer script (`install.sh`).
3.  **Packages:** Creates a neat `.tar.gz` ready for distribution.

### Build Steps
1.  Ensure `.env` contains the correct public hostname (`OFFICIAL_HOST`).
2.  Run the builder with a version tag:
    ```bash
    ./scripts/setup_release.sh v0.6.3
    ```

### Output
The script will generate an archive in the `dist/` folder:
* `dist/mot-client-v0.6.3.tar.gz`

Upload this file to GitHub Releases or distribute it to your users.

---

## 🧪 4. Manual Compilation (Development)

If you are developing features and want to compile locally without packaging:

### Prerequisites
* GCC, Make, OpenSSL Dev, Ncurses Dev.
    * *Debian/Ubuntu:* `sudo apt install build-essential libncurses-dev libssl-dev libsqlite3-dev`

### Compilation
The Makefile handles certificate embedding automatically.

```bash
# 1. Ensure certs exist (generates self-signed if missing)
./scripts/gencert.sh

# 2. Build the static client
make clean && make static-client

# 3. Run
./bin/client_linux_amd64