/**
 * @file crypto.h
 * @brief AES-256 Encryption/Decryption helpers for database fields.
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>

/**
 * @brief Initializes the encryption system (sets the master key).
 * Must be called once at startup.
 * @param key_str A string passphrase (will be hashed to create the actual 32-byte key).
 */
void crypto_init(const char *key_str);

/**
 * @brief Encrypts a plaintext string into a hex-encoded ciphertext.
 * format: [IV (32 hex)] + [Ciphertext (hex)]
 * @param plaintext The text to encrypt.
 * @return Dynamically allocated hex string (Caller must free). Returns NULL on error.
 */
char *crypto_encrypt(const char *plaintext);

/**
 * @brief Decrypts a hex-encoded ciphertext back to plaintext.
 * @param ciphertext_hex The hex string from the DB.
 * @return Dynamically allocated plaintext string (Caller must free). Returns NULL on error.
 */
char *crypto_decrypt(const char *ciphertext_hex);

#endif