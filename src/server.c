#include "server.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>

static _Atomic unsigned int cli_count = 0;
static _Atomic unsigned int gr_count = 0;
static int uid = 10;

client_t* clients[MAX_GROUPS * GROUP_MAX_CLIENTS];
group_t* groups[MAX_GROUPS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    // Success
    printf("=== WELCOME TO THE CHATROOM ===\n");

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
void send_group(char* s, int uid, char* group_id) {
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
                if (write(gr->clients[i]->sockfd, s, strlen(s)) < 0) {
                    printf("ERROR: write to descriptor failed\n");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_user(char* s, int uid) {
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
}

// Check if NAME is used in GROUP_ID
// @return -1 if group not found, 1 if used, 0 if not used
int check_used_name(char* name, char* group_id) {
    group_t* gr = get_group(group_id);

    if (!gr) {
        printf("[System] Group not found\n");
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
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i]) {
            // printf("Checking: %s - %s\n", groups[i]->group_id, groups[i]->password);
            if (!strcmp(groups[i]->group_id, group_id) &&
                !strcmp(groups[i]->password, password)) {
                // printf("Found group\n");
                return i;
            }
        }
    }
    return -1;
}

// Search for group with GROUP_ID
// @return Found group or NULL
group_t* get_group(char* group_id) {
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i] && !strcmp(groups[i]->group_id, group_id)) {
            return groups[i];
        }
    }
    return NULL;
}

// Create a new group with PASSWORD
// @return NULL or new group's ID
char* create_group(char* password) {
    char* id = rand_string(GROUP_ID_LEN);
    group_t* gr = (group_t*)malloc(sizeof(group_t));
    gr->cli_count = 0;
    strcpy(gr->group_id, id);
    strcpy(gr->password, password);

    for (int i = 0; i < MAX_GROUPS; i++) {
        if (!groups[i]) {
            groups[i] = gr;
            gr->idx = i;
            gr_count++;
            printf("Created new group: %s - %s\n", groups[i]->group_id, groups[i]->password);
            return id;
        }
    }

    return NULL;
}

