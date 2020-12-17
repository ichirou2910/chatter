#ifndef CLIENT_H
#define CLIENT_H

#include <ncurses.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9000

#define CLIENT_MAX_ROOMS 10
#define ROOM_MAX_MESSAGES 1000
#define ROOM_NAME_LEN 32
#define ROOM_ID_LEN 7
#define BUFFER_SZ 1024
#define NAME_LEN 32
#define ROOM_ID_LEN 7
#define PASSWORD_LEN 32

#define PAD_LENGTH 1000
#define PAD_VIEW_ROWS screen_cols - 10
#define PAD_VIEW_COLS screen_rows - 7
#define BLUE_TEXT 1
#define YELLOW_TEXT 2
#define RED_TEXT 3
#define GREEN_TEXT 4
#define CYAN_TEXT 5

typedef struct {
    // char messages[ROOM_MAX_MESSAGES][BUFFER_SZ];     // Max messages allocated for one room
    char room_id[ROOM_ID_LEN];            // Room ID
    char room_name[ROOM_NAME_LEN];        // Room name
} room_t;

void str_overwrite_stdout();
void str_trim_lf(char* arr, int length);
void catch_ctrl_c_and_exit();
void update_room_list(char* list);
void recv_msg_handler();
void send_msg_handler();
void auto_scroll(int chat_pad_height);

// === PRINT STUFF
void print_msg(char* str, int color); // Print a message
void print_info(char* info); // Print Room info
void print_chat(char* content); // Print Chat messages

// ---

#endif
