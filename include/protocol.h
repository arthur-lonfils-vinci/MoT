#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define PORT 8085
#define BUFFER_SIZE 4096
#define MAX_NAME_LEN 32
#define MAX_EMAIL_LEN 64
#define MAX_PASS_LEN 64
#define MAX_TEXT_LEN 1024
#define MAX_DESC_LEN 64
#define FRIEND_CODE_LEN 7
#define MAX_PARTICIPANTS 10
#define MAX_MEMBERS 50

#define CONV_TYPE_PRIVATE 0
#define CONV_TYPE_GROUP 1

#define ROLE_MEMBER 0
#define ROLE_ADMIN 1

typedef enum Protocol
{
	// Auth
	MSG_REGISTER = 1,
	MSG_REGISTER_SUCCESS,
	MSG_REGISTER_FAIL,
	MSG_LOGIN,
	MSG_LOGIN_SUCCESS,
	MSG_LOGIN_FAIL,

	// User Profile
	MSG_UPDATE_USER,
	MSG_UPDATE_SUCCESS,
	MSG_UPDATE_FAIL,

	// Contacts / Requests (V4 Binary)
	MSG_REQ_CONTACTS,
	MSG_RESP_CONTACTS, // Payload: Array of ContactSummary
	MSG_ADD_BY_CODE,
	MSG_ADD_REQ_SENT,
	MSG_ADD_SUCCESS,
	MSG_ADD_FAIL,
	MSG_GET_REQUESTS,
	MSG_RESP_REQUESTS, // Payload: Array of ContactSummary
	MSG_DECIDE_REQUEST,

	// Conversations (V4)
	MSG_CREATE_CONV = 19,
	MSG_CONV_CREATED,
	MSG_REQ_CONVERSATIONS,
	MSG_RESP_CONVERSATIONS, // Payload: Array of ConversationSummary

	// Group Management
	MSG_UPDATE_GROUP,		 // Update Name/Desc
	MSG_ADD_MEMBER = 24, // Add user to group
	MSG_MEMBER_ADDED,		 // Success response
	MSG_REQ_MEMBERS,	// Ask for member list
	MSG_RESP_MEMBERS, // Receive member list
	MSG_KICK_MEMBER,	// Admin kicks user
	MSG_DELETE_GROUP, // Admin deletes group

	// Messaging
	MSG_SEND_TEXT,
	MSG_RTE_TEXT,
	MSG_REQ_HISTORY,
	MSG_RESP_HISTORY,

	MSG_DISCONNECT
} MessageType;

typedef struct __attribute__((packed))
{
	uint32_t type;
	uint32_t payload_len;
} MessageHeader;

// --- PAYLOADS ---

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

// 3. Contacts & Requests
typedef struct __attribute__((packed))
{
	char friend_code[FRIEND_CODE_LEN];
} AddContactPayload;

// Standard struct for sending user lists (Contacts, Requests)
typedef struct __attribute__((packed))
{
	uint32_t uid;
	char username[MAX_NAME_LEN];
	int is_online;
} ContactSummary;

typedef struct __attribute__((packed))
{
	uint32_t target_uid; // Strict UID logic
	uint8_t accepted;		 // 1=Yes, 0=No
} DecideRequestPayload;

// 4. Conversations
typedef struct __attribute__((packed))
{
	uint8_t type;
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
	uint8_t my_role;
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
    uint8_t role; // 1 = Admin, 0 = Member
} GroupMemberSummary;

typedef struct __attribute__((packed)) {
    uint32_t conv_id;
    uint32_t target_uid;
} KickMemberPayload;

typedef struct __attribute__((packed)) {
    uint32_t conv_id;
} DeleteGroupPayload;

// Functions
int send_all(int sockfd, const void *data, size_t len);
int recv_all(int sockfd, void *data, size_t len);
int send_packet(int sockfd, MessageType type, const void *payload, uint32_t payload_len);
int recv_packet(int sockfd, MessageType *type, void **payload_out, uint32_t *payload_len_out);

#endif