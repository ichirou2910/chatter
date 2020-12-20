#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

#define PORT 9000

#define ROOM_MAX_CLIENTS 100
#define ROOM_MAX_MESSAGES 1000
#define CLIENT_MAX_ROOMS 10
#define MAX_ROOMS 100
#define BUFFER_SZ 1024
#define NAME_LEN 32
#define ROOM_ID_LEN 7
#define ROOM_NAME_LEN 32
#define PASSWORD_LEN 32

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;

    char name[NAME_LEN];    // Username
    char room_ids[CLIENT_MAX_ROOMS][ROOM_ID_LEN];   // Joined rooms
    char room_names[CLIENT_MAX_ROOMS][ROOM_NAME_LEN];   // Joined groups' names
    char active_room[ROOM_ID_LEN];  // Current active group
    int rm_count;   // Room count
} client_t;

typedef struct {
    char messages[ROOM_MAX_MESSAGES][BUFFER_SZ];     // Max messages allocated for one group
    client_t* clients[ROOM_MAX_CLIENTS];   // Room members
    char room_id[ROOM_ID_LEN];            // Room ID
    char room_name[ROOM_NAME_LEN];        // Room name
    char password[PASSWORD_LEN];            // Room password
    char name[NAME_LEN];                    // Group's name
    int mes_count;                          // Messages count
    int cli_count;                          // Member count
    int idx;                                // Room index
} room_t;

// === SERVER FUNCTIONS ===
void join_server(client_t* cl);
void leave_server(int uid);
// ---

// === ROOM FUNCTIONS ===
char* create_room(char* password, char* name);
room_t* get_room(char* room_id);
int join_room(char* room_id, char* password, client_t* cl);
int switch_room(char* room_id, client_t* cl);
int leave_room(char* room_id, client_t* cl);
void leave_all_rooms(client_t* cl);
// ---

// === DATA PUBLISHING ===
void send_info_room(char* room_id, int uid);
void send_list_room(int uid);
void send_list_msg(char* room_id, int uid);
void send_user_info(char* name, int uid);
// ---

// === MESSAGING FUNCTION ===
void send_room(char* s, char* room_id);
void send_user(char* s, int uid);
void send_other(char* s, int uid, char* room_id);
void send_file(char* path, int uid, char* room_id);
// ---

// Client thread
void* handle_client(void* arg);

#endif
