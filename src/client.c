#include "client.h"
#include "utils.h"

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ncurses.h>
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

// Rooms info collection
room_t* rooms[CLIENT_MAX_ROOMS];
int rm_count;

// Time stuff
time_t now;
struct tm* local;

WINDOW* chat_window, * chat_pad;
WINDOW* room_list_window, * room_list_pad, * room_active_pad;
WINDOW* input_window, * input_pad;
WINDOW* output_window, * output_pad;
WINDOW* user_window, * user_pad;
int screen_cols, screen_rows;
int mouse_row, mouse_col;
int chat_pad_height;
int chat_pad_row, chat_pad_col;
int chat_mode = 0;

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

    room_list_window = newwin(screen_rows - 7, 30, 6, 5);
    room_list_pad = derwin(room_list_window, screen_rows - 10, 27, 2, 2);
    room_active_pad = derwin(room_list_window, screen_rows - 10, 1, 2, 1);

    output_window = newwin(3, screen_cols - 40, screen_rows - 4, 35);
    output_pad = derwin(output_window, 1, screen_cols - 43, 1, 2);

    // user_window = newwin(4, 30, screen_rows - 5, 5);
    // user_pad = derwin(user_window, 2, 27, 1, 2);
    user_window = newwin(4, 30, 2, 5);
    user_pad = derwin(user_window, 2, 27, 1, 2);

    chat_pad_row = 0;
    chat_pad_col = 0;
    chat_pad_height = 0;

    // Draw window borders
    box(chat_window, 0, 0);
    box(input_window, 0, 0);
    box(output_window, 0, 0);
    box(room_list_window, 0, 0);
    box(user_window, 0, 0);

    // Allow scrolling for chatbox
    scrollok(chat_pad, TRUE);

    // Enable keypad for input
    keypad(input_pad, TRUE);

    refresh();
    wrefresh(chat_window);
    wrefresh(input_window);
    wrefresh(room_list_window);
    wrefresh(user_window);

    // Send name to server
    send(sockfd, name, NAME_LEN, 0);

    // Welcome text
    print_normal("=== WELCOME TO CHATTER ===", 1);
    print_normal("Type :h for Chatter commands", 1);

    // Initial List room
    mvwaddstr(chat_window, 0, 2, " CHATBOX ");
    mvwaddstr(room_list_window, 0, 2, " ROOMS ");
    mvwaddstr(input_window, 0, 2, " MESSAGE ");

    wrefresh(room_list_window);
    wrefresh(chat_window);
    wrefresh(input_window);
    wrefresh(output_window);

    // Thread for sending messages
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

    // Clean up
    close(sockfd);
    endwin();

    return EXIT_SUCCESS;
}

void send_msg_handler() {
    char buffer[BUFFER_SZ] = "";
    int submitted = 0;
    int buflen = 0;
    int ch;

    while (1) {
        str_overwrite_stdout();

        // Reset
        strcpy(buffer, "");
        buflen = 0;
        submitted = 0;

        // Handle Input
        noecho();
        do {
            ch = wgetch(input_pad);
            wrefresh(input_pad);
            switch (ch) {
            case KEY_UP: // Up = Scroll chat up
                getyx(input_pad, mouse_col, mouse_row);
                if (chat_pad_row > 0)
                    chat_pad_row--;
                prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
                wmove(input_pad, mouse_row, mouse_col);
                break;
            case KEY_DOWN: // Down = Scroll chat down
                getyx(input_pad, mouse_col, mouse_row);
                if (chat_pad_row < chat_pad_height - screen_rows + 16 && chat_pad_row < PAD_LENGTH)
                    chat_pad_row++;
                prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
                wmove(input_pad, mouse_row, mouse_col);
                break;
            case 127: // Delete
            case 8: // Backspace
            case KEY_RIGHT:
            case KEY_LEFT:
                // Emulate backspace
                if (buflen) {
                    // update msg content
                    buffer[--buflen] = 0;
                    // update input box
                    mvwaddch(input_pad, 0, buflen + 2, 32);
                    wrefresh(input_pad);
                    wmove(input_pad, 0, buflen + 2);
                }
                break;
            case 10: // Enter
                submitted = 1;
                break;
            default: // Emulate typing
                // update msg content
                buffer[buflen++] = ch;
                // update input box
                mvwaddch(input_pad, 0, buflen + 1, ch);
                wrefresh(input_pad);
                break;
            }
            if (submitted)
                break;
        } while (ch != KEY_ENTER);
        echo();

        buffer[buflen] = 0;
        // Clear input
        wclear(input_pad);
        wrefresh(input_pad);

        // --- Preprocess before sending to server ---
        // :h - Print help
        if (!strcmp(buffer, ":h")) {
            print_help();
        }
        // :s - Switch room
        // Room IDs are mapped to indexes for easier switching
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

                        // Generate command
                        sprintf(buffer, ":s %s", rooms[idx - 1]->room_id);
                        // Send command to server
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
        // :q - Quit room
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
            // Clear output window
            wclear(output_pad);
            wrefresh(output_pad);
            // Send command to server
            send(sockfd, buffer, strlen(buffer), 0);
        }
    }
    catch_ctrl_c_and_exit();
}

