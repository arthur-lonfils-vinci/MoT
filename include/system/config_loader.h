/**
 * @file config_loader.h
 * @brief Configuration definitions and parser.
 */

#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#define DEFAULT_PORT 8080
#define DEFAULT_HOST "localhost"
#define DEFAULT_DB_PATH "data/messagerie.db"

typedef struct {
    // Shared
    int port;

    // Client Specific
    char server_host[256];
    char ca_cert_path[512]; // Optional: path to custom CA

    // Server Specific
    char db_path[512];
    char server_cert_path[512];
    char server_key_path[512];
		char db_encryption_key[256];
} AppConfig;

/**
 * @brief Loads configuration from a file.
 * Applies defaults if the file is missing or values are empty.
 * @param path Path to the config file.
 * @param config Output structure.
 * @return 1 on success (file found), 0 on failure (defaults applied).
 */
int config_load(const char *path, AppConfig *config);

/**
 * @brief Saves the client configuration to disk.
 * @return 1 on success, 0 on failure.
 */
int config_save_client(const char *path, const AppConfig *config);

/**
 * @brief Helper to get the standard client config path (~/.mot/config.conf).
 * @param buffer Output buffer.
 * @param size Size of buffer.
 */
void config_get_client_path(char *buffer, size_t size);

void apply_defaults(AppConfig *config);

#endif