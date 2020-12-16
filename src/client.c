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
#include <locale.h>

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[NAME_LEN];
time_t now;
struct tm* local;

WINDOW* chatbox, * chat_field;
WINDOW* input, * input_field;
int maxX, maxY;
int curX, curY, curH;
int padX, padY;
int chat_mode = 0;
int ch;

int main() {
    const char* ip = SERVER_IP;
    int port = PORT;

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

    setlocale(LC_ALL, "en_US.UTF-8");

    // Initialize ncurses
    initscr();

    // Enable color
    start_color();
    // Set to use default colors, which will be used below
    use_default_colors();

    // Init color pairs with default BG color (terminal's BG color)
    init_pair(BLUE_TEXT, COLOR_BLUE, -1);
    init_pair(YELLOW_TEXT, COLOR_YELLOW, -1);
    init_pair(RED_TEXT, COLOR_RED, -1);
    init_pair(GREEN_TEXT, COLOR_GREEN, -1);
    init_pair(CYAN_TEXT, COLOR_CYAN, -1);

    // Get max size of terminal
    getmaxyx(stdscr, maxY, maxX);

    // Initialize windows
    chatbox = newwin(maxY - 10, maxX - 10, 2, 5);
    chat_field = newpad(PAD_LENGTH, maxX - 12);
    input = newwin(7, maxX - 10, maxY - 8, 5);
    input_field = derwin(input, 4, maxX - 12, 2, 1);
    padX = 0;
    padY = 0;
    curH = 2;
    // Draw window borders
    box(chatbox, 0, 0);
    box(input, 0, 0);

    scrollok(chat_field, TRUE);

    // Enable keypad for input
    keypad(input_field, TRUE);

    refresh();
    wrefresh(chatbox);
    wrefresh(input);

    send(sockfd, name, NAME_LEN, 0);

    // printf("=== WELCOME TO CHATTER ===\n");
    // printf("Current time: %s", ctime(&now));
    // printf("Type :h or :help for Chatter commands\n");
    // Welcome text
    wattron(chat_field, COLOR_PAIR(CYAN_TEXT));
    waddstr(chat_field, "=== WELCOME TO THE CHATROOM ===\n");
    wprintw(chat_field, "Current time: %s", ctime(&now));
    waddstr(chat_field, "Type :h or :help for Chatter commands\n");
    wattroff(chat_field, COLOR_PAIR(CYAN_TEXT));

    prefresh(chat_field, padX, padY, 4, 6, PAD_VIEW_ROWS, PAD_VIEW_COLS);

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
    endwin();

    return EXIT_SUCCESS;
}

void str_overwrite_stdout() {
    wprintw(input_field, "\r%s", "> ");
    wrefresh(input_field);
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

// Print received messages
void print_msg(char* str, int color) {
    wattron(chat_field, COLOR_PAIR(color));
    wprintw(chat_field, "%s\n", str);
    wattroff(chat_field, COLOR_PAIR(color));
    auto_scroll(curH);

    // prefresh(window);
    prefresh(chat_field, padX, padY, 4, 6, PAD_VIEW_ROWS, PAD_VIEW_COLS);
}

// Auto scroll down if necessary when a new message comes
void auto_scroll(int curH) {
    if (curH > maxY - 13 + padX) {
        padX = curH - maxY + 13;
    }
}

void recv_msg_handler() {
    char buffer[BUFFER_SZ] = {};
    char message[BUFFER_SZ + 18] = {};

    while (1) {
        int receive = recv(sockfd, buffer, BUFFER_SZ, 0);
        if (!strncmp(buffer, "[SYSTEM] File: ", 15)) {
            // Print file notification
            time(&now);
            local = localtime(&now);
            // printf("%02d:%02d ~ %s\n", local->tm_hour, local->tm_min, message);

            // Initialize
            int fd = open(buffer + 15, O_WRONLY | O_CREAT | O_EXCL, 0700);
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
            // printf("[SYSTEM] File received\n");
            str_overwrite_stdout();
            // continue;
        }
        else {
            if (receive > 0) {
                /*
            time(&now);
            local = localtime(&now);
            str_overwrite_stdout();
            */
                getyx(input_field, curY, curX);
                time(&now);
                local = localtime(&now);
                sprintf(message, "%02d:%02d ~ %s", local->tm_hour, local->tm_min, buffer);
                curH = curH + 1 + strlen(message) / (PAD_VIEW_COLS);
                print_msg(message, 2);
                wmove(input_field, curY, curX);
                // str_overwrite_stdout();
            }
            else if (receive == 0) {
                break;
            }
        }

        bzero(buffer, BUFFER_SZ);
        bzero(message, BUFFER_SZ + 18);
    }
}

void send_msg_handler() {
    char buffer[BUFFER_SZ] = {};

    while (1) {
        str_overwrite_stdout();
        // do {
        //     noecho();
        //     ch = wgetch(input_field);
        //     wrefresh(input_field);
        //     switch (ch) {
        //     case KEY_UP:
        //         if (padX > 0)
        //             padX--;
        //         prefresh(chat_field, padX, padY, 4, 6, PAD_VIEW_ROWS, PAD_VIEW_COLS);
        //         break;
        //         // Don't let use scroll to far
        //     case KEY_DOWN:
        //         if (padX < curH - maxY + 13 && padX < PAD_LENGTH)
        //             padX++;
        //         prefresh(chat_field, padX, padY, 4, 6, PAD_VIEW_ROWS, PAD_VIEW_COLS);
        //         break;
        //     case 'i':
        //         echo();
        //         wgetnstr(input_field, buffer, BUFFER_SZ);
        //         break;
        //     default:
        //         break;
        //     }
        // } while (ch != 'i');
        wgetnstr(input_field, buffer, BUFFER_SZ);
        wclear(input_field);
        // fgets(buffer, BUFFER_SZ, stdin);
        str_trim_lf(buffer, BUFFER_SZ);

        if (!strcmp(buffer, ":help") || !strcmp(buffer, ":h")) {
            print_msg("Chatter commands:", 4);
            print_msg("- :c <pass>       - CREATE a new room", 4);
            print_msg("- :j <id> <pass>  - JOIN a room", 4);
            print_msg("- :s <id>         - SWITCH to room", 4);
            print_msg("- :l              - Temporary LEAVE room", 4);
            print_msg("- :r <name>       - RENAME self", 4);
            print_msg("- :q              - QUIT current room", 4);
            print_msg("- :f <filename>   - Send FILE to roommate", 4);
            print_msg("- :i              - Print room INFO", 4);
            print_msg("===", 4);
            print_msg("Press Ctrl+C to quit Chatter", 4);
        }
        else {
            // time(&now);
            // local = localtime(&now);
            // sprintf(message, "%s\n", buffer);
            send(sockfd, buffer, strlen(buffer), 0);
            // wattron(chat_field, COLOR_PAIR(CYAN_TEXT));
            // wattroff(chat_field, COLOR_PAIR(CYAN_TEXT));

        }
        bzero(buffer, BUFFER_SZ);
    }
    catch_ctrl_c_and_exit();
}
