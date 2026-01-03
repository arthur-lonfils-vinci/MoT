/**
 * @file protocol.h
 * @brief Defines the binary protocol, packet structures, and networking primitives.
 *
 * This header contains all the shared constants, enums, and payload structures
 * used by both Client and Server to communicate over TCP/SSL.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// --- CONSTANTS ---
#define BUFFER_SIZE 4096

// Field Limits
#define MAX_NAME_LEN 32
#define MAX_EMAIL_LEN 64
#define MAX_PASS_LEN 64
#define MAX_TEXT_LEN 1024
#define MAX_DESC_LEN 64
#define FRIEND_CODE_LEN 7

// Logic Limits
#define MAX_PARTICIPANTS 10
#define MAX_MEMBERS 50

// Types & Roles
#define CONV_TYPE_PRIVATE 0
#define CONV_TYPE_GROUP 1
#define ROLE_MEMBER 0
#define ROLE_ADMIN 1

/**
 * @brief Enumeration of all supported message types.
 *
 * Each type corresponds to a specific logic flow and often a specific
 * payload structure defined below.
 */
typedef enum Protocol
{
    // --- Auth ---
    MSG_REGISTER = 1,       /**< Request: RegisterPayload */
    MSG_REGISTER_SUCCESS,   /**< Response: Empty */
    MSG_REGISTER_FAIL,      /**< Response: Empty */
    MSG_LOGIN,              /**< Request: LoginPayload */
    MSG_LOGIN_SUCCESS,      /**< Response: MyInfoPayload */
    MSG_LOGIN_FAIL,         /**< Response: Empty */

    // --- User Profile ---
    MSG_UPDATE_USER,        /**< Request: UpdateUserPayload */
    MSG_UPDATE_SUCCESS,     /**< Response: Empty */
    MSG_UPDATE_FAIL,        /**< Response: Empty */

    // --- Contacts ---
    MSG_REQ_CONTACTS,       /**< Request: Empty */
    MSG_RESP_CONTACTS,      /**< Response: Array of ContactSummary */
    MSG_ADD_BY_CODE,        /**< Request: AddContactPayload */
    MSG_ADD_REQ_SENT,       /**< Response: Empty */
    MSG_ADD_SUCCESS,        /**< Response: Empty */
    MSG_ADD_FAIL,           /**< Response: Empty */
    MSG_GET_REQUESTS,       /**< Request: Empty */
    MSG_RESP_REQUESTS,      /**< Response: Array of ContactSummary */
    MSG_DECIDE_REQUEST,     /**< Request: DecideRequestPayload */

    // --- Conversations ---
    MSG_CREATE_CONV = 19,   /**< Request: CreateConvPayload */
    MSG_CONV_CREATED,       /**< Response: uint32_t conv_id */
    MSG_REQ_CONVERSATIONS,  /**< Request: Empty */
    MSG_RESP_CONVERSATIONS, /**< Response: Array of ConversationSummary */

    // --- Group Management ---
    MSG_UPDATE_GROUP,       /**< Request: UpdateGroupPayload */
    MSG_ADD_MEMBER = 24,    /**< Request: AddMemberPayload */
    MSG_MEMBER_ADDED,       /**< Response: Empty */
    MSG_REQ_MEMBERS,        /**< Request: ReqMembersPayload */
    MSG_RESP_MEMBERS,       /**< Response: Array of GroupMemberSummary */
    MSG_KICK_MEMBER,        /**< Request: KickMemberPayload */
    MSG_DELETE_GROUP,       /**< Request: DeleteGroupPayload */

    // --- Messaging ---
    MSG_SEND_TEXT,          /**< Request: SendMessagePayload */
    MSG_RTE_TEXT,           /**< Async Push: RoutedMessagePayload */
    MSG_REQ_HISTORY,        /**< Request: RequestHistoryPayload */
    MSG_RESP_HISTORY,       /**< Response: Raw char string (history) */

    MSG_DISCONNECT          /**< Internal/Signal: Connection closed */
} MessageType;

/**
 * @brief Standard header prefixed to every network packet.
 */
typedef struct __attribute__((packed))
{
    uint32_t type;          /**< MessageType enum value */
    uint32_t payload_len;   /**< Size of the following payload in bytes */
} MessageHeader;

// --- PAYLOAD DEFINITIONS ---

// 1. Auth
typedef struct __attribute__((packed))
{
    char email[MAX_EMAIL_LEN];
    char username[MAX_NAME_LEN];
    char password[MAX_PASS_LEN];
} RegisterPayload;

typedef struct __attribute__((packed))
{
    char email[MAX_EMAIL_LEN];
    char password[MAX_PASS_LEN];
} LoginPayload;

// 2. Info
typedef struct __attribute__((packed))
{
    uint32_t uid;
    char username[MAX_NAME_LEN];
    char email[MAX_EMAIL_LEN];
    char friend_code[FRIEND_CODE_LEN];
} MyInfoPayload;

