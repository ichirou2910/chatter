#ifndef CLIENT_H
#define CLIENT_H

#define SERVER_IP "127.0.0.1"
#define PORT 9000

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define NAME_LEN 32
#define GROUP_ID_LEN 7
#define PASSWORD_LEN 32

void str_overwrite_stdout();
void str_trim_lf(char* arr, int length);
void catch_ctrl_c_and_exit();
void print_msg(char* str, int mode);
void recv_msg_handler();
void send_msg_handler();

#endif
