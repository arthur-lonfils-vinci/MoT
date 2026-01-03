#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <crypt.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// System
#include "system/crypto.h"
#include "system/storage.h"

static sqlite3 *db = NULL;

void generate_random_salt(char *buffer)
{
	const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
	// Format: $6$rounds=5000$ + 16 chars of salt
	strcpy(buffer, "$6$rounds=5000$");
	int base_len = strlen(buffer);

	for (int i = 0; i < 16; i++)
	{
		buffer[base_len + i] = charset[rand() % 64];
	}
	buffer[base_len + 16] = '\0';
}

static void ensure_directory(const char *path)
{
	struct stat st = {0};
	if (stat(path, &st) == -1)
	{
		mkdir(path, 0755);
	}
}

void generate_friend_code(char *buffer)
{
	const char charset[] = "0123456789ABCDEF";
	for (int i = 0; i < 6; i++)
		buffer[i] = charset[rand() % 16];
	buffer[6] = '\0';
}

int storage_init(const char *db_path)
{
	ensure_directory("data");

	if (sqlite3_open(db_path, &db) != SQLITE_OK)
	{
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		return 0;
	}

	const char *schema =
			"CREATE TABLE IF NOT EXISTS users ("
			"uid INTEGER PRIMARY KEY AUTOINCREMENT,"
			"username TEXT UNIQUE NOT NULL,"
			"email TEXT UNIQUE NOT NULL,"
			"password_hash TEXT NOT NULL,"
			"friend_code TEXT UNIQUE NOT NULL);"

			"CREATE TABLE IF NOT EXISTS contacts ("
			"user_id INTEGER, contact_id INTEGER,"
			"PRIMARY KEY(user_id, contact_id));"

			"CREATE TABLE IF NOT EXISTS requests ("
			"sender_id INTEGER, receiver_id INTEGER,"
			"PRIMARY KEY(sender_id, receiver_id));"

			"CREATE TABLE IF NOT EXISTS conversations ("
			"conv_id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"type INTEGER DEFAULT 0,"
			"name TEXT,"
			"description TEXT);"

			"CREATE TABLE IF NOT EXISTS participants ("
			"conv_id INTEGER, user_id INTEGER,"
			"role INTEGER DEFAULT 0,"
			"PRIMARY KEY(conv_id, user_id));"

			"CREATE TABLE IF NOT EXISTS messages ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"conv_id INTEGER, sender_id INTEGER, text TEXT, timestamp INTEGER);";

	char *err_msg = 0;
	if (sqlite3_exec(db, schema, 0, 0, &err_msg) != SQLITE_OK)
	{
		fprintf(stderr, "SQL Error: %s\n", err_msg);
		sqlite3_free(err_msg);
		return 0;
	}
	srand(time(NULL));
	return 1;
}

void storage_backup(const char *db_path)
{
	// 1. Create backups directory
	ensure_directory("data");
	ensure_directory("data/backups");

	// 2. Open Source
	FILE *src = fopen(db_path, "rb");
	if (!src)
		return; // No DB to backup yet (first run)

	// 3. Generate Destination Filename with Date
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	char dest_path[256];
	// e.g., data/backups/messagerie_2024-05-20.db
	snprintf(dest_path, sizeof(dest_path), "data/backups/messagerie_%04d-%02d-%02d.db",
					 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

	// 4. Copy File
	FILE *dst = fopen(dest_path, "wb");
	if (dst)
	{
		char buffer[8192];
		size_t n;
		while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
		{
			fwrite(buffer, 1, n, dst);
		}
		fclose(dst);
		printf("[Storage] Database backed up to %s\n", dest_path);
	}
	else
	{
		fprintf(stderr, "[Storage] Failed to create backup file %s\n", dest_path);
	}

	fclose(src);
}

void storage_close(void)
{
	if (db)
		sqlite3_close(db);
}

// --- USER AUTH ---

int storage_register_user(const char *email, const char *username, const char *password, char *friend_code_out)
{
	char code[8];
	generate_friend_code(code);
	char salt[64];
	generate_random_salt(salt);
	char *hash = crypt(password, salt);

	const char *sql = "INSERT INTO users (username, email, password_hash, friend_code) VALUES (?, ?, ?, ?)";
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;

	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, hash, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 4, code, -1, SQLITE_STATIC);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc == SQLITE_DONE)
	{
		strcpy(friend_code_out, code);
		return 1;
	}
	return 0;
}

