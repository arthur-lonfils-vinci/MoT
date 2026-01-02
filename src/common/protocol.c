#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "system/protocol.h"

// --- SSL HELPERS ---

void init_openssl(void) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl(void) {
    EVP_cleanup();
}

SSL_CTX *create_context(int is_server) {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    if (is_server)
        method = TLS_server_method();
    else
        method = TLS_client_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configure_context(SSL_CTX *ctx, const char *cert_file, const char *key_file) {
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

// --- NETWORK OPS ---

int send_all(SSL *ssl, const void *data, size_t len) {
    const char *ptr = (const char *)data;
    size_t total_sent = 0;
    
    while (total_sent < len) {
        int sent = SSL_write(ssl, ptr + total_sent, len - total_sent);
        if (sent <= 0) {
            // Handle error (check SSL_get_error if robust, for now assume fail)
            return -1; 
        }
        total_sent += sent;
    }
    return 0; // Success
}

int recv_all(SSL *ssl, void *data, size_t len) {
    char *ptr = (char *)data;
    size_t total_received = 0;
    
    while (total_received < len) {
        int received = SSL_read(ssl, ptr + total_received, len - total_received);
        if (received <= 0) {
             // 0 usually means clean shutdown, < 0 error
             return received;
        }
        total_received += received;
    }
    return 1; // Success
}

int load_cert_from_memory(SSL_CTX *ctx, const char *cert_pem) {
    if (!cert_pem || strlen(cert_pem) == 0) return 0;

    // Create a read-only memory BIO
    BIO *bio = BIO_new_mem_buf(cert_pem, -1);
    if (!bio) return 0;

    // Parse the X509 certificate
    X509 *cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    if (!cert) {
        BIO_free(bio);
        return 0;
    }

    // Get the cert store from the context and add the cert
    X509_STORE *store = SSL_CTX_get_cert_store(ctx);
    int ret = X509_STORE_add_cert(store, cert);

    X509_free(cert);
    BIO_free(bio);

    return ret; // 1 Success, 0 Fail
}

int send_packet(SSL *ssl, MessageType type, const void *payload, uint32_t payload_len) {
    if (!ssl) return -1;
    MessageHeader header;
    header.type = htonl(type);
    header.payload_len = htonl(payload_len);

    if (send_all(ssl, &header, sizeof(header)) == -1) return -1;

    if (payload_len > 0 && payload != NULL) {
        if (send_all(ssl, payload, payload_len) == -1) return -1;
    }
    return 1;
}

int recv_packet(SSL *ssl, MessageType *type, void **payload_out, uint32_t *payload_len_out) {
    if (!ssl) return -1;
    MessageHeader header;

    int status = recv_all(ssl, &header, sizeof(header));
    if (status <= 0) return status;

    *type = ntohl(header.type);
    *payload_len_out = ntohl(header.payload_len);

    if (*payload_len_out > 0) {
        *payload_out = malloc(*payload_len_out + 1);
        if (!*payload_out) return -1;

        if (recv_all(ssl, *payload_out, *payload_len_out) <= 0) {
            free(*payload_out);
            return -1;
        }
        ((char*)*payload_out)[*payload_len_out] = '\0';
    } else {
        *payload_out = NULL;
    }
    return 1;
}