#include "handlers/auth_handler.h"
#include "system/storage.h"
#include "system/logger.h"
#include <string.h>
#include <stdio.h>

void handle_register(Client *cli, const RegisterPayload *p) {
    char code[FRIEND_CODE_LEN];
    // storage_register_user generates the friend code and returns 1 on success
    if (storage_register_user(p->email, p->username, p->password, code)) {
        log_print(LOG_INFO, "Registered new user: %s (%s)", p->username, p->email);
        send_packet(cli->ssl, MSG_REGISTER_SUCCESS, NULL, 0);
    } else {
        log_print(LOG_WARN, "Registration failed for: %s", p->email);
        send_packet(cli->ssl, MSG_REGISTER_FAIL, NULL, 0);
    }
}

void handle_login(Client *cli, const LoginPayload *p) {
    User u;
    log_print(LOG_INFO, "Login attempt from FD %d", cli->fd);
    
    if (storage_check_credentials(p->email, p->password, &u)) {
        // Update the Client state (in memory)
        strncpy(cli->username, u.username, MAX_NAME_LEN);
        cli->uid = u.uid;
        cli->is_online = 1;

        // Prepare response payload
        MyInfoPayload info;
        info.uid = u.uid;
        strncpy(info.username, u.username, MAX_NAME_LEN);
        strncpy(info.email, u.email, MAX_EMAIL_LEN);
        strncpy(info.friend_code, u.friend_code, FRIEND_CODE_LEN);
        
        send_packet(cli->ssl, MSG_LOGIN_SUCCESS, &info, sizeof(info));
        log_print(LOG_INFO, "User %s (UID: %d) logged in", u.username, u.uid);
    } else {
        log_print(LOG_WARN, "Login failed from FD %d", cli->fd);
        send_packet(cli->ssl, MSG_LOGIN_FAIL, NULL, 0);
    }
}

void handle_update_user(Client *cli, const UpdateUserPayload *p)
{
    storage_update_user(cli->uid, p->new_username, p->new_password);
    if (strlen(p->new_username) > 0)
        strncpy(cli->username, p->new_username, MAX_NAME_LEN);
    send_packet(cli->ssl, MSG_UPDATE_SUCCESS, NULL, 0);
}