int storage_check_credentials(const char *email, const char *password, User *user_out)
{
	const char *sql = "SELECT uid, username, email, password_hash, friend_code FROM users WHERE email = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;

	sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);

	int result = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const char *stored_hash = (const char *)sqlite3_column_text(stmt, 3);
		char *hash = crypt(password, stored_hash);

		if (strcmp(hash, stored_hash) == 0)
		{
			user_out->uid = sqlite3_column_int(stmt, 0);
			strncpy(user_out->username, (char *)sqlite3_column_text(stmt, 1), MAX_NAME_LEN);
			strncpy(user_out->email, (char *)sqlite3_column_text(stmt, 2), MAX_EMAIL_LEN);
			strncpy(user_out->password_hash, stored_hash, 128);
			strncpy(user_out->friend_code, (char *)sqlite3_column_text(stmt, 4), FRIEND_CODE_LEN);
			result = 1;
		}
	}
	sqlite3_finalize(stmt);
	return result;
}

int storage_update_user(uint32_t uid, const char *new_username, const char *new_password)
{
	if (strlen(new_username) > 0)
	{
		const char *sql = "UPDATE users SET username = ? WHERE uid = ?";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, new_username, -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 2, uid);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}
	if (strlen(new_password) > 0)
	{
		char salt[64];
		generate_random_salt(salt);
		char *hash = crypt(new_password, salt);
		const char *sql = "UPDATE users SET password_hash = ? WHERE uid = ?";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, hash, -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 2, uid);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}
	return 1;
}

// --- LOOKUPS ---

int storage_get_uid_by_code(const char *code, uint32_t *uid_out)
{
	const char *sql = "SELECT uid FROM users WHERE friend_code = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_text(stmt, 1, code, -1, SQLITE_STATIC);

	int res = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		*uid_out = sqlite3_column_int(stmt, 0);
		res = 1;
	}
	sqlite3_finalize(stmt);
	return res;
}

int storage_get_user_by_uid(uint32_t uid, User *user_out)
{
	const char *sql = "SELECT username, friend_code FROM users WHERE uid = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_int(stmt, 1, uid);

	int res = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		user_out->uid = uid;
		strncpy(user_out->username, (char *)sqlite3_column_text(stmt, 0), MAX_NAME_LEN);
		strncpy(user_out->friend_code, (char *)sqlite3_column_text(stmt, 1), FRIEND_CODE_LEN);
		res = 1;
	}
	sqlite3_finalize(stmt);
	return res;
}

// --- CONTACTS ---

int storage_add_request(uint32_t from_uid, uint32_t to_uid)
{
	if (from_uid == to_uid)
		return 0;
	const char *sql = "INSERT INTO requests (sender_id, receiver_id) VALUES (?, ?)";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_int(stmt, 1, from_uid);
	sqlite3_bind_int(stmt, 2, to_uid);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_DONE);
}

int storage_remove_request(uint32_t from_uid, uint32_t to_uid)
{
	const char *sql = "DELETE FROM requests WHERE sender_id = ? AND receiver_id = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_int(stmt, 1, from_uid);
	sqlite3_bind_int(stmt, 2, to_uid);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return 1;
}

int storage_add_friendship(uint32_t uid_a, uint32_t uid_b)
{
	const char *sql = "INSERT INTO contacts (user_id, contact_id) VALUES (?, ?), (?, ?)";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_int(stmt, 1, uid_a);
	sqlite3_bind_int(stmt, 2, uid_b);
	sqlite3_bind_int(stmt, 3, uid_b);
	sqlite3_bind_int(stmt, 4, uid_a);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return 1;
}

int storage_get_contacts_data(uint32_t uid, ContactSummary *out_array, int max_count)
{
	const char *sql = "SELECT u.uid, u.username FROM contacts c JOIN users u ON c.contact_id = u.uid WHERE c.user_id = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;

	sqlite3_bind_int(stmt, 1, uid);

	int count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count)
	{
		out_array[count].uid = sqlite3_column_int(stmt, 0);
		strncpy(out_array[count].username, (char *)sqlite3_column_text(stmt, 1), MAX_NAME_LEN);
		out_array[count].is_online = 0; // Can be linked to Client list later
		count++;
	}
	sqlite3_finalize(stmt);
	return count;
}

int storage_get_requests_data(uint32_t uid, ContactSummary *out_array, int max_count)
{
	const char *sql = "SELECT u.uid, u.username FROM requests r JOIN users u ON r.sender_id = u.uid WHERE r.receiver_id = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;

	sqlite3_bind_int(stmt, 1, uid);

	int count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count)
	{
		out_array[count].uid = sqlite3_column_int(stmt, 0);
		strncpy(out_array[count].username, (char *)sqlite3_column_text(stmt, 1), MAX_NAME_LEN);
		out_array[count].is_online = 0;
		count++;
	}
	sqlite3_finalize(stmt);
	return count;
}

