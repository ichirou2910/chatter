#include "client.h"
#include "utils.h"

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
room_t* rooms[CLIENT_MAX_ROOMS];
int gr_count;
time_t now;
struct tm* local;

WINDOW* chatbox, * chat_field;
WINDOW* listbox, * list_field;
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
    chatbox = newwin(maxY - 10, maxX - 40, 2, 35);
    chat_field = newpad(PAD_LENGTH, maxX - 42);
    input = newwin(7, maxX - 40, maxY - 8, 35);
    input_field = derwin(input, 4, maxX - 42, 2, 1);
    listbox = newwin(maxY - 3, 30, 2, 5);
    list_field = newpad(PAD_LENGTH, maxX - 5);
    padX = 0;
    padY = 0;
    curH = 4;

    // Draw window borders
    box(chatbox, 0, 0);
    box(input, 0, 0);
    box(listbox, 0, 0);

    scrollok(chat_field, TRUE);

    // Enable keypad for input
    keypad(input_field, TRUE);

    refresh();
    wrefresh(chatbox);
    wrefresh(input);
    wrefresh(listbox);

    send(sockfd, name, NAME_LEN, 0);

    // Welcome text
    wattron(chat_field, COLOR_PAIR(1));
    waddstr(chat_field, "=== WELCOME TO CHATTER ===\n");
    wprintw(chat_field, "Current time: %s", ctime(&now));
    waddstr(chat_field, "Type :h or :help for Chatter commands\n");
    wattroff(chat_field, COLOR_PAIR(1));

    prefresh(chat_field, padX, padY, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);

    // Initial List room
    // TODO: Fix position
    // wattron(listbox, COLOR_PAIR(1));
    mvwaddstr(chatbox, 0, 2, " CHATBOX ");
    mvwaddstr(listbox, 0, 2, " ROOMS ");
    mvwaddstr(input, 0, 2, " MESSAGE ");
    // wattroff(listbox, COLOR_PAIR(1));

    wrefresh(listbox);
    wrefresh(chatbox);
    wrefresh(input);

    // Testing
    // wattron(list_field, COLOR_PAIR(1));
    // waddstr(list_field, "1. The Normies\n");
    // wattroff(list_field, COLOR_PAIR(1));

    // prefresh(list_field, padX, padY, 4, 6, maxY - 3, 30);

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
    wattron(input_field, COLOR_PAIR(4));
    wprintw(input_field, "\r%s", "> ");
    wattroff(input_field, COLOR_PAIR(4));
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
    curH = curH + 1 + strlen(str) / (PAD_VIEW_COLS);
    wattron(chat_field, COLOR_PAIR(color));
    wprintw(chat_field, "%s\n", str);
    wattroff(chat_field, COLOR_PAIR(color));
    auto_scroll(curH);

    // prefresh(window);
    prefresh(chat_field, padX, padY, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
}

void print_info(char* info) {
    char** tokens = str_split(info, '\n');
    for (int i = 0; *(tokens + i); i++) {
        print_msg(tokens[i], 2);
        free(*(tokens + i));
    }
    free(tokens);
}

void update_room_list(char* list) {
    // Clear List
    wclear(list_field);
    // Generate tokens
    char** tokens = str_split(list, '\n');
    int i = 0;
    int idx = 0;
    wattron(list_field, COLOR_PAIR(1));
    while (*(tokens + i)) {
        room_t* gr = (room_t*)malloc(sizeof(room_t));
        wprintw(list_field, "%d. %s\n", idx + 1, *(tokens + i));
        strcpy(gr->room_name, *(tokens + i));
        free(*(tokens + i));
        i++;
        strcpy(gr->room_id, *(tokens + i));
        free(*(tokens + i));
        i++;
        if (rooms[idx]) {
            free(rooms[idx]);
        }
        rooms[idx] = gr;
        idx++;
    }
    free(tokens);
    wattroff(list_field, COLOR_PAIR(1));
    prefresh(list_field, padX, padY, 4, 6, maxY - 3, 30);
}

// Auto scroll down if necessary when a new message comes
void auto_scroll(int curH) {
    if (curH > maxY - 12 + padX) {
        padX = curH - maxY + 12;
    }
}

void recv_msg_handler() {
    char buffer[BUFFER_SZ] = {};
    char message[BUFFER_SZ + 18] = {};
    char* cmd;
    char* param;

    while (1) {
        int receive = recv(sockfd, buffer, BUFFER_SZ, 0);
        buffer[strlen(buffer)] = 0;
        if (!strncmp(buffer, "[SYSTEM] File: ", 15)) {
            // Print file notification
            time(&now);
            local = localtime(&now);
            sprintf(message, "%02d:%02d ~ %s\n", local->tm_hour, local->tm_min, buffer);
            print_msg(message, 3);

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
                cmd = strtok(buffer, " \n");
                int color = 0;

                if (!strcmp(cmd, "[INFO]")) {
                    param = strtok(NULL, "");
                    print_info(param);
                }
                else if (!strcmp(cmd, "[ROOMS]")) {
                    param = strtok(NULL, "");
                    update_room_list(param);
                }
                else {
                    cmd[strlen(cmd)] = ' ';
                    if (!strncmp(buffer, "[SYSTEM]", 8)) {
                        color = 3;
                    }
                    getyx(input_field, curY, curX);
                    time(&now);
                    local = localtime(&now);
                    sprintf(message, "%02d:%02d ~ %s", local->tm_hour, local->tm_min, buffer);
                    print_msg(message, color);
                    wmove(input_field, curY, curX);
                    // str_overwrite_stdout();
                }
                str_overwrite_stdout();
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
        wrefresh(input_field);

        str_trim_lf(buffer, BUFFER_SZ);

        if (!strcmp(buffer, ":help") || !strcmp(buffer, ":h")) {
            int color = 5;
            print_msg("===", color);
            print_msg("Chatter commands:", color);
            print_msg("- :c <pass>       - CREATE a new room", color);
            print_msg("- :j <id> <pass>  - JOIN a room", color);
            print_msg("- :s <id>         - SWITCH to room", color);
            print_msg("- :r <name>       - RENAME self", color);
            print_msg("- :q              - QUIT current room", color);
            print_msg("- :f <filename>   - Send FILE to roommate", color);
            print_msg("- :i              - Print room INFO", color);
            print_msg("===", color);
            print_msg("Press Ctrl+C to quit Chatter", color);
        }
        else if (!strncmp(buffer, ":s", 2)) {
            strtok(buffer, " \n");
            char* param = strtok(NULL, " \n");
            if (param != NULL) {
                int idx = atoi(param);
                if (idx <= 0) {
                    print_msg("Positive index only!", 3);
                }
                else {
                    // print_msg(rooms[idx - 1]->room_id, 4);
                    sprintf(buffer, ":s %s", rooms[idx - 1]->room_id);
                    send(sockfd, buffer, strlen(buffer), 0);
                }
            }
            else {
                print_msg("Please provide index!", 3);
            }
        }
        else {
            send(sockfd, buffer, strlen(buffer), 0);
        }
        bzero(buffer, BUFFER_SZ);
    }
    catch_ctrl_c_and_exit();
}
