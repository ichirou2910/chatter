#include "client.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[NAME_LEN];
time_t now;
struct tm* local;

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

void catch_ctrl_c_and_exit() {
    flag = 1;
}

void recv_msg_handler() {
    char message[BUFFER_SZ] = {};

    while (1) {
        int receive = recv(sockfd, message, BUFFER_SZ, 0);

        if (receive > 0) {
            time(&now);
            local = localtime(&now);
            printf("%02d:%02d ~ %s\n", local->tm_hour, local->tm_min, message);
            str_overwrite_stdout();
        }
        else if (receive == 0) {
            break;
        }
        bzero(message, BUFFER_SZ);
    }
}

void send_msg_handler() {
    char buffer[BUFFER_SZ] = {};

    while (1) {
        str_overwrite_stdout();
        fgets(buffer, BUFFER_SZ, stdin);
        str_trim_lf(buffer, BUFFER_SZ);

        if (strcmp(buffer, "exit") == 0) {
            break;
        }
        else {
            time(&now);
            local = localtime(&now);
            // sprintf(message, "%s\n", buffer);
            send(sockfd, buffer, strlen(buffer), 0);
        }
        bzero(buffer, BUFFER_SZ);
        // bzero(message, BUFFER_SZ + NAME_LEN);
    }
    catch_ctrl_c_and_exit();
}

int main(int argc, char const* argv[]) {
    const char* ip = SERVER_IP;
    int port = PORT;
    // if (argc < 2) {
    //     printf("Usage: %s password [group_id]\n", argv[0]);
    //     printf("If provided group_id, join room with password and group_id\n");
    //     printf("If not provided group_id, create new room with password\n");
    //     return EXIT_FAILURE;
    // }

    time(&now);
    local = localtime(&now);

    signal(SIGINT, catch_ctrl_c_and_exit);

    printf("Enter your name: ");
    fgets(name, NAME_LEN, stdin);
    str_trim_lf(name, strlen(name));

    if (strlen(name) > NAME_LEN || strlen(name) < 2) {
        printf("ERROR: Enter name correctly\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;

    // Socket Settings
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    // Connect to the server
    int err = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (err == -1) {
        printf("ERROR: connect\n");
        return EXIT_FAILURE;
    }

    // Send the info
    // send(sockfd, &argc, sizeof(int), 0);
    // send(sockfd, argv[1], PASSWORD_LEN, 0);
    // if (argv[2])
    //     send(sockfd, argv[2], GROUP_ID_LEN, 0);
    send(sockfd, name, NAME_LEN, 0);

    // Receive code from server
    // 1 - Accepted
    // -1 - Used name
    // 0 - Room full
    // int res_code = 0;
    // if (recv(sockfd, &res_code, sizeof(int), 0) <= 0) {
    //     printf("An error occured\n");
    //     flag = 1;
    // }
    // else {
    // }

    // if (res_code == -1) {
    //     printf("Name already taken\n");
    //     flag = 1;
    // }
    // else if (res_code == 0) {
    //     printf("This room does not exist or is full\n");
    //     flag = 1;
    // }
    // else {
    printf("=== WELCOME TO THE CHATTER ===\n");
    printf("Current time: %s", ctime(&now));

    // Thread for sending the messages
    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void*)send_msg_handler, NULL) != 0) {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    // Thread for receiving messages
    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void*)recv_msg_handler, NULL) != 0) {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }
    // }


    while (1) {
        if (flag) {
            printf("\nGoodbye\n");
            break;
        }
    }

    close(sockfd);

    return EXIT_SUCCESS;
}
