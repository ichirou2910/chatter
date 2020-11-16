#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

#define PORT 9000

#define GROUP_MAX_CLIENTS 100
#define CLIENTS_MAX_GROUP 10
#define MAX_GROUPS 100
#define BUFFER_SZ 2048
#define NAME_LEN 32
#define GROUP_ID_LEN 7
#define PASSWORD_LEN 32

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[NAME_LEN];
    char group_id[GROUP_ID_LEN];
    char* groups[CLIENTS_MAX_GROUP];
} client_t;

typedef struct {
    char group_id[GROUP_ID_LEN];
    char password[PASSWORD_LEN];
    client_t* clients[GROUP_MAX_CLIENTS];
    int cli_count;
} group_t;

void str_overwrite_stdout();
void str_trim_lf(char* arr, int length);
void print_ip_addr(struct sockaddr_in addr);
int check_used_name(char* name, char* group_id);

void join_server(client_t* cl);
void leave_server(int uid);

char* create_group(char* password);
int check_group(char* group_id, char* password);
group_t* get_group(char* group_id);
void join_group(char* group_id, client_t* cl);
void leave_group(char* group_id, int uid);

void send_message(char* s, int uid, char* group_id, int mode);

void* handle_client(void* arg);

#endif
