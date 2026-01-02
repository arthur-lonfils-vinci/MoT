#include "services/chat_service.h"
#include "infrastructure/client_context.h"
#include "system/protocol.h"
#include <string.h>

void service_send_text(uint32_t conv_id, const char *text)
{
    if (strlen(text) == 0) return;
    
    SendMessagePayload msg;
    msg.conv_id = conv_id;
    strncpy(msg.text, text, MAX_TEXT_LEN - 1);
    msg.text[MAX_TEXT_LEN - 1] = '\0';
    
    send_packet(app.ssl, MSG_SEND_TEXT, &msg, sizeof(msg));
}

void service_req_history(uint32_t conv_id)
{
    RequestHistoryPayload hp;
    hp.conv_id = conv_id;
    send_packet(app.ssl, MSG_REQ_HISTORY, &hp, sizeof(hp));
}