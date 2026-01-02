#include "services/contact_service.h"
#include "infrastructure/client_context.h"
#include "system/protocol.h"
#include <string.h>

void service_refresh_contacts(void) {
    send_packet(app.ssl, MSG_REQ_CONTACTS, NULL, 0);
}

void service_refresh_requests(void) {
    send_packet(app.ssl, MSG_GET_REQUESTS, NULL, 0);
}

void service_add_contact(const char *friend_code) {
    AddContactPayload acp = {0};
    strncpy(acp.friend_code, friend_code, FRIEND_CODE_LEN);
    send_packet(app.ssl, MSG_ADD_BY_CODE, &acp, sizeof(acp));
}

void service_decide_request(uint32_t target_uid, int accept) {
    DecideRequestPayload p;
    p.target_uid = target_uid;
    p.accepted = accept;
    send_packet(app.ssl, MSG_DECIDE_REQUEST, &p, sizeof(p));
}