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
int rm_count;
time_t now;
struct tm* local;

WINDOW* chat_window, * chat_pad;
WINDOW* room_list_window, * room_list_pad, * room_active_pad;
WINDOW* input_window, * input_pad;
WINDOW* output_window, * output_pad;
int screen_cols, screen_rows;
int mouse_row, mouse_col;
int chat_pad_height;
int chat_pad_row, chat_pad_col;
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
    getmaxyx(stdscr, screen_rows, screen_cols);

    // Initialize windows
    chat_window = newwin(screen_rows - 13, screen_cols - 40, 2, 35);
    chat_pad = newpad(PAD_LENGTH, screen_cols - 42);

    input_window = newwin(7, screen_cols - 40, screen_rows - 11, 35);
    input_pad = derwin(input_window, 4, screen_cols - 42, 2, 1);

    room_list_window = newwin(screen_rows - 3, 30, 2, 5);
    room_list_pad = derwin(room_list_window, screen_rows - 6, 27, 2, 2);
    room_active_pad = derwin(room_list_window, screen_rows - 6, 1, 2, 1);

    output_window = newwin(3, screen_cols - 40, screen_rows - 4, 35);
    output_pad = derwin(output_window, 1, screen_cols - 43, 1, 2);

    chat_pad_row = 0;
    chat_pad_col = 0;
    chat_pad_height = 4;

    // Draw window borders
    box(chat_window, 0, 0);
    box(input_window, 0, 0);
    box(output_window, 0, 0);
    box(room_list_window, 0, 0);

    scrollok(chat_pad, TRUE);

    // Enable keypad for input
    keypad(input_pad, TRUE);

    refresh();
    wrefresh(chat_window);
    wrefresh(input_window);
    wrefresh(room_list_window);

    send(sockfd, name, NAME_LEN, 0);

    // Welcome text
    wattron(chat_pad, COLOR_PAIR(1));
    waddstr(chat_pad, "=== WELCOME TO CHATTER ===\n");
    wprintw(chat_pad, "Current time: %s", ctime(&now));
    waddstr(chat_pad, "Type :h or :help for Chatter commands\n");
    wattroff(chat_pad, COLOR_PAIR(1));

    prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);

    // Initial List room
    // TODO: Fix position
    // wattron(room_list_window, COLOR_PAIR(1));
    mvwaddstr(chat_window, 0, 2, " CHATBOX ");
    mvwaddstr(room_list_window, 0, 2, " ROOMS ");
    mvwaddstr(input_window, 0, 2, " MESSAGE ");
    // wattroff(room_list_window, COLOR_PAIR(1));

    wrefresh(room_list_window);
    wrefresh(chat_window);
    wrefresh(input_window);
    wrefresh(output_window);

    // Testing
    // wattron(room_list_pad, COLOR_PAIR(1));
    // waddstr(room_list_pad, "1. The Normies\n");
    // wattroff(room_list_pad, COLOR_PAIR(1));

    // prefresh(room_list_pad, chat_pad_row, chat_pad_col, 4, 6, screen_cols - 3, 30);

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
    wattron(input_pad, COLOR_PAIR(4));
    wprintw(input_pad, "\r%s", "> ");
    wattroff(input_pad, COLOR_PAIR(4));
    wrefresh(input_pad);
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
    chat_pad_height = chat_pad_height + 1 + strlen(str) / (PAD_VIEW_COLS);
    if (str != NULL) {
        if (color)
            wattron(chat_pad, COLOR_PAIR(color));
        wprintw(chat_pad, "%s\n", str);
        if (color)
            wattroff(chat_pad, COLOR_PAIR(color));
        auto_scroll(chat_pad_height);
    }

    prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
}

void print_info(char* info) {
    char** tokens = str_split(info, '\n');
    for (int i = 0; *(tokens + i); i++) {
        print_msg(tokens[i], 2);
        free(*(tokens + i));
    }
    free(tokens);
}

void print_chat(char* content) {
    // Clear chatbox
    wclear(chat_pad);

    if (content) {
        char** tokens = str_split(content, '\n');
        if (tokens) {
            for (int i = 0; *(tokens + i); i++) {
                print_msg(tokens[i], 0);
                free(*(tokens + i));
            }
        }
        free(tokens);
    }

    // Update
    prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
}

void print_sys(char* msg) {
    wclear(output_pad);
    wattron(output_pad, COLOR_PAIR(3));
    waddstr(output_pad, msg);
    wattroff(output_pad, COLOR_PAIR(3));
    wrefresh(output_pad);
}