void recv_msg_handler() {
    char buffer[BUFFER_SZ] = {};
    char message[BUFFER_SZ + 18] = {};
    char* cmd;
    char* param;

    while (1) {
        int receive = recv(sockfd, buffer, BUFFER_SZ, 0);

        if (receive > 0) {
            // Get current mouse position
            getyx(input_pad, mouse_col, mouse_row);
            buffer[strlen(buffer)] = 0;
            // Get message header
            cmd = strtok(buffer, " \n");

            // [FILE] - Incoming file
            if (!strcmp(cmd, "[FILE]")) {
                param = strtok(NULL, "");

                // Print file notification
                time(&now);
                local = localtime(&now);
                sprintf(message, "%02d:%02d ~ [FILE] %s", local->tm_hour, local->tm_min, param);
                print_normal(message, 3);

                // Initialize
                int fd = open(param, O_WRONLY | O_CREAT | O_EXCL, 0700);
                char fileBuf[BUFFER_SZ];
                memset(fileBuf, 0x0, BUFFER_SZ);
                int bufLen = 0;

                // Receive file
                while ((bufLen = read(sockfd, fileBuf, BUFFER_SZ)) > 0) {
                    int write_sz = write(fd, fileBuf, bufLen);
                    memset(fileBuf, 0x0, BUFFER_SZ);
                    if (write_sz < bufLen) {
                        break;
                    }
                    if (bufLen == 0 || bufLen != BUFFER_SZ) {
                        break;
                    }
                }
                close(fd);
            }
            // [INFO] - Incoming info data
            else if (!strcmp(cmd, "[INFO]")) {
                param = strtok(NULL, "");
                print_info(param);
            }
            // [USER] - Incoming User info
            else if (!strcmp(cmd, "[USER]")) {
                param = strtok(NULL, "");
                print_user(param);
            }
            // [ROOMS] - Incoming room list
            else if (!strcmp(cmd, "[ROOMS]")) {
                param = strtok(NULL, "");
                update_room_list(param);
            }
            // [MESSAGES] - Incoming chatroom's previous messages
            else if (!strcmp(cmd, "[MESSAGES]")) {
                param = strtok(NULL, "");
                print_chat(param);
            }
            // [SYSTEM] - Incoming system message
            else if (!strcmp(cmd, "[SYSTEM]")) {
                cmd[strlen(cmd)] = ' ';
                print_sys(buffer + 9);
            }
            // A normal chat message
            else {
                cmd[strlen(cmd)] = ' ';
                // time(&now);
                // local = localtime(&now);
                // sprintf(message, "%02d:%02d ~ %s", local->tm_hour, local->tm_min, buffer);
                print_msg(buffer);
            }
            wmove(input_pad, mouse_col, mouse_row);
            str_overwrite_stdout();
        }
        else {
            break;
        }

        bzero(buffer, BUFFER_SZ);
        bzero(message, BUFFER_SZ + 18);
    }
}

// Print STR to chatbox with specified COLOR
void print_normal(char* str, int color) {
    // Watch for content height
    chat_pad_height = chat_pad_height + 1 + strlen(str) / (PAD_VIEW_COLS);

    // Print message
    if (str != NULL) {
        if (color)
            wattron(chat_pad, COLOR_PAIR(color));
        wprintw(chat_pad, "%s\n", str);
        if (color)
            wattroff(chat_pad, COLOR_PAIR(color));
        // Scroll down to fit if needed
        auto_scroll(chat_pad_height);
    }

    prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
}

// Print a chat message
// [Name#uid] ~ hh:mm
// Message
void print_msg(char* msg) {
    // Watch for content height
    chat_pad_height = chat_pad_height + 2 + strlen(msg) / (PAD_VIEW_COLS);
    int idx = 0;

    // Open bracket
    wattron(chat_pad, COLOR_PAIR(2));
    waddch(chat_pad, msg[idx++]);
    wattroff(chat_pad, COLOR_PAIR(2));

    // Name part
    wattron(chat_pad, COLOR_PAIR(1));
    while (msg[idx] != '#') {
        waddch(chat_pad, msg[idx++]);
    }
    wattroff(chat_pad, COLOR_PAIR(1));

    // UID part
    wattron(chat_pad, COLOR_PAIR(4));
    while (msg[idx] != ']') {
        waddch(chat_pad, msg[idx++]);
    }
    wattroff(chat_pad, COLOR_PAIR(4));

    // Closing bracket
    wattron(chat_pad, COLOR_PAIR(2));
    waddch(chat_pad, msg[idx++]);

    // Message
    while (msg[idx] != '\n') {
        waddch(chat_pad, msg[idx++]);
    }
    wattroff(chat_pad, COLOR_PAIR(2));
    waddstr(chat_pad, msg + idx);

    // Final line break
    waddch(chat_pad, '\n');

    // Refresh
    prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
}

