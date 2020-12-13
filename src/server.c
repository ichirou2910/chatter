#include "server.h"

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

static _Atomic unsigned int cli_count = 0;
static _Atomic unsigned int gr_count = 0;
static int uid = 10;

client_t* clients[MAX_GROUPS * GROUP_MAX_CLIENTS];
group_t* groups[MAX_GROUPS];

pthread_mutex_t clients_mutex;

static char* rand_string(size_t size) {
    char* str = malloc(size + 1);
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if (str && size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int)(sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

int main() {
    // Generate seed
    srand(time(NULL));
    int port = PORT;

    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    // Socket settings
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    // serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // Signals
    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)) < 0) {
        printf("ERROR: setsockopt\n");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");exit(EXIT_FAILURE);
    }

    // Bind
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("ERROR: bind\n");
        return EXIT_FAILURE;
    }

    // Listen
    if (listen(listenfd, 10) < 0) {
        printf("ERROR: listen\n");
        return EXIT_FAILURE;
    }

    pthread_mutex_init(&clients_mutex, NULL);

    // Success
    printf("=== CHATTER MONITOR ===\n");

    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        // Check for max clients
        // if (cli_count + 1 == GROUP_MAX_CLIENTS) {
        //     printf("Max clients connection reached. Connection rejected\n");
        //     print_ip_addr(cli_addr);
        //     close(connfd);
        //     continue;
        // }


        // Client Settings
        client_t* cl = (client_t*)malloc(sizeof(client_t));
        cl->address = cli_addr;
        cl->sockfd = connfd;
        cl->uid = uid++;

        for (int i = 0; i < CLIENTS_MAX_GROUP; i++) {
            cl->groups[i][0] = '\0';
        }

        // Add client to queue
        join_server(cl);
        pthread_create(&tid, NULL, &handle_client, (void*)cl);

        // Reduce CPU usage
        sleep(1);

    }

    return EXIT_SUCCESS;
}


