#include "client.h"

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>

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
        if (!strncmp(message, "[SYSTEM] File: ", 15)) {
            // Print file notification
            time(&now);
            local = localtime(&now);
            printf("%02d:%02d ~ %s\n", local->tm_hour, local->tm_min, message);
            // get the file name only
            // memmove(message, message + 15, strlen(message) - 15);
            // Initialize
            int fd = open(message + 15, O_WRONLY | O_CREAT | O_EXCL, 0700);
            char fileBuf[BUFFER_SZ];
            memset(fileBuf, 0x0, BUFFER_SZ);
            int bufLen = 0;
            // int pck_cnt = 0;

            // Get file size
            // char tmpBuf[BUFFER_SZ];
            // recv(sockfd, tmpBuf, BUFFER_SZ, 0);
            // long file_size = atol(tmpBuf);
            // printf("File size: %ld\n", file_size);

            while ((bufLen = read(sockfd, fileBuf, BUFFER_SZ)) > 0) {
                int write_sz = write(fd, fileBuf, bufLen);
                memset(fileBuf, 0x0, BUFFER_SZ);
                // file_size -= (long)bufLen;
                // printf("%d - Data left: %ld\n", pck_cnt++, file_size);
                if (write_sz < bufLen) {
                    break;
                }
                if (bufLen == 0 || bufLen != BUFFER_SZ) {
                    break;
                }
            }
            close(fd);
            printf("[SYSTEM] File received\n");
            str_overwrite_stdout();
            // continue;
        }
        else {
            if (receive > 0) {
                time(&now);
                local = localtime(&now);
                printf("%02d:%02d ~ %s\n", local->tm_hour, local->tm_min, message);
                str_overwrite_stdout();
            }
            else if (receive == 0) {
                break;
            }
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

        if (!strcmp(buffer, ":help") || !strcmp(buffer, ":h")) {
            printf("Chatter commands:\n");
            printf("- :c | :create <password>     - Create a new room with <password>\n");
            printf("- :j | :join <id> <password>  - Join a room with <id> & <password>\n");
            printf("- :s | :switch <id>           - Switch to room with <id>\n");
            printf("- :l | :leave                 - Temporary leave room and return to lobby\n");
            printf("- :r | :rename <name>         - Rename self to <name>\n");
            printf("- :q | :quit                  - Quit current room and return to lobby\n");
            printf("- :f | :file <filename>       - Send file with <filename> to roommate\n");
            printf("- :i | :info                  - Print room info\n");
            printf("===\n");
            printf("Press Ctrl+C to quit Chatter\n");
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

// int main(int argc, char const* argv[]) {
int main() {
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
    else if (!strcmp(name, "SYSTEM")) {
        printf("ERROR: Reserved name. Please use another");
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

    send(sockfd, name, NAME_LEN, 0);

    printf("=== WELCOME TO CHATTER ===\n");
    printf("Current time: %s", ctime(&now));
    printf("Type :h or :help for Chatter commands\n");

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
