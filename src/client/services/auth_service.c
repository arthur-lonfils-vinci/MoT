#include "services/auth_service.h"
#include "infrastructure/client_context.h"
#include "system/logger.h"
#include <string.h>
#include <stdlib.h>

int service_register(const char *email, const char *username, const char *password)
{
    RegisterPayload reg = {0};
    strncpy(reg.email, email, MAX_EMAIL_LEN - 1);
    strncpy(reg.username, username, MAX_NAME_LEN - 1);
    strncpy(reg.password, password, MAX_PASS_LEN - 1);

    send_packet(app.ssl, MSG_REGISTER, &reg, sizeof(reg));

    // Wait for response (Blocking)
    MessageType type;
    void *p = NULL;
    uint32_t len;
    
    // Simple blocking wait for specific packet
    if (recv_packet(app.ssl, &type, &p, &len) > 0) {
        int success = (type == MSG_REGISTER_SUCCESS);
        if (p) free(p);
        return success;
    }
    return 0;
}

int service_login(const char *email, const char *password)
{
    LoginPayload log = {0};
    strncpy(log.email, email, MAX_EMAIL_LEN - 1);
    strncpy(log.password, password, MAX_PASS_LEN - 1);

    send_packet(app.ssl, MSG_LOGIN, &log, sizeof(log));

    MessageType type;
    void *p = NULL;
    uint32_t len;

    if (recv_packet(app.ssl, &type, &p, &len) > 0) {
        if (type == MSG_LOGIN_SUCCESS && p) {
            memcpy(&app.my_info, p, sizeof(MyInfoPayload));
            free(p);
            return 1;
        }
        if (p) free(p);
    }
    return 0;
}

void service_logout(void)
{
    clear_session();
    // We don't necessarily send a disconnect packet here as the main loop will close socket
}

void service_update_user(const char *username, const char *password)
{
    UpdateUserPayload up = {0};
    if (username) strncpy(up.new_username, username, MAX_NAME_LEN - 1);
    if (password) strncpy(up.new_password, password, MAX_PASS_LEN - 1);
    
    send_packet(app.ssl, MSG_UPDATE_USER, &up, sizeof(up));
}