// --- CONVERSATIONS ---

// Check if a private conversation already exists between two users
uint32_t storage_find_private_conversation(uint32_t uid_a, uint32_t uid_b)
{
	// Logic: Find a conv_id of type 0 where both UIDs are participants
	const char *sql =
			"SELECT c.conv_id FROM conversations c "
			"JOIN participants p1 ON c.conv_id = p1.conv_id "
			"JOIN participants p2 ON c.conv_id = p2.conv_id "
			"WHERE c.type = 0 AND p1.user_id = ? AND p2.user_id = ?";

	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;

	sqlite3_bind_int(stmt, 1, uid_a);
	sqlite3_bind_int(stmt, 2, uid_b);

	uint32_t found_id = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		found_id = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return found_id;
}

uint32_t storage_create_conversation(uint8_t type, const char *name, const char *desc, uint32_t *uids, int count)
{
	const char *sql_conv = "INSERT INTO conversations (type, name, description) VALUES (?, ?, ?)";
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, sql_conv, -1, &stmt, 0);

	sqlite3_bind_int(stmt, 1, type);
	sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, desc, -1, SQLITE_STATIC);

	if (sqlite3_step(stmt) != SQLITE_DONE)
	{
		sqlite3_finalize(stmt);
		return 0;
	}

	uint32_t conv_id = (uint32_t)sqlite3_last_insert_rowid(db);
	sqlite3_finalize(stmt);

	const char *sql_part = "INSERT INTO participants (conv_id, user_id, role) VALUES (?, ?, ?)";
	sqlite3_prepare_v2(db, sql_part, -1, &stmt, 0);

	for (int i = 0; i < count; i++)
	{
		sqlite3_bind_int(stmt, 1, conv_id);
		sqlite3_bind_int(stmt, 2, uids[i]);
		int role = (i == 0 && type == 1) ? 1 : 0;

		sqlite3_bind_int(stmt, 3, role);
		sqlite3_step(stmt);
		sqlite3_reset(stmt);
	}
	sqlite3_finalize(stmt);
	return conv_id;
}

int storage_get_user_conversations(uint32_t uid, ConversationSummary *out_array, int max_count)
{
	const char *sql = "SELECT c.conv_id, c.type, c.name, c.description, p.role FROM conversations c "
										"JOIN participants p ON c.conv_id = p.conv_id "
										"WHERE p.user_id = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;

	sqlite3_bind_int(stmt, 1, uid);

	int count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count)
	{
		out_array[count].conv_id = sqlite3_column_int(stmt, 0);
		int type = sqlite3_column_int(stmt, 1);
		out_array[count].type = (uint8_t)type;
		out_array[count].my_role = (uint8_t)sqlite3_column_int(stmt, 4);
		out_array[count].unread_count = 0;

		// DYNAMIC NAMING
		if (type == 0)
		{ // Private
			// Find the 'other' participant
			const char *sql_other = "SELECT u.username FROM participants p JOIN users u ON p.user_id = u.uid WHERE p.conv_id = ? AND p.user_id != ?";
			sqlite3_stmt *stmt2;
			sqlite3_prepare_v2(db, sql_other, -1, &stmt2, 0);
			sqlite3_bind_int(stmt2, 1, out_array[count].conv_id);
			sqlite3_bind_int(stmt2, 2, uid);

			if (sqlite3_step(stmt2) == SQLITE_ROW)
			{
				const char *other_name = (const char *)sqlite3_column_text(stmt2, 0);
				snprintf(out_array[count].name, 32, "Private with %s", other_name);
			}
			else
			{
				strcpy(out_array[count].name, "Private Chat");
			}
			sqlite3_finalize(stmt2);
			out_array[count].description[0] = '\0'; // No desc for private
		}
		else
		{
			// Group: Use stored name
			strncpy(out_array[count].name, (char *)sqlite3_column_text(stmt, 2), 32);
			const char *desc = (char *)sqlite3_column_text(stmt, 3);
			if (desc)
				strncpy(out_array[count].description, desc, MAX_DESC_LEN);
			else
				out_array[count].description[0] = '\0';
		}

		count++;
	}
	sqlite3_finalize(stmt);
	return count;
}

int storage_get_conv_participants(uint32_t conv_id, uint32_t *out_uids, int max_count)
{
	const char *sql = "SELECT user_id FROM participants WHERE conv_id = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_int(stmt, 1, conv_id);

	int count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count)
	{
		out_uids[count++] = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return count;
}

