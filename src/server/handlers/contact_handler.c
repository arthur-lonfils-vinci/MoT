#include "handlers/contact_handler.h"
#include "infrastructure/client_manager.h"
#include "system/storage.h"
#include "system/logger.h"

void handle_req_contacts(Client *cli)
{
    ContactSummary contacts[50];
    int count = storage_get_contacts_data(cli->uid, contacts, 50);
    send_packet(cli->ssl, MSG_RESP_CONTACTS, contacts, count * sizeof(ContactSummary));
}

void handle_add_by_code(Client *cli, const AddContactPayload *p)
{
    uint32_t target_uid;
    if (storage_get_uid_by_code(p->friend_code, &target_uid)) {
        if (storage_add_request(cli->uid, target_uid)) {
            send_packet(cli->ssl, MSG_ADD_REQ_SENT, NULL, 0);
            
            Client *tc = get_client_by_uid(target_uid);
            if (tc && tc->is_online) {
                ContactSummary reqs[50];
                int count = storage_get_requests_data(target_uid, reqs, 50);
                send_packet(tc->ssl, MSG_RESP_REQUESTS, reqs, count * sizeof(ContactSummary));
            }
        } else {
            send_packet(cli->ssl, MSG_ADD_FAIL, NULL, 0);
        }
    } else {
        send_packet(cli->ssl, MSG_ADD_FAIL, NULL, 0);
    }
}

void handle_get_requests(Client *cli)
{
    ContactSummary reqs[50];
    int count = storage_get_requests_data(cli->uid, reqs, 50);
    send_packet(cli->ssl, MSG_RESP_REQUESTS, reqs, count * sizeof(ContactSummary));
}

void handle_decide_request(Client *cli, const DecideRequestPayload *p)
{
    if (p->accepted) {
        storage_add_friendship(cli->uid, p->target_uid);
        
        // Auto-create private chat
        uint32_t uids[2] = {cli->uid, p->target_uid};
        uint32_t conv_id = storage_find_private_conversation(cli->uid, p->target_uid);
        
        int new_conv = 0;
        if (conv_id == 0) {
            storage_create_conversation(CONV_TYPE_PRIVATE, "Private Chat", "", uids, 2);
            new_conv = 1;
        }

        if (new_conv) {
            // Refresh Me
            ConversationSummary my_convs[50];
            int c1 = storage_get_user_conversations(cli->uid, my_convs, 50);
            send_packet(cli->ssl, MSG_RESP_CONVERSATIONS, my_convs, c1 * sizeof(ConversationSummary));

            // Refresh Them
            Client *orig = get_client_by_uid(p->target_uid);
            if (orig && orig->is_online) {
                ConversationSummary their_convs[50];
                int c2 = storage_get_user_conversations(p->target_uid, their_convs, 50);
                send_packet(orig->ssl, MSG_RESP_CONVERSATIONS, their_convs, c2 * sizeof(ConversationSummary));
            }
        }
    }
    
    storage_remove_request(p->target_uid, cli->uid);

    // Refresh lists
    handle_req_contacts(cli); // Send contacts
    handle_get_requests(cli); // Send requests remaining

    // Refresh Sender if accepted
    if (p->accepted) {
        Client *orig = get_client_by_uid(p->target_uid);
        if (orig && orig->is_online) {
            ContactSummary sender_contacts[50];
            int s_count = storage_get_contacts_data(p->target_uid, sender_contacts, 50);
            send_packet(orig->ssl, MSG_RESP_CONTACTS, sender_contacts, s_count * sizeof(ContactSummary));
        }
    }
}