// Print help content
void print_help() {
    print_normal("[HELP]", 5);
    print_normal("Chatter commands:", 5);
    print_normal("- :c <pass> <name> - CREATE a new room, <pass> must not have space", 5);
    print_normal("- :j <id> <pass>   - JOIN a room", 5);
    print_normal("- :s <num>         - SWITCH to room with index displayed in [ROOMS]", 5);
    print_normal("- :r <name>        - RENAME self", 5);
    print_normal("- :q               - QUIT current room", 5);
    print_normal("- :f <path>        - Send FILE to roommate", 5);
    print_normal("- :i               - Print room INFO", 5);
    print_normal("---", 5);
    print_normal("Chatter keybinding:", 5);
    print_normal("- Ctrl+C           - Quit Chatter", 5);
    print_normal("- Up               - Scroll chatbox up", 5);
    print_normal("- Down             - Scroll chatbox down", 5);
}

// Print room info
void print_info(char* info) {
    char** tokens = str_split(info, '\n');
    for (int i = 0; *(tokens + i); i++) {
        print_normal(tokens[i], 2);
        free(*(tokens + i));
    }
    free(tokens);
}

// Print user info
void print_user(char* info) {
    wclear(user_pad);
    waddstr(user_pad, info);
    wrefresh(user_pad);
}

// Print chatroom's previous messages
void print_chat(char* content) {
    // Clear chatbox
    wclear(chat_pad);
    chat_pad_row = 0;
    chat_pad_height = 0;

    char* msg = (char*)malloc(BUFFER_SZ + NAME_LEN + 17);
    strcpy(msg, "");

    if (content) {
        char** tokens = str_split(content, '\n');
        if (tokens) {
            int i = 0;
            while (*(tokens + i)) {
                // A message takes 2 lines = 2 tokens
                strcat(msg, tokens[i++]);
                strcat(msg, "\n");
                strcat(msg, tokens[i++]);
                // strcat(msg, "\n");
                print_msg(msg);
                strcpy(msg, "");
            }
            // for (int i = 0; *(tokens + i); i++) {

            //     print_normal(tokens[i], 0);
            //     free(*(tokens + i));
            // }
        }
        free(tokens);
    }

    // Update
    prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
}

// Print system message
void print_sys(char* msg) {
    wclear(output_pad);
    wattron(output_pad, COLOR_PAIR(3));
    waddstr(output_pad, msg);
    wattroff(output_pad, COLOR_PAIR(3));
    wrefresh(output_pad);
}

// Update ROOMS window content
void update_room_list(char* list) {
    // Clear old list
    wclear(room_list_pad);

    if (list != NULL) {
        // Prepare list
        char** tokens = str_split(list, '\n');
        int i = 0; // Token index
        int idx = 0; // Collection index

        // Update
        wattron(room_list_pad, COLOR_PAIR(1));
        while (*(tokens + i)) {
            room_t* gr = (room_t*)malloc(sizeof(room_t));

            // Print
            mvwprintw(room_list_pad, idx * 2, 0, "%d. %s\n", idx + 1, *(tokens + i));
            strcpy(gr->room_name, *(tokens + i));
            free(*(tokens + i));
            i++;
            strcpy(gr->room_id, *(tokens + i));
            free(*(tokens + i));
            i++;

            // Add to collection
            if (rooms[idx]) {
                free(rooms[idx]);
            }
            rooms[idx] = gr;
            idx++;
        }
        wattroff(room_list_pad, COLOR_PAIR(1));

        // Cleanup
        free(tokens);
    }
    wrefresh(room_list_pad);
}

// Print prompt
void str_overwrite_stdout() {
    wattron(input_pad, COLOR_PAIR(4));
    wprintw(input_pad, "\r%s", "> ");
    wattroff(input_pad, COLOR_PAIR(4));
    wrefresh(input_pad);
    fflush(stdout);
}

void catch_ctrl_c_and_exit() {
    flag = 1;
}

// Auto scroll down if necessary when a new message comes
void auto_scroll(int chat_pad_height) {
    if (chat_pad_height > screen_rows - 16 + chat_pad_row) {
        chat_pad_row = chat_pad_height - screen_rows + 16;
    }
}

// Reset chat pad param
void reset_chat_pad() {
    chat_pad_row = 0;
    chat_pad_col = 0;
    chat_pad_height = 0;
    wclear(chat_pad);
    prefresh(chat_pad, chat_pad_row, chat_pad_col, 4, 36, PAD_VIEW_ROWS, PAD_VIEW_COLS);
}