void storage_log_message(uint32_t conv_id, uint32_t sender_uid, const char *text)
{

	char *encrypted_text = crypto_encrypt(text);
	if (!encrypted_text)
		return;

	const char *sql = "INSERT INTO messages (conv_id, sender_id, text, timestamp) VALUES (?, ?, ?, ?)";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
	{
		sqlite3_bind_int(stmt, 1, conv_id);
		sqlite3_bind_int(stmt, 2, sender_uid);
		sqlite3_bind_text(stmt, 3, encrypted_text, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 4, time(NULL));
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	free(encrypted_text);
}

char *storage_get_history(uint32_t conv_id)
{
	char *buf = NULL;
	size_t len = 0;

	const char *sql = "SELECT u.username, m.text, m.timestamp FROM messages m "
										"JOIN users u ON m.sender_id = u.uid "
										"WHERE m.conv_id = ? ORDER BY m.timestamp ASC LIMIT 50";

	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return strdup("");

	sqlite3_bind_int(stmt, 1, conv_id);

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const char *user = (const char *)sqlite3_column_text(stmt, 0);
		const char *enc_text = (const char *)sqlite3_column_text(stmt, 1);

		char *decrypted_text = crypto_decrypt(enc_text);

		time_t rawtime = (time_t)sqlite3_column_int64(stmt, 2);
		struct tm *t = localtime(&rawtime);

		char line_header[64];
		snprintf(line_header, sizeof(line_header), "[%02d:%02d] %s: ", t->tm_hour, t->tm_min, user);

		size_t line_len = strlen(line_header) + strlen(decrypted_text) + 2; // +1 for \n, +1 for null (temp)

		char *new_buf = realloc(buf, len + line_len + 1);
		if (!new_buf)
		{
			// Allocation failed, stop here and return what we have
			break;
		}

		buf = new_buf;

		// Append
		sprintf(buf + len, "%s%s\n", line_header, decrypted_text);
		len += line_len;

		free(decrypted_text);
	}
	sqlite3_finalize(stmt);
	if (!buf)
		return strdup("");
	return buf;
}

int storage_is_admin(uint32_t conv_id, uint32_t uid)
{
	const char *sql = "SELECT role FROM participants WHERE conv_id = ? AND user_id = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_int(stmt, 1, conv_id);
	sqlite3_bind_int(stmt, 2, uid);
	int role = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		role = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return role == 1;
}

int storage_update_group(uint32_t conv_id, const char *name, const char *desc)
{
	const char *sql = "UPDATE conversations SET name = ?, description = ? WHERE conv_id = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, desc, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, conv_id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return 1;
}

int storage_add_participant(uint32_t conv_id, uint32_t uid, int role)
{
	// Check if exists first to avoid PK error? Or just use INSERT OR IGNORE
	const char *sql = "INSERT OR IGNORE INTO participants (conv_id, user_id, role) VALUES (?, ?, ?)";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_int(stmt, 1, conv_id);
	sqlite3_bind_int(stmt, 2, uid);
	sqlite3_bind_int(stmt, 3, role);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_DONE);
}

int storage_get_group_members(uint32_t conv_id, GroupMemberSummary *out_array, int max_count)
{
	const char *sql = "SELECT u.uid, u.username, p.role FROM participants p "
										"JOIN users u ON p.user_id = u.uid WHERE p.conv_id = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;

	sqlite3_bind_int(stmt, 1, conv_id);

	int count = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count)
	{
		out_array[count].uid = sqlite3_column_int(stmt, 0);
		strncpy(out_array[count].username, (char *)sqlite3_column_text(stmt, 1), MAX_NAME_LEN);
		out_array[count].role = sqlite3_column_int(stmt, 2);
		count++;
	}
	sqlite3_finalize(stmt);
	return count;
}

int storage_remove_participant(uint32_t conv_id, uint32_t uid)
{
	const char *sql = "DELETE FROM participants WHERE conv_id = ? AND user_id = ?";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
		return 0;
	sqlite3_bind_int(stmt, 1, conv_id);
	sqlite3_bind_int(stmt, 2, uid);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_DONE);
}

int storage_delete_conversation(uint32_t conv_id)
{
	// Transactional delete recommended, but simple sequential for now
	char *sql[] = {
			"DELETE FROM messages WHERE conv_id = ?",
			"DELETE FROM participants WHERE conv_id = ?",
			"DELETE FROM conversations WHERE conv_id = ?"};

	for (int i = 0; i < 3; i++)
	{
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql[i], -1, &stmt, 0) == SQLITE_OK)
		{
			sqlite3_bind_int(stmt, 1, conv_id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}
	return 1;
}