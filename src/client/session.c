#include <stdio.h>
#include <string.h>
#include "system/protocol.h"

#define SESSION_FILE "session.dat"

// Save credentials (In prod, use an OS keychain, not a file)
void save_session(const char *email, const char *password) {
    FILE *f = fopen(SESSION_FILE, "wb");
    if (f) {
        LoginPayload p;
        strncpy(p.email, email, MAX_EMAIL_LEN);
        strncpy(p.password, password, MAX_PASS_LEN);
        fwrite(&p, sizeof(LoginPayload), 1, f);
        fclose(f);
    }
}

// Returns 1 if session found and loaded into payload
int load_session(LoginPayload *p) {
    FILE *f = fopen(SESSION_FILE, "rb");
    if (!f) return 0;
    
    size_t read = fread(p, sizeof(LoginPayload), 1, f);
    fclose(f);
    return (read == 1);
}

void clear_session() {
    remove(SESSION_FILE);
}