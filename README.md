# 📨 MoT - Messenger of Things

**MoT** is a lightweight, high-performance terminal-based messaging application written in C. It features a robust central server using `epoll` for concurrency and a TUI (Text User Interface) client built with `ncurses`.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Language](https://img.shields.io/badge/language-C-orange.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)

---

## ✨ Features

### 🖥️ Server
* **High Concurrency**: Uses Linux `epoll` to handle multiple simultaneous client connections efficiently.
* **Data Persistence**: Stores users, conversations, and messages in a **SQLite** database (`messagerie.db`).
* **Automatic Backups**: Creates daily backups of the database in `data/backups/` upon startup.
* **Robust Logging**: Rotated daily logs stored in `log/` with timestamping and log levels.
* **Docker Ready**: Fully containerized with `docker-compose` for easy deployment.

### 📱 Client (TUI)
* **Text User Interface**: Navigable via keyboard (Arrow keys, Enter, Esc), utilizing `ncurses` for a retro yet functional look.
* **Authentication**: Secure Registration and Login system.
* **Friends System**:
    * Add friends via unique **Friend Codes** (7 characters).
    * Accept/Deny friend requests.
    * Real-time status updates.
* **Messaging**:
    * **Private DMs**: Automatically created upon accepting a friend request.
    * **Group Chats**: Create groups, set names/descriptions, and manage members.
    * **Admin Tools**: Group creators can kick members or delete the group.
* **Persisted Session**: "Remember Me" functionality to stay logged in.
* **System Integration**: Custom `mot` command wrapper for easy access from the terminal.

---

## 🏗️ Project Structure

```text
messagerie_c/
├── bin/                 # Compiled binaries (server, client)
├── data/                # Database and Backups (Server only)
│   ├── messagerie.db
│   └── backups/
├── include/             # Header files (.h)
│   ├── protocol.h       # Shared protocol definitions
│   └── ...
├── log/                 # Application logs (Client & Server)
├── src/                 # Source code
│   ├── client/          # Client-specific logic (UI, Network, Input)
│   ├── server/          # Server-specific logic (Epoll, Storage)
│   └── common/          # Shared utilities (Logger, Packet handling)
├── Dockerfile           # Server container definition
├── docker-compose.yml   # Server deployment config
├── Makefile             # Build system
├── install.sh           # Client installer script
└── README.md            # This file
```

## 📡 Protocol

The application uses a custom binary protocol defined in include/protocol.h.

    Header: Fixed size (Type + Payload Length).

    Payload: Packed Structs (e.g., LoginPayload, SendMessagePayload).

    Flow: TCP (Stream).

Key Message Types


## 🔧 Deployment
Server (Docker)

The server is designed to run in a Docker container with persistent storage for the database and logs.

    Port: 85 (Configurable in protocol.h and docker-compose.yml).

    Volumes: Maps ./data and ./log to the host machine for persistence.

Client (User)

Users can install the client using the provided script, which sets up a dedicated environment in ~/.mot/ to keep logs and session files organized.


## 📝 Logging & Data
Logs

Logs are automatically rotated and stored in the log/ directory.

    Format: [HH:MM:SS] [LEVEL] [File:Line] Message

    Server Log: log/server_YYYY-MM-DD.log

    Client Log: log/client_u{UID}_YYYY-MM-DD.log

Database

    Engine: SQLite3

    Location: data/messagerie.db

    Schema: Stores users, contacts, requests, conversations, participants, and messages.


## 🤝 Contributing

We welcome contributions! Please follow these steps:

    Fork the repository.

    Create a feature branch (git checkout -b feature/NewThing).

    Commit your changes.

    Push to the branch.

    Open a Pull Request.

Developed by Arthur