#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

#define PORT 9000

#define GROUP_MAX_CLIENTS 100
#define GROUP_MAX_MESSAGES 1000
#define CLIENTS_MAX_GROUP 10
#define MAX_GROUPS 100
#define BUFFER_SZ 1024
#define NAME_LEN 32
#define GROUP_ID_LEN 7
#define PASSWORD_LEN 32

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;

    char name[NAME_LEN];                    // Username
    char groups[CLIENTS_MAX_GROUP][GROUP_ID_LEN];        // Joined groups
    char active_group[GROUP_ID_LEN];        // Current active group
    int gr_count;                           // Room count
} client_t;

typedef struct {
    char messages[GROUP_MAX_MESSAGES][BUFFER_SZ];     // Max messages allocated for one group
    client_t* clients[GROUP_MAX_CLIENTS];   // Room members
    char group_id[GROUP_ID_LEN];            // Room ID
    char password[PASSWORD_LEN];            // Room password
    int mes_count;                          // Messages count
    int cli_count;                          // Member count
    int idx;                                // Room index
} group_t;

// UTILITIES ===

void str_overwrite_stdout();
void str_trim_lf(char* arr, int length);
void print_ip_addr(struct sockaddr_in addr);
int check_used_name(char* name, char* group_id);

// ===
// SERVER FUNCTIONS ===

void join_server(client_t* cl);
void leave_server(int uid);

// ===
// GROUP FUNCTIONS ===

char* create_group(char* password);
int check_group(char* group_id, char* password);
group_t* get_group(char* group_id);
int join_group(char* group_id, char* password, client_t* cl);
void leave_all_groups(client_t* cl);
int leave_group(char* group_id, client_t* cl);
int switch_group(char* group_id, client_t* cl);
void return_lobby(client_t* cl);
void info_group(char* group_id, int uid);

// ===
// MESSAGING FUNCTION ===
void send_group(char* s, char* group_id);
void send_user(char* s, int uid);
void send_other(char* s, int uid, char* group_id);
void send_file(char* path, int uid, char* group_id);

// ===

void* handle_client(void* arg);

#endif
