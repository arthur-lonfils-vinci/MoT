#include "client_context.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

AppState app;

void app_init(void) {
    memset(&app, 0, sizeof(AppState));
    pthread_mutex_init(&app.state_lock, NULL);
    app.current_state = STATE_START;
}

void app_remove_request(int index) {
    pthread_mutex_lock(&app.state_lock);
    if (index >= 0 && index < app.requests_count) {
        // Shift array
        for (int i = index; i < app.requests_count - 1; i++) {
            app.requests[i] = app.requests[i + 1];
        }
        app.requests_count--;
    }
    pthread_mutex_unlock(&app.state_lock);
}