#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include "protocol.h"

typedef struct User {
    uint32_t uid;
    char username[MAX_NAME_LEN];
    char email[MAX_EMAIL_LEN];
    char password_hash[128];
    char friend_code[FRIEND_CODE_LEN];
} User;

int storage_init(const char *db_path);
void storage_backup(const char *db_path);
void storage_close(void);

// User Management
int storage_register_user(const char *email, const char *username, const char *password, char *friend_code_out);
int storage_check_credentials(const char *email, const char *password, User *user_out);
int storage_update_user(uint32_t uid, const char *new_username, const char *new_password);

// Lookups
int storage_get_uid_by_code(const char *code, uint32_t *uid_out);
int storage_get_user_by_uid(uint32_t uid, User *user_out);

// Contacts
int storage_add_request(uint32_t from_uid, uint32_t to_uid);
int storage_remove_request(uint32_t from_uid, uint32_t to_uid);
int storage_add_friendship(uint32_t uid_a, uint32_t uid_b);
int storage_get_contacts_data(uint32_t uid, ContactSummary *out_array, int max_count);
int storage_get_requests_data(uint32_t uid, ContactSummary *out_array, int max_count);

// Group Management
int storage_get_group_members(uint32_t conv_id, GroupMemberSummary *out_array, int max_count);
int storage_remove_participant(uint32_t conv_id, uint32_t uid);
int storage_delete_conversation(uint32_t conv_id);

// --- CONVERSATIONS UPDATED ---

// Creates a conversation. Returns 0 on failure.
uint32_t storage_create_conversation(uint8_t type, const char *name, const char *desc, uint32_t *uids, int count);

// Checks if a private conversation exists between two users. Returns conv_id or 0.
uint32_t storage_find_private_conversation(uint32_t uid_a, uint32_t uid_b);

int storage_get_user_conversations(uint32_t uid, ConversationSummary *out_array, int max_count);
int storage_get_conv_participants(uint32_t conv_id, uint32_t *out_uids, int max_count);

int storage_is_admin(uint32_t conv_id, uint32_t uid);
int storage_update_group(uint32_t conv_id, const char *name, const char *desc);
int storage_add_participant(uint32_t conv_id, uint32_t uid, int role);

// Messaging
void storage_log_message(uint32_t conv_id, uint32_t sender_uid, const char *text);
char* storage_get_history(uint32_t conv_id);

#endif