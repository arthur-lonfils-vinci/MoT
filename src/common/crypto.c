#include "system/crypto.h"
#include "system/logger.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// 32 bytes for AES-256 key
static unsigned char g_master_key[32];
static int g_initialized = 0;

void crypto_init(const char *key_str) {
    if (!key_str || strlen(key_str) == 0) {
        log_print(LOG_WARN, "Crypto: No key provided! Encryption will fail.");
        return;
    }
    // Hash the input passphrase to ensure a perfect 32-byte key
    SHA256((unsigned char*)key_str, strlen(key_str), g_master_key);
    g_initialized = 1;
}

// Helper: Convert bytes to hex string
static void bytes_to_hex(const unsigned char *bytes, int len, char *hex_out) {
    for (int i = 0; i < len; i++) {
        sprintf(hex_out + (i * 2), "%02x", bytes[i]);
    }
    hex_out[len * 2] = '\0';
}

// Helper: Convert hex string to bytes
static void hex_to_bytes(const char *hex, unsigned char *bytes) {
    size_t len = strlen(hex);
    for (size_t i = 0; i < len / 2; i++) {
        sscanf(hex + 2 * i, "%2hhx", &bytes[i]);
    }
}

char *crypto_encrypt(const char *plaintext) {
    if (!g_initialized || !plaintext) return NULL;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = strlen(plaintext);
    int c_len, f_len;
    
    // 1. Generate Random IV (16 bytes for AES)
    unsigned char iv[16];
    if (!RAND_bytes(iv, sizeof(iv))) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    // Allocate buffer (Input len + Block size)
    unsigned char *ciphertext = malloc(len + 32); 
    if (!ciphertext) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    
    // 2. Encrypt
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, g_master_key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }

    if (EVP_EncryptUpdate(ctx, ciphertext, &c_len, (unsigned char*)plaintext, len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }

    if (EVP_EncryptFinal_ex(ctx, ciphertext + c_len, &f_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }
    
    EVP_CIPHER_CTX_free(ctx);

    int total_len = c_len + f_len;

    // 3. Format Output: HEX(IV) + HEX(Ciphertext)
    // IV is 16 bytes -> 32 hex chars
    // Ciphertext is total_len -> total_len * 2 hex chars
    // +1 for null terminator
    char *hex_out = malloc(32 + (total_len * 2) + 1);
    if (!hex_out) {
        free(ciphertext);
        return NULL;
    }
    
    bytes_to_hex(iv, 16, hex_out);
    bytes_to_hex(ciphertext, total_len, hex_out + 32);
    
    free(ciphertext);
    return hex_out;
}

char *crypto_decrypt(const char *hex_in) {
    if (!g_initialized || !hex_in) return NULL;
    
    size_t hex_len = strlen(hex_in);
    if (hex_len < 32) return NULL; // Too short to contain IV

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int p_len, f_len;

    // 1. Extract IV
    unsigned char iv[16];
    char iv_hex[33];
    strncpy(iv_hex, hex_in, 32);
    iv_hex[32] = '\0';
    hex_to_bytes(iv_hex, iv);

    // 2. Extract Ciphertext
    int cipher_len = (hex_len - 32) / 2;
    unsigned char *ciphertext = malloc(cipher_len);
    if (!ciphertext) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    hex_to_bytes(hex_in + 32, ciphertext);

    unsigned char *plaintext = malloc(cipher_len + 32);
    if (!plaintext) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }

    // 3. Decrypt
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, g_master_key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        free(plaintext);
        return NULL;
    }

    if (EVP_DecryptUpdate(ctx, plaintext, &p_len, ciphertext, cipher_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        free(plaintext);
        return NULL;
    }

    int ret = EVP_DecryptFinal_ex(ctx, plaintext + p_len, &f_len);
    
    EVP_CIPHER_CTX_free(ctx);
    free(ciphertext);

    if (ret <= 0) {
        // Decryption failed (Wrong key? Corrupted data?)
        free(plaintext);
        // Return a dummy string to indicate error without crashing
        return strdup("[Unreadable Encrypted Message]");
    }

    plaintext[p_len + f_len] = '\0';
    return (char*)plaintext;
}