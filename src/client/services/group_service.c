#include "services/group_service.h"
#include "infrastructure/client_context.h"
#include "system/protocol.h"
#include <string.h>

int service_create_group(const char *name, const char *desc, uint32_t *uids, int count)
{
    if (count < 1) return 0;

    CreateConvPayload ccp = {0};
    ccp.type = CONV_TYPE_GROUP;
    strncpy(ccp.name, name, 31);
    strncpy(ccp.description, desc, 63);
    
    ccp.participant_uids[0] = app.my_info.uid;
    int actual_count = 1;
    
    for(int i=0; i<count && actual_count < MAX_PARTICIPANTS; i++) {
        ccp.participant_uids[actual_count++] = uids[i];
    }
    ccp.participants_count = actual_count;

    send_packet(app.ssl, MSG_CREATE_CONV, &ccp, sizeof(ccp));
    return 1;
}

void service_create_private(uint32_t target_uid, const char *target_name)
{
    CreateConvPayload ccp = {0};
    strncpy(ccp.name, target_name, 32);
    ccp.participants_count = 2;
    ccp.participant_uids[0] = app.my_info.uid;
    ccp.participant_uids[1] = target_uid;
    ccp.type = CONV_TYPE_PRIVATE;
    send_packet(app.ssl, MSG_CREATE_CONV, &ccp, sizeof(ccp));
}

void service_req_conversations(void) {
    send_packet(app.ssl, MSG_REQ_CONVERSATIONS, NULL, 0);
}

void service_delete_group(uint32_t conv_id) {
    DeleteGroupPayload p = {.conv_id = conv_id};
    send_packet(app.ssl, MSG_DELETE_GROUP, &p, sizeof(p));
}

void service_req_members(uint32_t conv_id) {
    ReqMembersPayload p = {.conv_id = conv_id};
    send_packet(app.ssl, MSG_REQ_MEMBERS, &p, sizeof(p));
}

void service_kick_member(uint32_t conv_id, uint32_t target_uid) {
    KickMemberPayload p;
    p.conv_id = conv_id;
    p.target_uid = target_uid;
    send_packet(app.ssl, MSG_KICK_MEMBER, &p, sizeof(p));
}