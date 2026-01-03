# Compiler and Flags
CC = gcc

NOTIFY_FLAGS := $(shell pkg-config --cflags libnotify 2>/dev/null)
NOTIFY_LIBS   := $(shell pkg-config --libs libnotify 2>/dev/null)

# Fallback/Debug: If pkg-config fails, try to set minimal defaults or warn
ifeq ($(NOTIFY_LIBS),)
    $(warning "⚠️  pkg-config could not find libnotify. Ensure libnotify-dev is installed.")
    # Attempt to force flags if you know it's installed but pkg-config is broken:
    # NOTIFY_CFLAGS = -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
    # NOTIFY_LIBS   = -lnotify -lgobject-2.0 -lglib-2.0
endif

CFLAGS = -Wall -Wextra -Werror -Iinclude -g -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE $(NOTIFY_FLAGS)

# Standard Dynamic Linking (for development/local use)
LDFLAGS = -lncurses -lssl -lcrypto -lncurses -lpthread $(NOTIFY_LIBS)
SERVER_LDFLAGS = -lsqlite3 -lcrypt -lssl -lcrypto

# --- STATIC BUILD CONFIGURATION ---
# Flags for creating a standalone binary
# -static: Forces static linking
# -ltinfo: Often required by ncurses when linking statically
# -lpthread: Explicitly required for static threading
# -ldl: Sometimes needed for system calls
STATIC_LDFLAGS = -static -lncurses -ltinfo -lssl -lcrypto -lpthread -lc $(NOTIFY_LIBS)

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
LOG_DIR = log
DATA_DIR = data
DIST_DIR= dist

# Source Discovery
SRCS_COMMON = $(wildcard $(SRC_DIR)/common/*.c)
SRCS_CLIENT = $(wildcard $(SRC_DIR)/client/*.c) $(wildcard $(SRC_DIR)/client/services/*.c)
SRCS_SERVER = $(wildcard $(SRC_DIR)/server/*.c) $(wildcard $(SRC_DIR)/server/handlers/*.c)

# Object Generation
OBJS_COMMON = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS_COMMON))
OBJS_CLIENT = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS_CLIENT))
OBJS_SERVER = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS_SERVER))

# Target Binaries
TARGET_CLIENT = $(BIN_DIR)/client
TARGET_CLIENT_STATIC = $(BIN_DIR)/client_linux_amd64
TARGET_SERVER = $(BIN_DIR)/server

# Default Rule
all: directories certs $(TARGET_CLIENT) $(TARGET_SERVER)

# Ensure certificate header exists before compiling client objects
$(OBJS_CLIENT): include/server_cert.h

include/server_cert.h: server.crt
	@mkdir -p scripts
	@chmod +x scripts/embed_cert.sh
	@./scripts/embed_cert.sh

# Ensure certs exist (wrapper for gencert)
certs:
	@if [ ! -f server.crt ]; then scripts/gencert.sh; fi

# --- LINKING TARGETS ---

# Link Standard Client (Dynamic)
$(TARGET_CLIENT): $(OBJS_CLIENT) $(OBJS_COMMON)
	@echo "Linking Client (Dynamic)..."
	$(CC) $^ -o $@ $(LDFLAGS)

# Link Static Client (Standalone for Distribution)
# Usage: make static-client
$(TARGET_CLIENT_STATIC): $(OBJS_CLIENT) $(OBJS_COMMON)
	@echo "Linking Client (Static)..."
	$(CC) $^ -o $@ $(STATIC_LDFLAGS)

# Link Server
$(TARGET_SERVER): $(OBJS_SERVER) $(OBJS_COMMON)
	@echo "Linking Server..."
	$(CC) $^ -o $@ $(SERVER_LDFLAGS)

# Compile Source Files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR) $(LOG_DIR)

# Add static-client to phony targets so 'make static-client' always checks dependencies
static-client: directories $(TARGET_CLIENT_STATIC)

clean:
	@echo "Cleaning..."
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LOG_DIR) ${DIST_DIR}

.PHONY: all clean directories static-client