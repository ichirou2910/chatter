#include "server.h"
#include "utils.h"

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
#include <libgen.h>

static _Atomic unsigned int cli_count = 0;
static _Atomic unsigned int rm_count = 0;
static int uid = 10;

time_t now;
struct tm* local;

client_t* clients[MAX_ROOMS * ROOM_MAX_CLIENTS];
room_t* rooms[MAX_ROOMS];

pthread_mutex_t clients_mutex;

int main() {
    // Generate seed
    srand(time(NULL));
    int port = PORT;

    time(&now);
    local = localtime(&now);

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
        // if (cli_count + 1 == ROOM_MAX_CLIENTS) {
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

        for (int i = 0; i < CLIENT_MAX_ROOMS; i++) {
            cl->room_ids[i][0] = '\0';
        }

        // Add client to queue
        join_server(cl);
        pthread_create(&tid, NULL, &handle_client, (void*)cl);

        // Reduce CPU usage
        sleep(1);

    }

    // free(clients);
    // free(rooms);
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

    for (int i = 0; i < ROOM_MAX_CLIENTS; i++) {
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

    for (int i = 0; i < ROOM_MAX_CLIENTS; i++) {
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
// Send message S to clients of room room_ID 
void send_room(char* s, char* room_id) {
    pthread_mutex_lock(&clients_mutex);

    room_t* rm = get_room(room_id);
    if (!rm) {
        printf("An error occured\n");
        return;
    }

    int count = rm->cli_count;
    for (int i = 0; i < ROOM_MAX_CLIENTS; i++) {
        if (count == 0) {
            break;
        }
        if (rm->clients[i]) {
            count--;
            if (!strcmp(rm->clients[i]->active_room, room_id) && write(rm->clients[i]->sockfd, s, strlen(s)) < 0) {
                printf("ERROR: write to descriptor failed\n");
                break;
            }
        }
    }

    // Add message to room storage
    time(&now);
    local = localtime(&now);
    char* archive = (char*)malloc(strlen(s) + 9);
    sprintf(archive, "%02d:%02d ~ %s", local->tm_hour, local->tm_min, s);
    strcpy(rm->messages[rm->mes_count++], archive);
    free(archive);

    pthread_mutex_unlock(&clients_mutex);
}

void send_user(char* s, int uid) {
    pthread_mutex_lock(&clients_mutex);

    // s[strlen(s)] = 0;
    for (int i = 0; i < ROOM_MAX_CLIENTS * MAX_ROOMS; i++) {
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

void send_other(char* s, int uid, char* room_id) {
    pthread_mutex_lock(&clients_mutex);

    room_t* rm = get_room(room_id);
    if (!rm) {
        printf("An error occured\n");
        return;
    }

    int count = rm->cli_count;
    for (int i = 0; i < ROOM_MAX_CLIENTS; i++) {
        if (count == 0) {
            break;
        }
        if (rm->clients[i]) {
            count--;
            if (rm->clients[i]->uid != uid) {
                if (!strcmp(rm->clients[i]->active_room, room_id) && write(rm->clients[i]->sockfd, s, strlen(s)) < 0) {
                    printf("ERROR: write to descriptor failed\n");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Send a file of PATH to the room
void send_file(char* path, int uid, char* room_id) {
    pthread_mutex_lock(&clients_mutex);

    char* filename = basename(path);

    // Cache receiver
    room_t* rm = get_room(room_id);
    if (!rm) {
        printf("An error occured\n");
        return;
    }

    int count = rm->cli_count;
    int idx[count - 1];
    int idx_cnt = 0;
    for (int i = 0; i < ROOM_MAX_CLIENTS; i++) {
        if (count == 0) {
            break;
        }
        if (rm->clients[i]) {
            count--;
            if (rm->clients[i]->uid != uid) {
                idx[idx_cnt++] = rm->clients[i]->sockfd;
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
    for (int i = 0; i < rm->cli_count - 1; i++) {
        sprintf(msg, "[FILE] %s", filename);
        write(idx[i], msg, strlen(msg));
        // sprintf(msg, "%ld", file_stat.st_size);
        // write(idx[i], msg, strlen(msg));
    }

    // Actually send file
    // int pck_cnt = 0;
    while ((bufLen = read(fd, fileBuf, BUFFER_SZ)) > 0) {
        // printf("%d - Current bufLen: %d\n", pck_cnt++, bufLen);
        for (int i = 0; i < rm->cli_count - 1; i++) {
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

// Send a message to room. Basically send_other but with formatting
// Check if NAME is used in room_ID
// @return -1 if room not found, 1 if used, 0 if not used
int check_used_name(char* name, char* room_id) {
    room_t* rm = get_room(room_id);

    if (!rm) {
        printf("[SYSTEM] room not found\n");
        return -1;
    }

    int count = 0;
    for (int i = 0; i < ROOM_MAX_CLIENTS; i++) {
        if (count == rm->cli_count) {
            return 0;
        }
        if (rm->clients[i]) {
            count++;
            if (!strcmp(rm->clients[i]->name, name))
                return 1;
        }
    }
    return 0;
}

// Check if a room with room_ID and PASSWORD exists
// @return room index if exist, -1 otherwise
int check_room(char* room_id, char* password) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i]) {
            // printf("Checking: %s - %s\n", rooms[i]->room_id, rooms[i]->password);
            if (!strcmp(rooms[i]->room_id, room_id) &&
                !strcmp(rooms[i]->password, password)) {
                // printf("Found room\n");
                pthread_mutex_unlock(&clients_mutex);
                return i;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1;
}

// Search for room with room_ID
// @return Found room or NULL
room_t* get_room(char* room_id) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i] && !strcmp(rooms[i]->room_id, room_id)) {
            room_t* res = rooms[i];
            return res;
        }
    }
    return NULL;
}

// Create a new room with PASSWORD
// @return NULL or new room's ID
char* create_room(char* password, char* name) {
    pthread_mutex_lock(&clients_mutex);

    // Truncate input
    name[ROOM_NAME_LEN] = 0;
    password[PASSWORD_LEN] = 0;

    char* id = rand_string(ROOM_ID_LEN);
    room_t* rm = (room_t*)malloc(sizeof(room_t));
    rm->cli_count = 0;
    rm->mes_count = 0;
    strcpy(rm->room_id, id);
    strcpy(rm->password, password);
    strcpy(rm->room_name, name);
    printf("Created Pass: \n");
    for (int i = 0; i < PASSWORD_LEN; i++) {
        printf("%d ", password[i]);
    }
    printf("\n");

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i]) {
            rooms[i] = rm;
            rm->idx = i;
            rm_count++;
            printf("[SYSTEM] Created new room: %s - %s - %s\n", id, password, name);
            pthread_mutex_unlock(&clients_mutex);
            return id;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

// Join client CL in room ROOM_ID
// @return 1 if success, 0 if wrong info, -1 if already joined 
int join_room(char* room_id, char* password, client_t* cl) {
    pthread_mutex_lock(&clients_mutex);

    // Truncate input
    room_id[ROOM_ID_LEN] = 0;
    password[PASSWORD_LEN] = 0;

    // printf("Join to ID: %s - %s\n", room_id, password);

    for (int i = 0; i < MAX_ROOMS; i++) {
        // if (rooms[i]) {
        //     printf("Name: %s, ID: %s, Pass: %s\n", rooms[i]->room_name, rooms[i]->room_id, rooms[i]->password);
        // }
        if (rooms[i] &&
            !strcmp(rooms[i]->room_id, room_id) &&
            !strcmp(rooms[i]->password, password)) {

            printf("Target room name: %s\n", rooms[i]->room_name);
            int idx = -1;
            int cnt = 0;
            int mem_cnt = rooms[i]->cli_count;
            // If room empty, add user to the 1st slot
            if (mem_cnt == 0) {
                idx = 0;
            }
            else {
                // printf("Found room\n");
                for (int j = 0; j < ROOM_MAX_CLIENTS; j++) {
                    // Store the 1st empty slot index     
                    if (!rooms[i]->clients[j] && idx == -1) {
                        idx = j;
                    }
                    if (cnt == mem_cnt)
                        break;
                    // If the user is already in the room, skip
                    else {
                        if (rooms[i]->clients[j]->uid == cl->uid) {
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
            rooms[i]->clients[idx] = cl;
            rooms[i]->cli_count++;
            // printf("Joined room\n");
            // Add room to user's joined rooms
            for (int k = 0; k < CLIENT_MAX_ROOMS; k++) {
                if (cl->room_ids[k][0] == 0) {
                    strcpy(cl->room_ids[k], rooms[i]->room_id);
                    strcpy(cl->room_names[k], rooms[i]->room_name);
                    // strcpy(cl->active_room, room_id); // Switch focus to newly joined room
                    cl->rm_count++;
                    pthread_mutex_unlock(&clients_mutex);
                    return 1;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    return 0;
}

// Switch focused room of CL to room_ID
// @return 1 if success, 0 if wrong id
int switch_room(char* room_id, client_t* cl) {
    int cnt = 0;
    for (int i = 0; i < CLIENT_MAX_ROOMS; i++) {
        // If target room exists
        if (!strcmp(cl->room_ids[i], room_id)) {
            strcpy(cl->active_room, room_id);
            return 1;
        }
        else if (cnt == cl->rm_count) {
            break;
        }
        else {
            cnt++;
        }
    }
    return 0;
}

// Return to lobby, not active in any room/chat
void return_lobby(client_t* cl) {
    strcpy(cl->active_room, "");
}

// Remove client CL from all joined rooms
void leave_all_rooms(client_t* cl) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < cl->rm_count; i++) {
        // Remove member from room
        if (cl->room_ids[i][0] != 0) {
            room_t* rm = get_room(cl->room_ids[i]);
            // printf("Quit: %s\n", cl->room_ids[i]);
            for (int j = 0; j < rm->cli_count; j++) {
                if (rm->clients[j]) {
                    if (rm->clients[j]->uid == cl->uid) {
                        rm->clients[j] = NULL;
                        rm->cli_count--;
                        break;
                    }
                }
            }
            // Remove room if empty
            if (rm->cli_count == 0) {
                printf("[SYSTEM] Room ID %s removed\n", rm->room_id);
                rooms[rm->idx] = NULL;
                rm_count--;

                free(rm);
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_info_room(char* room_id, int uid) {
    room_t* rm = get_room(room_id);

    // Allocate size for the message
    // Size = General info size + padding + (name size + padding) * mem count
    int size = 76 + PASSWORD_LEN + ROOM_ID_LEN + (NAME_LEN + 8) * rm->cli_count;
    char* buffer = (char*)malloc(size);

    sprintf(buffer, "[INFO] Room ID: %s\n===\nRoom info:\n- Password: %s\n- Members: %d\n===\nMembers info:\n", room_id, rm->password, rm->cli_count);

    int cnt = 0;
    int mem_cnt = rm->cli_count;
    char* user_info = (char*)malloc(NAME_LEN + 8);
    for (int i = 0; i < ROOM_MAX_CLIENTS; i++) {
        if (rm->clients[i]) {
            cnt++;
            sprintf(user_info, "- %d. %s\n", rm->clients[i]->uid, rm->clients[i]->name);
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
}

void send_list_room(int uid) {
    char* tmp = (char*)malloc(ROOM_ID_LEN + ROOM_NAME_LEN + 3);

    for (int i = 0; i < ROOM_MAX_CLIENTS * MAX_ROOMS; i++) {
        // Find client
        if (clients[i] && clients[i]->uid == uid) {
            client_t* cl = (client_t*)malloc(sizeof(client_t));
            cl = clients[i];
            // Concatenate list
            char* gr_list = (char*)malloc(9 + cl->rm_count * (ROOM_ID_LEN + ROOM_NAME_LEN + 2));
            sprintf(gr_list, "[ROOMS] ");
            if (cl->rm_count != 0) {
                int cnt = 0;
                int gr_cnt = cl->rm_count;
                for (int j = 0; j < CLIENT_MAX_ROOMS; j++) {
                    if (cnt == gr_cnt)
                        break;
                    if (cl->room_ids[j]) {
                        cnt++;
                        sprintf(tmp, "%s\n%s\n", cl->room_names[j], cl->room_ids[j]);
                        strcat(gr_list, tmp);
                        // bzero(tmp, ROOM_ID_LEN + ROOM_NAME_LEN + 2);
                    }
                }
            }
            // printf("List: %s", gr_list);
            send_user(gr_list, uid);
            free(gr_list);
            free(tmp);
        }
    }
}

void send_list_msg(char* room_id, int uid) {
    char* tmp = (char*)malloc(BUFFER_SZ + 1);

    // Find room
    room_t* rm = get_room(room_id);

    if (rm && rm->mes_count) {
        // Concatenate list
        int msg_cnt = rm->mes_count;
        char* msg_list = (char*)malloc(12 + msg_cnt * BUFFER_SZ);
        sprintf(msg_list, "[MESSAGES] ");
        if (msg_cnt) {
            for (int i = 0; i < msg_cnt; i++) {
                sprintf(tmp, "%s\n", rm->messages[i]);
                strcat(msg_list, tmp);
            }
        }
        send_user(msg_list, uid);
        free(msg_list);
    }
    else {
        send_user("[MESSAGES]", uid);
    }

    free(tmp);
}

// Leave a specific room
// @return 1 if success, 0 if room doesn't exist
int leave_room(char* room_id, client_t* cl) {
    pthread_mutex_lock(&clients_mutex);

    room_t* rm = get_room(room_id);

    if (!rm) {
        pthread_mutex_unlock(&clients_mutex);
        return 0;
    }

    for (int i = 0; i < CLIENT_MAX_ROOMS; i++) {
        if (!strcmp(cl->room_ids[i], rm->room_id)) {
            strcpy(cl->room_ids[i], "\0");
            strcpy(cl->room_names[i], "\0");
            break;
        }
    }

    int cnt = 0;
    for (int j = 0; j < ROOM_MAX_CLIENTS; j++) {
        if (cnt == rm->cli_count) {
            break;
        }
        if (rm->clients[j]) {
            cnt++;
            if (rm->clients[j]->uid == cl->uid) {
                rm->clients[j] = NULL;
                rm->cli_count--;
                cl->rm_count--;
                strcpy(cl->active_room, "");
                break;
            }
        }
    }
    // Remove room if empty
    if (rm->cli_count == 0) {
        printf("[SYSTEM] Room ID %s removed\n", rm->room_id);
        rooms[rm->idx] = NULL;
        rm_count--;

        free(rm);
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
        strcpy(cli->active_room, "");
        cli->rm_count = 0;
        printf("[SYSTEM] User %s (uid: %d) joined the server\n", cli->name, cli->uid);
    }

    // While stay connected to the chat
    while (1) {
        if (leave_flag) {
            break;
        }

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
            else if (!strcmp(cmd, ":c")) {
                // Get parameters
                param = strtok(NULL, " \n");
                char* password = (char*)malloc(PASSWORD_LEN * sizeof(char) + 1);
                strcpy(password, param);
                param = strtok(NULL, "\n");
                char* name = (char*)malloc(ROOM_NAME_LEN * sizeof(char) + 1);
                strcpy(name, param);

                if (password != NULL && name != NULL) {
                    char* result = create_room(password, name);
                    printf("Result Pass: \n");
                    for (int i = 0; i < PASSWORD_LEN; i++) {
                        printf("%d ", password[i]);
                    }
                    printf("\n");
                    join_room(result, password, cli);

                    // sprintf(buffer, "[SYSTEM] Room created and joined. room ID: %s", result);
                    // send_user(buffer, cli->uid);

                    send_list_room(cli->uid);
                    // send_list_msg(result, cli->uid);
                }
                else {
                    sprintf(buffer, "[SYSTEM] Please provide room password");
                    send_user(buffer, cli->uid);
                }
                free(password);
                free(name);
            }
            // :j - Join room
            else if (!strcmp(cmd, ":j")) {
                param = strtok(NULL, " \n");
                if (param == NULL) {
                    sprintf(buffer, "[SYSTEM] Not enough info provided");
                    send_user(buffer, cli->uid);
                    continue;
                }
                // printf("Join request to %s\n", param);
                char* pass = strtok(NULL, " \n");
                char* gid = (char*)malloc(ROOM_ID_LEN + 1);

                strcpy(gid, param);

                if (gid != NULL && pass != NULL) {
                    int res = join_room(gid, pass, cli);
                    if (res == 0) {
                        sprintf(buffer, "[SYSTEM] Invalid room info");
                        send_user(buffer, cli->uid);
                    }
                    else if (res == -1) {
                        sprintf(buffer, "[SYSTEM] You already joined this room");
                        send_user(buffer, cli->uid);
                    }
                    else if (res == 1) {
                        // sprintf(buffer, "[SYSTEM] Joined room: %s", gid);
                        // send_user(buffer, cli->uid);
                        sprintf(buffer, "[SYSTEM] %s joined in the chat", name);
                        send_other(buffer, cli->uid, gid);
                        send_list_room(cli->uid);
                        // send_list_msg(gid, cli->uid);
                    }
                }
                else {
                    sprintf(buffer, "[SYSTEM] Not enough info provided");
                    send_user(buffer, cli->uid);
                }
                free(gid);
            }
            // :s - Switch room
            else if (!strcmp(cmd, ":s")) {
                param = strtok(NULL, " \n");
                // printf("Switch request to %s\n", param);
                if (param != NULL) {
                    if (!strcmp(param, cli->active_room)) {
                        sprintf(buffer, "[SYSTEM] You are already in this room");
                        send_user(buffer, cli->uid);
                    }
                    else {
                        int res = switch_room(param, cli);
                        if (res) {
                            send_list_msg(cli->active_room, cli->uid);
                        }
                        else {
                            sprintf(buffer, "[SYSTEM] Invalid room id");
                            send_user(buffer, cli->uid);
                        }
                    }
                }
                else {
                    sprintf(buffer, "[SYSTEM] Not enough info provided");
                    send_user(buffer, cli->uid);
                }
            }
            // :r - Rename
            else if (!strcmp(cmd, ":rename") || !strcmp(cmd, ":r")) {
                if (!strcmp(cli->active_room, "")) {
                    sprintf(buffer, "[SYSTEM] You are not in a room");
                    send_user(buffer, cli->uid);
                }
                else {
                    param = strtok(NULL, " \n");
                    if (param != NULL) {
                        char* name = (char*)malloc(NAME_LEN + 1);
                        strcpy(name, param);
                        name[NAME_LEN] = 0; // Truncate
                        // printf("Rename request to %s\n", param);
                        sprintf(buffer, "[SYSTEM] Renamed %s to %s", cli->name, name);
                        strcpy(cli->name, name);
                        send_room(buffer, cli->active_room);
                        free(name);
                    }
                    else {
                        sprintf(buffer, "[SYSTEM] Not enough info provided");
                        send_user(buffer, cli->uid);
                    }
                }
            }
            // :q - Quit room
            else if (!strcmp(cmd, ":q")) {
                if (!strcmp(cli->active_room, "")) {
                    sprintf(buffer, "[SYSTEM] You are not in a room");
                    send_user(buffer, cli->uid);
                }
                else {
                    char* gid = (char*)malloc(NAME_LEN + 1);
                    strcpy(gid, cli->active_room);

                    sprintf(buffer, "[SYSTEM] %s left the room", cli->name);
                    send_other(buffer, cli->uid, gid);

                    leave_room(gid, cli);
                    send_list_room(cli->uid);
                    free(gid);
                }
            }
            // :f - Send file
            else if (!strcmp(cmd, ":file") || !strcmp(cmd, ":f")) {
                if (!strcmp(cli->active_room, "")) {
                    sprintf(buffer, "[SYSTEM] You are not in a room");
                    send_user(buffer, cli->uid);
                }
                else {
                    cmd[strlen(cmd)] = ' ';

                    // Send file
                    send_file(cmd + 3, cli->uid, cli->active_room);
                }
            }
            // :i - Get room info
            else if (!strcmp(cmd, ":info") || !strcmp(cmd, ":i")) {
                // printf("Info request\n");
                if (!strcmp(cli->active_room, "")) {
                    sprintf(buffer, "[SYSTEM] You are not in a room");
                    send_user(buffer, cli->uid);
                }
                else {
                    send_info_room(cli->active_room, cli->uid);
                }
            }
            // In case not in a room
            else if (!strcmp(cli->active_room, "")) {
                sprintf(buffer, "[SYSTEM] You are not in a room");
                send_user(buffer, cli->uid);
                // str_trim_lf(buffer, strlen(buffer));
            }
            // A normal message
            else {
                cmd[strlen(cmd)] = ' ';
                char* msg = (char*)malloc(BUFFER_SZ + NAME_LEN + 4);
                sprintf(msg, "[%s] %s", cli->name, buffer);
                send_room(msg, cli->active_room);

                printf("[SYSTEM] Room: %s, User: %s, Message: %s\n", cli->active_room, cli->name, buffer);
                str_trim_lf(buffer, strlen(buffer));

                free(msg);
            }
        }
        else {
            printf("[SYSTEM] User %s (uid: %d) disconnected\n", cli->name, cli->uid);
            if (strcmp(cli->active_room, "")) {
                sprintf(buffer, "[SYSTEM] %s disconnected", cli->name);
                send_other(buffer, cli->uid, cli->active_room);
            }
            leave_flag = 1;
        }

        bzero(buffer, BUFFER_SZ);
    }
    // Close connection
    close(cli->sockfd);
    // Remove member from room
    if (cli->rm_count)
        leave_all_rooms(cli);
    // Remove member from server
    leave_server(cli->uid);

    free(cli);
    pthread_detach(pthread_self());

    return NULL;
}