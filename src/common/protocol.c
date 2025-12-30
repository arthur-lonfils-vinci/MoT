#include "protocol.h"
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>

int send_all(int sockfd, const void *data, size_t len) {
    const char *ptr = (const char *)data;
    size_t total_sent = 0;
    
    while (total_sent < len) {
        ssize_t sent = send(sockfd, ptr + total_sent, len - total_sent, 0);
        if (sent == -1) return -1; // Error
        total_sent += sent;
    }
    return 0; // Success
}

int recv_all(int sockfd, void *data, size_t len) {
    char *ptr = (char *)data;
    size_t total_received = 0;
    
    while (total_received < len) {
        ssize_t received = recv(sockfd, ptr + total_received, len - total_received, 0);
        if (received <= 0) return received; // 0=Disconnect, -1=Error
        total_received += received;
    }
    return 1; // Success
}

int send_packet(int sockfd, MessageType type, const void *payload, uint32_t payload_len) {
    MessageHeader header;
    header.type = htonl(type); // Host to Network Long
    header.payload_len = htonl(payload_len);

    // 1. Send Header
    if (send_all(sockfd, &header, sizeof(header)) == -1) return -1;

    // 2. Send Payload (if exists)
    if (payload_len > 0 && payload != NULL) {
        if (send_all(sockfd, payload, payload_len) == -1) return -1;
    }
    
    return 1;
}

int recv_packet(int sockfd, MessageType *type, void **payload_out, uint32_t *payload_len_out) {
    MessageHeader header;

    // 1. Read Header
    int status = recv_all(sockfd, &header, sizeof(header));
    if (status <= 0) return status; // Error or Disconnect

    *type = ntohl(header.type); // Network to Host Long
    *payload_len_out = ntohl(header.payload_len);

    // 2. Read Payload
    if (*payload_len_out > 0) {
        *payload_out = malloc(*payload_len_out + 1); // +1 for safety (null terminator)
        if (!*payload_out) return -1; // OOM

        if (recv_all(sockfd, *payload_out, *payload_len_out) <= 0) {
            free(*payload_out);
            return -1;
        }
        
        // Null terminate for safety if it's treated as string later
        ((char*)*payload_out)[*payload_len_out] = '\0';
    } else {
        *payload_out = NULL;
    }

    return 1;
}