# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -g
LDFLAGS = -lncurses
# Server needs sqlite3 and crypt
SERVER_LDFLAGS = -lsqlite3 -lcrypt

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
LOG_DIR = log
BACKUPS_DIR = backups

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
TARGET_SERVER = $(BIN_DIR)/server

# Default Rule
all: directories $(TARGET_CLIENT) $(TARGET_SERVER)

# Link Client
$(TARGET_CLIENT): $(OBJS_CLIENT) $(OBJS_COMMON)
	@echo "Linking Client..."
	$(CC) $^ -o $@ $(LDFLAGS)

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
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

clean:
	@echo "Cleaning..."
	rm -rf $(OBJ_DIR) $(BIN_DIR) ${LOG_DIR} ${BACKUPS_DIR}

.PHONY: all clean directories