void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf(char* arr, int length) {
    for (int i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

// Join the server
// @param cl Target client
void join_server(client_t* cl) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < GROUP_MAX_CLIENTS; i++) {
        if (!clients[i]) {
            clients[i] = cl;
            cli_count++;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Leave the server
// @param uid Target user ID
void leave_server(int uid) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < GROUP_MAX_CLIENTS; i++) {
        if (clients[i]) {
            if (clients[i]->uid == uid) {
                clients[i] = NULL;
                cli_count--;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Print IP Address, just in case needed
void print_ip_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d", addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// Handle message sending to clients
// Send message S to clients of room GROUP_ID 
void send_group(char* s, char* group_id) {
    pthread_mutex_lock(&clients_mutex);

    group_t* gr = get_group(group_id);
    if (!gr) {
        printf("An error occured\n");
        return;
    }

    int count = gr->cli_count;
    for (int i = 0; i < GROUP_MAX_CLIENTS; i++) {
        if (count == 0) {
            break;
        }
        if (gr->clients[i]) {
            count--;
            if (!strcmp(gr->clients[i]->active_group, group_id) && write(gr->clients[i]->sockfd, s, strlen(s)) < 0) {
                printf("ERROR: write to descriptor failed\n");
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_user(char* s, int uid) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < GROUP_MAX_CLIENTS * MAX_GROUPS; i++) {
        // Prevent looping through all 100 clients
        if (clients[i]->uid == uid) {
            if (write(clients[i]->sockfd, s, strlen(s)) < 0) {
                printf("ERROR: write to descriptor failed\n");
                break;
            }
            else {
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_other(char* s, int uid, char* group_id) {
    pthread_mutex_lock(&clients_mutex);

    group_t* gr = get_group(group_id);
    if (!gr) {
        printf("An error occured\n");
        return;
    }

    int count = gr->cli_count;
    for (int i = 0; i < GROUP_MAX_CLIENTS; i++) {
        if (count == 0) {
            break;
        }
        if (gr->clients[i]) {
            count--;
            if (gr->clients[i]->uid != uid) {
                if (!strcmp(gr->clients[i]->active_group, group_id) && write(gr->clients[i]->sockfd, s, strlen(s)) < 0) {
                    printf("ERROR: write to descriptor failed\n");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Send a file of PATH to the group
void send_file(char* path, int uid, char* group_id) {
    pthread_mutex_lock(&clients_mutex);

    // Temporary file name, will implement proper file name extractor
    char* filename = "usagyuun.png";

    // Cache receiver
    group_t* gr = get_group(group_id);
    if (!gr) {
        printf("An error occured\n");
        return;
    }

    int count = gr->cli_count;
    int idx[count - 1];
    int idx_cnt = 0;
    for (int i = 0; i < GROUP_MAX_CLIENTS; i++) {
        if (count == 0) {
            break;
        }
        if (gr->clients[i]) {
            count--;
            if (gr->clients[i]->uid != uid) {
                idx[idx_cnt++] = gr->clients[i]->sockfd;
            }
        }
    }

    // Initialize
    int fd = open(path, O_RDONLY);
    if (!fd) {
        printf("File not found\n");
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    struct stat file_stat;
    fstat(fd, &file_stat);
    char fileBuf[BUFFER_SZ];
    memset(fileBuf, 0x0, BUFFER_SZ);
    int bufLen = 0;

    // Notify of file transmission
    char msg[15 + strlen(filename) + 1];
    for (int i = 0; i < gr->cli_count - 1; i++) {
        sprintf(msg, "[SYSTEM] File: %s", filename);
        write(idx[i], msg, strlen(msg));
        // sprintf(msg, "%ld", file_stat.st_size);
        // write(idx[i], msg, strlen(msg));
    }

    // Actually send file
    // int pck_cnt = 0;
    while ((bufLen = read(fd, fileBuf, BUFFER_SZ)) > 0) {
        // printf("%d - Current bufLen: %d\n", pck_cnt++, bufLen);
        for (int i = 0; i < gr->cli_count - 1; i++) {
            write(idx[i], fileBuf, bufLen);
        }
        if (bufLen == 0 || bufLen != BUFFER_SZ) {
            break;
        }
        memset(fileBuf, 0x0, BUFFER_SZ);
    }
    printf("File sent\n");
    close(fd);
    printf("File closed\n");

    pthread_mutex_unlock(&clients_mutex);
}

// Send a message to group. Basically send_other but with formatting
// Check if NAME is used in GROUP_ID
// @return -1 if group not found, 1 if used, 0 if not used
int check_used_name(char* name, char* group_id) {
    group_t* gr = get_group(group_id);

    if (!gr) {
        printf("[SYSTEM] Group not found\n");
        return -1;
    }

    int count = 0;
    for (int i = 0; i < GROUP_MAX_CLIENTS; i++) {
        if (count == gr->cli_count) {
            return 0;
        }
        if (gr->clients[i]) {
            count++;
            if (!strcmp(gr->clients[i]->name, name))
                return 1;
        }
    }
    return 0;
}

// Check if a group with GROUP_ID and PASSWORD exists
// @return Group index if exist, -1 otherwise
int check_group(char* group_id, char* password) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i]) {
            // printf("Checking: %s - %s\n", groups[i]->group_id, groups[i]->password);
            if (!strcmp(groups[i]->group_id, group_id) &&
                !strcmp(groups[i]->password, password)) {
                // printf("Found group\n");
                pthread_mutex_unlock(&clients_mutex);
                return i;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1;
}

// Search for group with GROUP_ID
// @return Found group or NULL
group_t* get_group(char* group_id) {
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i] && !strcmp(groups[i]->group_id, group_id)) {
            group_t* res = groups[i];
            return res;
        }
    }
    return NULL;
}

// Create a new group with PASSWORD
// @return NULL or new group's ID
char* create_group(char* name, char* password) {
    pthread_mutex_lock(&clients_mutex);

    char* id = rand_string(GROUP_ID_LEN);
    group_t* gr = (group_t*)malloc(sizeof(group_t));
    gr->cli_count = 0;
    gr->mes_count = 0;
    strcpy(gr->group_id, id);
    strcpy(gr->name, name);
    strcpy(gr->password, password);

    for (int i = 0; i < MAX_GROUPS; i++) {
        if (!groups[i]) {
            groups[i] = gr;
            gr->idx = i;
            gr_count++;
            printf("[SYSTEM] Created new group: %s - %s - %s\n", id, name, password);
            pthread_mutex_unlock(&clients_mutex);
            return id;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

// Join client CL in group GROUP_ID
// @return 1 if success, 0 if wrong info, -1 if already joined 
int join_group(char* group_id, char* password, client_t* cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i] &&
            !strcmp(groups[i]->group_id, group_id) &&
            !strcmp(groups[i]->password, password)) {


            int idx = -1;
            int cnt = 0;
            int mem_cnt = groups[i]->cli_count;
            // If group empty, add user to the 1st slot
            if (mem_cnt == 0) {
                idx = 0;
            }
            else {
                // printf("Found group\n");
                for (int j = 0; j < GROUP_MAX_CLIENTS; j++) {
                    // Store the 1st empty slot index     
                    if (!groups[i]->clients[j] && idx == -1) {
                        idx = j;
                    }
                    if (cnt == mem_cnt)
                        break;
                    // If the user is already in the room, skip
                    else {
                        if (groups[i]->clients[j]->uid == cl->uid) {
                            pthread_mutex_unlock(&clients_mutex);
                            return -1;
                        }
                        else {
                            cnt++;
                        }
                    }
                }
            }
            // Add user to the previously stored slot
            groups[i]->clients[idx] = cl;
            groups[i]->cli_count++;
            // printf("Joined group\n");
            // Add group to user's joined groups
            for (int k = 0; k < CLIENTS_MAX_GROUP; k++) {
                if (cl->groups[k][0] == 0) {
                    strcpy(cl->groups[k], groups[i]->group_id);
                    strcpy(cl->active_group, group_id); // Switch focus to newly joined group
                    cl->gr_count++;
                    pthread_mutex_unlock(&clients_mutex);
                    return 1;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    return 0;
}

// Switch focused group of CL to GROUP_ID
// @return 1 if success, 0 if wrong id
int switch_group(char* group_id, client_t* cl) {
    int cnt = 0;
    for (int i = 0; i < CLIENTS_MAX_GROUP; i++) {
        // If target room exists
        if (!strcmp(cl->groups[i], group_id)) {
            strcpy(cl->active_group, group_id);
            return 1;
        }
        else if (cnt == cl->gr_count) {
            break;
        }
        else {
            cnt++;
        }
    }
    return 0;
}

// Return to lobby, not active in any group/chat
void return_lobby(client_t* cl) {
    strcpy(cl->active_group, "");
}

// Remove client CL from all joined groups
void leave_all_groups(client_t* cl) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < cl->gr_count; i++) {
        // Remove member from room
        if (cl->groups[i][0] != 0) {
            group_t* gr = get_group(cl->groups[i]);
            // printf("Quit: %s\n", cl->groups[i]);
            for (int j = 0; j < gr->cli_count; j++) {
                if (gr->clients[j]) {
                    if (gr->clients[j]->uid == cl->uid) {
                        gr->clients[j] = NULL;
                        gr->cli_count--;
                        break;
                    }
                }
            }
            // Remove room if empty
            if (gr->cli_count == 0) {
                printf("[SYSTEM] Room ID %s removed\n", gr->group_id);
                groups[gr->idx] = NULL;
                gr_count--;

                free(gr);
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void info_group(char* group_id, int uid) {
    group_t* gr = get_group(group_id);

    // Allocate size for the message
    // Size = General info size + padding + (name size + padding) * mem count
    int size = 77 + PASSWORD_LEN + GROUP_ID_LEN + (NAME_LEN + 8) * gr->cli_count;
    char* buffer = (char*)malloc(size);

    sprintf(buffer, "[SYSTEM] Room ID: %s\n===\nRoom info:\n- Password: %s\n- Members: %d\n===\nMembers info:\n", group_id, gr->password, gr->cli_count);
    // printf("%ld\n", strlen(buffer));

    int cnt = 0;
    int mem_cnt = gr->cli_count;
    char* user_info = (char*)malloc(NAME_LEN + 7);
    for (int i = 0; i < GROUP_MAX_CLIENTS; i++) {
        if (gr->clients[i]) {
            cnt++;
            sprintf(user_info, "- %d. %s\n", gr->clients[i]->uid, gr->clients[i]->name);
            // send_user(buffer, uid);
            strcat(buffer, user_info);
        }
        else if (cnt == mem_cnt) {
            break;
        }
    }
    send_user(buffer, uid);
    free(user_info);
    free(buffer);
    /*
       [SYSTEM] Room ID: === Room info: - Password: - Members: 100 === Members info:
    */
}

// Leave a specific room
// @return 1 if success, 0 if room doesn't exist
int leave_group(char* group_id, client_t* cl) {
    pthread_mutex_lock(&clients_mutex);

    group_t* gr = get_group(group_id);

    if (!gr) {
        pthread_mutex_unlock(&clients_mutex);
        return 0;
    }

    int cnt = 0;
    for (int j = 0; j < GROUP_MAX_CLIENTS; j++) {
        if (cnt == gr->cli_count) {
            break;
        }
        if (gr->clients[j]) {
            cnt++;
            if (gr->clients[j]->uid == cl->uid) {
                gr->clients[j] = NULL;
                gr->cli_count--;
                cl->gr_count--;
                strcpy(cl->active_group, "");
                break;
            }
        }
    }
    // Remove room if empty
    if (gr->cli_count == 0) {
        printf("[SYSTEM] Room ID %s removed\n", gr->group_id);
        groups[gr->idx] = NULL;
        gr_count--;

        free(gr);
    }

    pthread_mutex_unlock(&clients_mutex);
    return 1;
}


void* handle_client(void* arg) {
    char buffer[BUFFER_SZ];
    char name[NAME_LEN];
    int leave_flag = 0;
    char* cmd;
    char* param;

    client_t* cli = (client_t*)arg;
    bzero(buffer, BUFFER_SZ);
    bzero(name, NAME_LEN);

    if (recv(cli->sockfd, name, NAME_LEN, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LEN) {
        printf("ERROR: Enter the name correctly\n");
        leave_flag = 1;
    }
    else {
        strcpy(cli->name, name);
        strcpy(cli->active_group, "");
        cli->gr_count = 0;
        printf("[SYSTEM] User %s (uid: %d) joined the server\n", cli->name, cli->uid);
    }

    // While stay connected to the chat
    while (!leave_flag) {
        int receive = recv(cli->sockfd, buffer, BUFFER_SZ, 0);
        // printf("%s\n", buffer);
        // for (int i = 0; i < receive; i++) {
        //     printf("%d ", buffer[i]);
        // }
        // printf("\n");

        if (receive > 0) {
            buffer[strlen(buffer)] = 0;

            // Extract command
            cmd = strtok(buffer, " \n");

            if (cmd == NULL) {
                printf("No command\n");
            }
            // :c - Create room
            else if (!strcmp(buffer, ":create") || !strcmp(buffer, ":c")) {
                // printf("Someone requested to join\n");
                param = strtok(NULL, " \n");
                if (param != NULL) {
                    char *pass = strtok(NULL, " \n");
                    char *name = (char*)malloc(NAME_LEN + 1);
                    strcpy(name, param);

                    char* result = create_group(name, pass);
                    join_group(result, pass, cli);

                    sprintf(buffer, "[SYSTEM] Group created and joined. Group ID: %s", result);
                    send_user(buffer, cli->uid);
                }
                else {
                    sprintf(buffer, "[SYSTEM] Please provide group password");
                    send_user(buffer, cli->uid);
                }
                // printf("Create request: %s\n", param);
            }
            // :j - Join room
            else if (!strcmp(buffer, ":join") || !strcmp(buffer, ":j")) {
                param = strtok(NULL, " \n");
                if (param == NULL) {
                    sprintf(buffer, "[SYSTEM] Not enough info provided");
                    send_user(buffer, cli->uid);
                    continue;
                }
                // printf("Join request to %s\n", param);
                char* pass = strtok(NULL, " \n");
                char* gid = (char*)malloc(GROUP_ID_LEN + 1);

                strcpy(gid, param);

                if (gid != NULL && pass != NULL) {
                    int res = join_group(gid, pass, cli);
                    if (res == 0) {
                        sprintf(buffer, "[SYSTEM] Invalid room info");
                        send_user(buffer, cli->uid);
                    }
                    else if (res == -1) {
                        sprintf(buffer, "[SYSTEM] You already joined this room");
                        send_user(buffer, cli->uid);
                    }
                    else if (res == 1) {
                        sprintf(buffer, "[SYSTEM] Joined group: %s", gid);
                        send_user(buffer, cli->uid);
                        sprintf(buffer, "[SYSTEM] %s joined in the chat", cli->name);
                        send_other(buffer, cli->uid, gid);
                    }
                }
                else {
                    sprintf(buffer, "[SYSTEM] Not enough info provided");
                    send_user(buffer, cli->uid);
                }
                free(gid);
            }
            // :s - Switch room
            else if (!strcmp(buffer, ":switch") || !strcmp(buffer, ":s")) {
                param = strtok(NULL, " \n");
                // printf("Switch request to %s\n", param);
                if (param != NULL) {
                    if (!strcmp(param, cli->active_group)) {
                        sprintf(buffer, "[SYSTEM] You are already in this group");
                        send_user(buffer, cli->uid);
                    }
                    else {
                        int res = switch_group(param, cli);
                        if (res) {
                            sprintf(buffer, "[SYSTEM] Switched to group %s", cli->active_group);
                            send_user(buffer, cli->uid);
                        }
                        else {
                            sprintf(buffer, "[SYSTEM] Invalid group id");
                            send_user(buffer, cli->uid);
                        }
                    }
                }
                else {
                    sprintf(buffer, "[SYSTEM] Not enough info provided");
                    send_user(buffer, cli->uid);
                }
            }
            // :l - Go to lobby
            else if (!strcmp(cmd, ":lobby") || !strcmp(cmd, ":l")) {
                // printf("Return to lobby request\n");
                if (!strcmp(cli->active_group, "")) {
                    sprintf(buffer, "[SYSTEM] You are already in the lobby");
                    send_user(buffer, cli->uid);
                }
                else {
                    return_lobby(cli);
                }
            }
            // :r - Rename
            else if (!strcmp(cmd, ":rename") || !strcmp(cmd, ":r")) {
                // if (!strcmp(cli->active_group, "")) {
                //     sprintf(buffer, "[SYSTEM] You are not in a group");
                //     send_user(buffer, cli->uid);
                // }
                param = strtok(NULL, " \n");
                if (param != NULL) {
                    char* name = (char*)malloc(NAME_LEN);
                    strcpy(name, param);
                    // printf("Rename request to %s\n", param);
                    sprintf(buffer, "[SYSTEM] Renamed %s to %s", cli->name, name);
                    strcpy(cli->name, name);
                    if(!strcmp(cli->active_group, ""))
                        send_user(buffer, cli->uid);
                    else 
                        send_group(buffer, cli->active_group);
                    free(name);
                }
                else {
                    sprintf(buffer, "[SYSTEM] Not enough info provided");
                    send_user(buffer, cli->uid);
                }
            }
            // :q - Quit room
            else if (!strcmp(cmd, ":quit") || !strcmp(cmd, ":q")) {
                if (!strcmp(cli->active_group, "")) {
                    sprintf(buffer, "[SYSTEM] You are not in a group");
                    send_user(buffer, cli->uid);
                }
                else {
                    char* gid = (char*)malloc(NAME_LEN);
                    strcpy(gid, cli->active_group);

                    sprintf(buffer, "[SYSTEM] %s left the room", cli->name);
                    send_other(buffer, cli->uid, gid);
                    sprintf(buffer, "[SYSTEM] Left room %s", gid);
                    send_user(buffer, cli->uid);

                    leave_group(gid, cli);
                    free(gid);
                }
            }
            // :f - Send file
            else if (!strcmp(cmd, ":file") || !strcmp(cmd, ":f")) {
                if (!strcmp(cli->active_group, "")) {
                    sprintf(buffer, "[SYSTEM] You are not in a group");
                    send_user(buffer, cli->uid);
                }
                else {
                    //Get file's path
                    param = strtok(NULL, " \n");
                    printf("%s\n", param);
                    // Send file
                    send_file(param, cli->uid, cli->active_group);
                }
            }
            // :i - Get room info
            else if (!strcmp(cmd, ":info") || !strcmp(cmd, ":i")) {
                // printf("Info request\n");
                if (!strcmp(cli->active_group, "")) {
                    sprintf(buffer, "[SYSTEM] You are not in a group");
                    send_user(buffer, cli->uid);
                }
                else {
                    info_group(cli->active_group, cli->uid);
                }
            }
            // In case not in a group
            else if (!strcmp(cli->active_group, "")) {
                sprintf(buffer, "[SYSTEM] You are not in a group");
                send_user(buffer, cli->uid);
                // str_trim_lf(buffer, strlen(buffer));
            }
            // A normal message
            else {
                cmd[strlen(cmd)] = ' ';
                char* msg = (char*)malloc(BUFFER_SZ + NAME_LEN + 3);
                sprintf(msg, "[%s] %s", cli->name, buffer);
                send_other(msg, cli->uid, cli->active_group);

                printf("[SYSTEM] Group: %s, User: %s, Message: %s\n", cli->active_group, cli->name, buffer);
                str_trim_lf(buffer, strlen(buffer));

                free(msg);
            }
        }
        else {
            printf("[SYSTEM] User %s (uid: %d) disconnected\n", cli->name, cli->uid);
            if (strcmp(cli->active_group, "")) {
                sprintf(buffer, "[SYSTEM] %s disconnected", cli->name);
                send_other(buffer, cli->uid, cli->active_group);
            }
            leave_flag = 1;
        }

        bzero(buffer, BUFFER_SZ);
    }
    // Close connection
    close(cli->sockfd);
    // Remove member from group
    if (cli->gr_count)
        leave_all_groups(cli);
    // Remove member from server
    leave_server(cli->uid);

    free(cli);
    pthread_detach(pthread_self());

    return NULL;
}