# 📨 MoT - Messenger of Things

**MoT** is a secure, lightweight, high-performance terminal-based messaging application written in C. It features a robust central server using `epoll` for concurrency and a TUI (Text User Interface) client built with `ncurses`.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Language](https://img.shields.io/badge/language-C-orange.svg)
![Security](https://img.shields.io/badge/security-SSL%2FTLS-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)

---

## Features

### Security
* **SSL/TLS Encryption**: All communication between client and server is encrypted using OpenSSL.
* **Certificate Pinning**: The client embeds the server's certificate at compile-time to prevent Man-in-the-Middle (MITM) attacks.
* **Database Encryption**: Messages stored on the server are encrypted at rest using **AES-256**.
* **Secure Passwords**: Passwords are hashed using SHA-512 with per-user random salts.

### Server
* **High Concurrency**: Uses Linux `epoll` to handle multiple simultaneous client connections efficiently.
* **Data Persistence**: SQLite database (`messagerie.db`) with automatic daily backups.
* **Dockerized**: Multi-stage Docker build (GCC builder -> Alpine/Debian Slim runtime) with dynamic configuration via Entrypoint.
* **Robust Logging**: Rotated daily logs with timestamping.

### Client (TUI)
* **Text User Interface**: Navigable via keyboard (Arrow keys, Enter, Esc) using `ncurses`.
* **Smart Configuration**: Auto-detects environment (Official vs Custom server) via the installer.
* **Features**:
    * **Friends System**: Add via 7-char Friend Codes, Request management.
    * **Messaging**: Private DMs and Group Chats (with Admin tools: Kick/Delete).
    * **Offline Mode**: View cached conversation history even when the server is down.

---

## Project Structure

```text
messagerie_c/
├── bin/                 # Compiled binaries (server, client)
├── dist/                # Generated release packages (tar.gz)
├── doc/                 # Documentation
│   ├── README_CLIENT.md # User guide
│   └── README_SERVER.md # Admin guide
├── include/             # Header files
│   ├── protocol.h       # Shared protocol definitions
│   ├── server_cert.h    # Embedded certificate (Generated)
│   └── system/          # Crypto, Logger, Config definitions
├── scripts/             # DevOps & Automation
│   ├── docker-entrypoint.sh # Server startup config generator
│   ├── embed_cert.sh    # Injects .crt into C header
│   ├── gencert.sh       # Generates self-signed certs
│   ├── install.sh       # Client installer
│   ├── uninstall.sh       # Client uninstaller
│   └── setup_release.sh # Automated release builder
├── src/                 # Source code
│   ├── client/          # TUI, Network, Session
│   ├── server/          # Epoll, Storage, Handlers
│   └── common/          # Crypto, Logger, Protocol
├── .env.template        # Template for server configuration
├── Dockerfile           # Multi-stage server build
├── docker-compose.yml   # Deployment config
└── Makefile             # Build system
```

----

## Getting Started
### 1. For Users (The Client)

We provide a pre-compiled static binary.
Download the latest release (mot-client-vX.X.tar.gz).

- Install:
```bash
./install.sh
```

The installer automatically configures the client to connect to the official server.

- Run:
```bash
mot
```

👉 Read the [Full Client Guide](https://github.com/arthur-lonfils-vinci/MoT/blob/main/doc/README_CLIENT.md) for custom server connections and details.

### 2. For Admins (The Server)

The server runs via Docker and is configured using environment variables.

- Setup Configuration:
```bash
cp .env.template .env
# Edit .env to set DB_KEY, CERT_FILE, and OFFICIAL_PORT
```

- Run with Docker:
```bash
docker-compose up -d --build
```

👉 Read the [Full Server Guide](https://github.com/arthur-lonfils-vinci/MoT/blob/main/doc/README_SERVER.md) for certificate management and deployment details.

----

## Development & Building

If you want to contribute or build from source:
Prerequisites

- GCC, Make

- OpenSSL Development Libraries (libssl-dev)

- NCurses Development Libraries (libncurses-dev)

- SQLite3 Development Libraries (libsqlite3-dev)

Build Commands
```bash
# 1. Generate Development Certificates (if you don't have them)
./scripts/gencert.sh

# 2. Build Server
make server

# 3. Build Client (Automatically embeds the cert)
make static-client
```

### Creating a Release

To create a distributable package (like the ones on GitHub):
```bash
# Usage: ./scripts/setup_release.sh <version_tag>
./scripts/setup_release.sh v0.6.3
```

This script compiles the binary, embeds the current certificate, injects the .env configuration into the installer, and zips everything into dist/.

-----

## Protocol & Encryption

- Transport: TCP Stream wrapped in SSL/TLS.

- Format: Binary Protocol (Header + Payload).

	- Header: Type (1 byte) + Length (4 bytes).

  - Payload: Packed C Structs.

- Storage: Messages are encrypted via AES-256 before insertion into SQLite and decrypted only upon retrieval.

----

## 🤝 Contributing

We welcome contributions!

1. Fork the repository.

2. Create a feature branch (`git checkout -b feature/AmazingFeature`).

3. Commit your changes.

4. Push to the branch.

5. Open a Pull Request.

Developed by Arthur