void update_room_list(char* list) {
    // Clear List
    wclear(room_list_pad);
    if (list != NULL) {
        // Generate tokens
        char** tokens = str_split(list, '\n');
        int i = 0;
        int idx = 0;
        wattron(room_list_pad, COLOR_PAIR(1));
        while (*(tokens + i)) {
            room_t* gr = (room_t*)malloc(sizeof(room_t));
            mvwprintw(room_list_pad, idx * 2, 0, "%d. %s\n", idx + 1, *(tokens + i));
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
        wattroff(room_list_pad, COLOR_PAIR(1));
        free(tokens);
    }
    // prefresh(room_list_pad, chat_pad_row, chat_pad_col, 4, 6, screen_rows - 3, 30);
    wrefresh(room_list_pad);
}

// Auto scroll down if necessary when a new message comes
void auto_scroll(int chat_pad_height) {
    if (chat_pad_height > screen_rows - 12 + chat_pad_row) {
        chat_pad_row = chat_pad_height - screen_rows + 12;
    }
}

void recv_msg_handler() {
    char buffer[BUFFER_SZ] = {};
    char message[BUFFER_SZ + 18] = {};
    char* cmd;
    char* param;

    while (1) {
        int receive = recv(sockfd, buffer, BUFFER_SZ, 0);

        // if (!strncmp(buffer, "[SYSTEM] File: ", 15)) {
        //     // Print file notification
        //     time(&now);
        //     local = localtime(&now);
        //     sprintf(message, "%02d:%02d ~ %s\n", local->tm_hour, local->tm_min, buffer);
        //     print_msg(message, 3);

        //     // Initialize
        //     int fd = open(buffer + 15, O_WRONLY | O_CREAT | O_EXCL, 0700);
        //     char fileBuf[BUFFER_SZ];
        //     memset(fileBuf, 0x0, BUFFER_SZ);
        //     int bufLen = 0;
        //     // int pck_cnt = 0;

        //     // Get file size
        //     // char tmpBuf[BUFFER_SZ];
        //     // recv(sockfd, tmpBuf, BUFFER_SZ, 0);
        //     // long file_size = atol(tmpBuf);
        //     // printf("File size: %ld\n", file_size);

        //     while ((bufLen = read(sockfd, fileBuf, BUFFER_SZ)) > 0) {
        //         int write_sz = write(fd, fileBuf, bufLen);
        //         memset(fileBuf, 0x0, BUFFER_SZ);
        //         // file_size -= (long)bufLen;
        //         // printf("%d - Data left: %ld\n", pck_cnt++, file_size);
        //         if (write_sz < bufLen) {
        //             break;
        //         }
        //         if (bufLen == 0 || bufLen != BUFFER_SZ) {
        //             break;
        //         }
        //     }
        //     close(fd);
        //     // printf("[SYSTEM] File received\n");
        //     str_overwrite_stdout();
        //     // continue;
        // }
        // else {
        if (receive > 0) {
            buffer[strlen(buffer)] = 0;
            cmd = strtok(buffer, " \n");
            int color = 0;

            if (!strcmp(cmd, "[FILE]")) {
                param = strtok(NULL, "");
                // Print file notification
                time(&now);
                local = localtime(&now);
                sprintf(message, "%02d:%02d ~ [FILE] %s\n", local->tm_hour, local->tm_min, param);
                print_msg(message, 3);

                // Initialize
                int fd = open(param, O_WRONLY | O_CREAT | O_EXCL, 0700);
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
                // str_overwrite_stdout();
                // continue;
            }
            else if (!strcmp(cmd, "[INFO]")) {
                param = strtok(NULL, "");
                print_info(param);
            }
            else if (!strcmp(cmd, "[ROOMS]")) {
                param = strtok(NULL, "");
                update_room_list(param);
            }
            else if (!strcmp(cmd, "[MESSAGES]")) {
                param = strtok(NULL, "");
                print_chat(param);
            }
            else if (!strcmp(cmd, "[SYSTEM]")) {
                cmd[strlen(cmd)] = ' ';
                print_sys(buffer + 9);
            }
            else {
                cmd[strlen(cmd)] = ' ';
                getyx(input_pad, mouse_col, mouse_row);
                time(&now);
                local = localtime(&now);
                sprintf(message, "%02d:%02d ~ %s", local->tm_hour, local->tm_min, buffer);
                print_msg(message, color);
                wmove(input_pad, mouse_col, mouse_row);
                // str_overwrite_stdout();
            }
            str_overwrite_stdout();
        }
        else {
            break;
        }
        // }

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
        //     ch = wgetch(input_pad);
        //     wrefresh(input_pad);
        //     switch (ch) {
        //     case KEY_UP:
        //         if (chat_pad_row > 0)
        //             chat_pad_row--;
        //         prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 6, PAD_VIEW_ROWS, PAD_VIEW_COLS);
        //         break;
        //         // Don't let use scroll to far
        //     case KEY_DOWN:
        //         if (chat_pad_row < chat_pad_height - screen_cols + 13 && chat_pad_row < PAD_LENGTH)
        //             chat_pad_row++;
        //         prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 6, PAD_VIEW_ROWS, PAD_VIEW_COLS);
        //         break;
        //     case 'i':
        //         echo();
        //         wgetnstr(input_pad, buffer, BUFFER_SZ);
        //         break;
        //     default:
        //         break;
        //     }
        // } while (ch != 'i');
        wgetnstr(input_pad, buffer, BUFFER_SZ);
        wclear(input_pad);
        wrefresh(input_pad);

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
                    print_sys("Positive index only!");
                }
                else {
                    if (rooms[idx - 1]) {
                        // Mark active room in room list window
                        wclear(room_active_pad);
                        wattron(room_active_pad, COLOR_PAIR(4));
                        mvwaddch(room_active_pad, (idx - 1) * 2, 0, '*');
                        wattroff(room_active_pad, COLOR_PAIR(4));
                        wrefresh(room_active_pad);

                        sprintf(buffer, ":s %s", rooms[idx - 1]->room_id);
                        send(sockfd, buffer, strlen(buffer), 0);
                    }
                    else {
                        print_sys("Room doesn't exist!");
                    }
                }
            }
            else {
                print_sys("Please provide index!");
            }
        }
        else if (!strncmp(buffer, ":q", 2)) {
            // Clear active mark
            wclear(room_active_pad);
            wrefresh(room_active_pad);
            // Clear chatbox
            wclear(chat_pad);
            prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
            // Actually send :q
            sprintf(buffer, ":q");
            send(sockfd, buffer, strlen(buffer), 0);
        }
        else {
            send(sockfd, buffer, strlen(buffer), 0);
        }
        bzero(buffer, BUFFER_SZ);
    }
    catch_ctrl_c_and_exit();
}
