# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -g

# Standard Dynamic Linking (for development/local use)
LDFLAGS = -lncurses
SERVER_LDFLAGS = -lsqlite3 -lcrypt

# --- STATIC BUILD CONFIGURATION ---
# Flags for creating a standalone binary
# -static: Forces static linking
# -ltinfo: Often required by ncurses when linking statically
# -lpthread: Explicitly required for static threading
# -ldl: Sometimes needed for system calls
STATIC_LDFLAGS = -static -lncurses -ltinfo -lpthread -lc

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
LOG_DIR = log
BACKUPS_DIR = backups
DATA_DIR = data

# Source Discovery
SRCS_COMMON = $(wildcard $(SRC_DIR)/common/*.c)
SRCS_CLIENT = $(wildcard $(SRC_DIR)/client/*.c)
SRCS_SERVER = $(wildcard $(SRC_DIR)/server/*.c)

# Object Generation
OBJS_COMMON = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS_COMMON))
OBJS_CLIENT = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS_CLIENT))
OBJS_SERVER = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS_SERVER))

# Target Binaries
TARGET_CLIENT = $(BIN_DIR)/client
TARGET_CLIENT_STATIC = $(BIN_DIR)/client_linux_amd64
TARGET_SERVER = $(BIN_DIR)/server

# Default Rule
all: directories $(TARGET_CLIENT) $(TARGET_SERVER)

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
	@mkdir -p $(OBJ_DIR) $(BIN_DIR) $(LOG_DIR) $(DATA_DIR)/backups

# Add static-client to phony targets so 'make static-client' always checks dependencies
static-client: directories $(TARGET_CLIENT_STATIC)

clean:
	@echo "Cleaning..."
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LOG_DIR)

.PHONY: all clean directories static-client