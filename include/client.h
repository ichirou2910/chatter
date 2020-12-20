#ifndef CLIENT_H
#define CLIENT_H

#define SERVER_IP "127.0.0.1"
#define PORT 9000

// Lengths
#define CLIENT_MAX_ROOMS 10
#define ROOM_MAX_MESSAGES 1000
#define ROOM_NAME_LEN 32
#define ROOM_ID_LEN 7
#define BUFFER_SZ 1024
#define NAME_LEN 32
#define ROOM_ID_LEN 7
#define PASSWORD_LEN 32

#define PAD_LENGTH 1000
#define PAD_VIEW_ROWS screen_rows - 13
#define PAD_VIEW_COLS screen_cols - 7
// Display color
#define BLUE_TEXT 1
#define YELLOW_TEXT 2
#define RED_TEXT 3
#define GREEN_TEXT 4
#define CYAN_TEXT 5

typedef struct {
    char room_id[ROOM_ID_LEN]; // Room ID
    char room_name[ROOM_NAME_LEN]; // Room name
} room_t;

// === THREAD HANDLERS ===
void recv_msg_handler();
void send_msg_handler();
// ---

// === PRINT STUFF ===
void print_normal(char* str, int color);
void print_msg(char* msg);
void print_help();
void print_info(char* info);
void print_user(char* info);
void print_chat(char* content);
void print_sys(char* msg);
void update_room_list(char* list);
// ---

// === UTILITIES ===
void str_overwrite_stdout();
void catch_ctrl_c_and_exit();
void auto_scroll(int chat_pad_height);
void reset_chat_pad();
// ---

#endif