typedef struct __attribute__((packed))
{
    char new_username[MAX_NAME_LEN];
    char new_password[MAX_PASS_LEN];
} UpdateUserPayload;

// 3. Contacts
typedef struct __attribute__((packed))
{
    char friend_code[FRIEND_CODE_LEN];
} AddContactPayload;

typedef struct __attribute__((packed))
{
    uint32_t uid;
    char username[MAX_NAME_LEN];
    int is_online;
} ContactSummary;

typedef struct __attribute__((packed))
{
    uint32_t target_uid; 
    uint8_t accepted; /**< 1 = Accept, 0 = Deny */
} DecideRequestPayload;

// 4. Conversations
typedef struct __attribute__((packed))
{
    uint8_t type; /**< CONV_TYPE_PRIVATE or CONV_TYPE_GROUP */
    char name[32];
    char description[MAX_DESC_LEN];
    uint32_t participants_count;
    uint32_t participant_uids[MAX_PARTICIPANTS];
} CreateConvPayload;

typedef struct __attribute__((packed))
{
    uint32_t conv_id;
    uint8_t type;
    char name[32];
    char description[MAX_DESC_LEN];
    uint32_t unread_count;
    uint8_t my_role; /**< ROLE_MEMBER or ROLE_ADMIN */
} ConversationSummary;

// 5. Messaging
typedef struct __attribute__((packed))
{
    uint32_t conv_id;
    char text[MAX_TEXT_LEN];
} SendMessagePayload;

typedef struct __attribute__((packed))
{
    uint32_t conv_id;
    uint32_t sender_uid;
    char sender_username[MAX_NAME_LEN];
    char text[MAX_TEXT_LEN];
} RoutedMessagePayload;

typedef struct __attribute__((packed))
{
    uint32_t conv_id;
} RequestHistoryPayload;

typedef struct __attribute__((packed))
{
    uint32_t conv_id;
    char new_name[32];
    char new_desc[MAX_DESC_LEN];
} UpdateGroupPayload;

typedef struct __attribute__((packed))
{
    uint32_t conv_id;
    char target_friend_code[FRIEND_CODE_LEN];
} AddMemberPayload;

typedef struct __attribute__((packed)) {
    uint32_t conv_id;
} ReqMembersPayload;

typedef struct __attribute__((packed)) {
    uint32_t uid;
    char username[MAX_NAME_LEN];
    uint8_t role; 
} GroupMemberSummary;

typedef struct __attribute__((packed)) {
    uint32_t conv_id;
    uint32_t target_uid;
} KickMemberPayload;

typedef struct __attribute__((packed)) {
    uint32_t conv_id;
} DeleteGroupPayload;

// --- SSL & NETWORK FUNCTIONS ---

/**
 * @brief Initialize OpenSSL libraries. Call once at startup.
 */
void init_openssl(void);

/**
 * @brief Cleanup OpenSSL. Call at shutdown.
 */
void cleanup_openssl(void);

/**
 * @brief Create an SSL Context.
 * @param is_server 1 for Server Mode, 0 for Client Mode.
 * @return Pointer to SSL_CTX.
 */
SSL_CTX *create_context(int is_server);

/**
 * @brief Load certificate and private key into the context.
 * @param ctx The SSL Context.
 * @param cert_file Path to .crt file.
 * @param key_file Path to .key file.
 */
void configure_context(SSL_CTX *ctx, const char *cert_file, const char *key_file);

/**
 * @brief Loads a CA certificate from a memory string into the SSL Context for verification.
 * @param ctx The SSL Context.
 * @param cert_pem The certificate content in PEM format (null-terminated string).
 * @return 1 on success, 0 on failure.
 */
int load_cert_from_memory(SSL_CTX *ctx, const char *cert_pem);

/**
 * @brief Sends a full Protocol Message (Header + Payload) atomically via SSL.
 * @param ssl The SSL connection handle.
 * @param type The MessageType.
 * @param payload Pointer to the payload data (can be NULL if payload_len is 0).
 * @param payload_len Size of the payload.
 * @return 1 on success, -1 on failure.
 */
int send_packet(SSL *ssl, MessageType type, const void *payload, uint32_t payload_len);

/**
 * @brief Receives a full Protocol Message via SSL.
 * Allocates memory for *payload_out if payload_len > 0.
 * The caller is responsible for freeing *payload_out.
 * @param ssl The SSL connection handle.
 * @param type_out Pointer to store received MessageType.
 * @param payload_out Pointer to store pointer to allocated payload buffer.
 * @param payload_len_out Pointer to store size of received payload.
 * @return >0 on success (bytes received), 0 on clean disconnect, -1 on error.
 */
int recv_packet(SSL *ssl, MessageType *type_out, void **payload_out, uint32_t *payload_len_out);

#endif