// Join client CL in group GROUP_ID
void join_group(char* group_id, client_t* cl) {
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i] && !strcmp(groups[i]->group_id, group_id)) {
            // printf("Found group\n");
            for (int j = 0; j < GROUP_MAX_CLIENTS; j++) {
                if (!groups[i]->clients[j]) {
                    groups[i]->clients[j] = cl;
                    groups[i]->cli_count++;
                    // printf("Joined group\n");
                    for (int k = 0; k < CLIENTS_MAX_GROUP; k++) {
                        if (!cl->groups[k]) {
                            strcpy(cl->groups[k], groups[i]->group_id);
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
}

// void switch_group(char* group_id, client_t* cl) {

// }

// Remove client CL from all joined groups
void leave_group(client_t* cl) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < cl->gr_count; i++) {
        // Remove member from room
        if (cl->groups[i]) {
            group_t* gr = get_group(cl->groups[i]);
            printf("Quit: %s\n", cl->groups[i]);
            for (int j = 0; j < gr->cli_count; j++) {
                if (gr->clients[j]) {
                    if (gr->clients[j]->uid == uid) {
                        gr->clients[j] = NULL;
                        gr->cli_count--;
                        break;
                    }
                }
            }
            // Remove room if empty
            if (gr->cli_count == 0) {
                printf("[System] Room ID %s removed\n", gr->group_id);
                groups[gr->idx] = NULL;
                gr_count--;

                free(gr);
            }
        }
    }
    // int count = 0;
    // for (int i = 0; i < GROUP_MAX_CLIENTS; i++) {
    //     if (count == gr->cli_count) {
    //         return 0;
    //     }
    //     if (gr->clients[i]) {
    //         count++;
    //         if (!strcmp(gr->clients[i]->name, name))
    //             return 1;
    //     }
    // }

    // group_t* target_group;
    // int idx = -1;

    // for (int i = 0; i < MAX_GROUPS; i++) {
    //     if (groups[i] && !strcmp(groups[i]->group_id, group_id)) {
    //         target_group = groups[i];
    //         idx = i;
    //         break;
    //     }
    // }

    // // Remove member from room
    // for (int i = 0; i < GROUP_MAX_CLIENTS; i++) {
    //     if (target_group->clients[i]) {
    //         if (target_group->clients[i]->uid == uid) {
    //             target_group->clients[i] = NULL;
    //             target_group->cli_count--;
    //             break;
    //         }
    //     }
    // }


    pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* arg) {
    char buffer[BUFFER_SZ];
    char name[NAME_LEN];
    // int gid;
    // char password[PASSWORD_LEN];
    int leave_flag = 0;
    // int res_code = 1;
    // int joined = 0;
    int joined_group = 0; // If user has joined a group
    char* cmd;
    char* param;
    char current_group[GROUP_ID_LEN] = {}; // Current target room

    client_t* cli = (client_t*)arg;
    bzero(buffer, BUFFER_SZ);

    // // Group ID and Password
    // if (recv(cli->sockfd, &argc, sizeof(int), 0) <= 0) {
    //     printf("ERROR: Cannot retrieve args info\n");
    //     leave_flag = 1;
    // }
    // else {
    //     // printf("Argc: %d\n", argc);
    //     if (recv(cli->sockfd, password, PASSWORD_LEN, 0) <= 0) {
    //         printf("ERROR: Cannot retrieve info\n");
    //     }
    //     else {
    //         // printf("Pass: %s\n", password);
    //         if (argc == 2) {
    //             // printf("Creating a new group...\n");
    //             char* id = create_group(password);
    //             // send_message(cli->sockfd, cli->group_id, GROUP_ID_LEN, 0);
    //             strcpy(cli->group_id, id);
    //             join_group(cli->group_id, cli);
    //             joined = 1;
    //             // printf("Group: %s\n", id);
    //         }
    //         else {
    //             if (recv(cli->sockfd, group_id, GROUP_ID_LEN, 0) <= 0) {
    //                 printf("ERROR: Cannot retrieve info\n");
    //                 leave_flag = 1;
    //             }
    //             else {
    //                 printf("ID: %s\n", group_id);
    //                 gid = check_group(group_id, password);
    //                 if (gid < 0) {
    //                     printf("ERROR: Wrong group info\n");
    //                     leave_flag = 1;
    //                 }
    //                 else {
    //                     // Check if room is full
    //                     if (groups[gid]->cli_count == GROUP_MAX_CLIENTS) {
    //                         printf("[System] Room full\n");
    //                         res_code = 0;
    //                         send(cli->sockfd, &res_code, sizeof(int), 0);
    //                         leave_flag = 1;
    //                     }
    //                     else {
    //                         strcpy(cli->group_id, group_id);
    //                         join_group(cli->group_id, cli);
    //                         joined = 1;
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }

    if (!leave_flag) {
        if (recv(cli->sockfd, name, NAME_LEN, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LEN) {
            printf("ERROR: Enter the name correctly\n");
            leave_flag = 1;
        }
        else {
            // int res = check_used_name(name, cli->group_id);
            // if (res == -1) {
            //     printf("An error occured. Please try again\n");
            //     leave_flag = 1;
            // }
            // else if (res == 1) {
            //     res_code = -1;
            //     send(cli->sockfd, &res_code, sizeof(int), 0);
            //     leave_flag = 1;
            // }
            // else {
            strcpy(cli->name, name);
            // send(cli->sockfd, &res_code, sizeof(int), 0);
            // sprintf(buffer, "-- %s has joined the chat --\n", cli->name);
            // printf("[System] User %s (uid: %d) joined group %s\n", cli->name, cli->uid, cli->group_id);
            // send_message(buffer, cli->uid, cli->group_id, 0);
            // sprintf(buffer, "[SERVER] Room ID: %s\n", cli->group_id);
            // send_message(buffer, cli->uid, cli->group_id, 1);
        // }
        }
    }

    // bzero(buffer, BUFFER_SZ);

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
            else if (!strcmp(buffer, ":create") || !strcmp(buffer, ":c")) {
                // printf("Someone requested to join\n");
                param = strtok(NULL, " \n");
                printf("Create request: %s\n", param);
                char* result = create_group(param);
                join_group(result, cli);
                sprintf(buffer, "[System] Group created. Group ID: %s", result);
                send_user(buffer, cli->uid);
                joined_group = 1;
                strcpy(current_group, result);
            }
            else if (!strcmp(buffer, ":join") || !strcmp(buffer, ":j")) {
                // printf("Someone requested to join\n");
                param = strtok(NULL, " \n");
                printf("Join request to %s\n", param);
                join_group(param, cli);
                joined_group = 1;
                strcpy(current_group, param);
            }
            else if (!strcmp(buffer, ":switch") || !strcmp(buffer, ":s")) {
                // printf("Someone requested to join\n");
                param = strtok(NULL, " \n");
                printf("Switch request to %s\n", param);
                // switch_group(param, cli);
                strcpy(current_group, param);
                sprintf(buffer, "[System] Switched to group %s", current_group);
                send_user(buffer, cli->uid);
            }
            else if (!strcmp(cmd, ":quit") || !strcmp(cmd, ":q")) {
                printf("Quit request\n");
                leave_flag = 1;
            }
            else if (!strcmp(cmd, ":info") || !strcmp(cmd, ":i")) {
                printf("Info request\n");
            }
            else if (!strcmp(cmd, ":rename") || !strcmp(cmd, ":r")) {
                param = strtok(NULL, " \n");
                printf("Rename request to %s\n", param);
                sprintf(buffer, "[System] Renamed %s to %s", cli->name, param);
                strcpy(cli->name, param);
                send_group(buffer, cli->uid, current_group);
            }
            else if (!strcmp(cmd, ":users") || !strcmp(cmd, ":u")) {
                printf("User List request\n");
            }
            else if (!strcmp(cmd, ":help") || !strcmp(cmd, ":h")) {
                printf("Help request\n");
            }
            else if (!strcmp(cmd, ":file") || !strcmp(cmd, ":f")) {
                printf("File request\n");
            }
            else if (!joined_group) {
                sprintf(buffer, "[System] You are not in a group");
                send_user(buffer, cli->uid);
                // str_trim_lf(buffer, strlen(buffer));
            }
            else {
                cmd[strlen(cmd)] = ' ';
                printf("%s\n", buffer);
                send_group(buffer, cli->uid, current_group);
                printf("[System] Group: %s, Message: %s\n", current_group, buffer);
                str_trim_lf(buffer, strlen(buffer));
            }
        }
        else {
            printf("[System] User %s (uid: %d) disconnected\n", cli->name, cli->uid);
            sprintf(buffer, "-- %s has left the chat --", cli->name);
            send_group(buffer, cli->uid, current_group);
            leave_flag = 1;
        }

        bzero(buffer, BUFFER_SZ);
    }
    // Close connection
    close(cli->sockfd);
    // Remove member from group 
    leave_group(cli);
    // Remove member from server
    leave_server(cli->uid);
    free(cli);
    pthread_detach(pthread_self());

    return NULL;
}