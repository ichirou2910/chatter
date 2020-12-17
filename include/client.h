#ifndef CLIENT_H
#define CLIENT_H

#include <ncurses.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9000

#define CLIENTS_MAX_GROUP 10
#define GROUP_MAX_MESSAGES 1000
#define GROUP_NAME_LEN 32
#define GROUP_ID_LEN 7
#define BUFFER_SZ 1024
#define NAME_LEN 32
#define GROUP_ID_LEN 7
#define PASSWORD_LEN 32

#define PAD_LENGTH 1000
#define PAD_VIEW_ROWS maxY - 10
#define PAD_VIEW_COLS maxX - 7
#define BLUE_TEXT 1
#define YELLOW_TEXT 2
#define RED_TEXT 3
#define GREEN_TEXT 4
#define CYAN_TEXT 5

typedef struct {
    // char messages[GROUP_MAX_MESSAGES][BUFFER_SZ];     // Max messages allocated for one group
    char group_id[GROUP_ID_LEN];            // Room ID
    char group_name[GROUP_NAME_LEN];        // Room name
} group_t;

void str_overwrite_stdout();
void str_trim_lf(char* arr, int length);
void catch_ctrl_c_and_exit();
void add_group(char* gid, char* name);
void update_room_list(char* list);
void recv_msg_handler();
void send_msg_handler();
void auto_scroll(int curH);

// === PRINT STUFF
void print_msg(char* str, int color); // Print a message
void print_info(char* info); // Print Room info
void print_chat(char* content); // Print Chat messages

// ---

#endif
