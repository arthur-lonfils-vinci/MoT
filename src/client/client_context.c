#include "infrastructure/client_context.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

AppState app;

void app_init(void) {
    memset(&app, 0, sizeof(AppState));
    pthread_mutex_init(&app.state_lock, NULL);
    app.current_state = STATE_START;

		app.contacts = NULL;
    app.requests = NULL;
    app.conversations = NULL;
    app.current_group_members = NULL;
    app.chat_history = NULL;
}

void app_cleanup(void) {
    pthread_mutex_lock(&app.state_lock);
    
    if (app.contacts) free(app.contacts);
    if (app.requests) free(app.requests);
    if (app.conversations) free(app.conversations);
    if (app.current_group_members) free(app.current_group_members);
    if (app.chat_history) free(app.chat_history);
    
    pthread_mutex_unlock(&app.state_lock);
    pthread_mutex_destroy(&app.state_lock);
}

void app_remove_request(int index) {
    pthread_mutex_lock(&app.state_lock);
    if (index >= 0 && index < app.requests_count && app.requests) {
        // Shift array
        for (int i = index; i < app.requests_count - 1; i++) {
            app.requests[i] = app.requests[i + 1];
        }
        app.requests_count--;
        // Optional: realloc down to save space
    }
    pthread_mutex_unlock(&app.state_lock);
}

void app_append_history(const char *text) {
    if (!text) return;
    
    size_t new_len = strlen(text);
    
    if (app.chat_history == NULL) {
        app.chat_history = strdup(text);
        app.chat_history_len = new_len;
    } else {
        size_t total = app.chat_history_len + new_len + 1;
        char *ptr = realloc(app.chat_history, total);
        if (ptr) {
            app.chat_history = ptr;
            strcat(app.chat_history, text);
            app.chat_history_len += new_len;
        }
    }
}