#ifndef CLIENT_H
#define CLIENT_H

#include <ncurses.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9000

#define MAX_CLIENTS 100
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

void str_overwrite_stdout();
void str_trim_lf(char* arr, int length);
void catch_ctrl_c_and_exit();
void print_msg(char* str, int color);
void recv_msg_handler();
void send_msg_handler();
void auto_scroll(int curH);

#endif
