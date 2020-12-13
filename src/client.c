#include "client.h"
#include <gtk/gtk.h>
#include <glib.h>

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

int connected = 0;
int connect_once = 0;
int name_inited = 0;
int switch_flag = 0;

GtkWidget *msg_box;
GtkWidget *chat_box;
GtkWidget *room_input;
GtkWidget *pass_input;
GtkTextBuffer *room_buf;
GtkTextBuffer *pass_buf;
GtkTextBuffer *msg_buffer;
GtkTextBuffer *chat_buffer;
char send_buf[BUFFER_SZ] = {};

struct sockaddr_in serv_addr;
static pthread_mutex_t mutex;
static pthread_t conn;

void str_overwrite_stdout() {
    g_print("\r%s", "> ");
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
    char msg_content[1000] = {};

    while (1) {
        int receive = recv(sockfd, message, BUFFER_SZ, 0);

        if (!strncmp(message, "[SYSTEM] File: ", 15)) {
            // Print file notification
            time(&now);
            local = localtime(&now);
            // sprintf(msg_content, "%02d:%02d ~ %s\n", local->tm_hour, local->tm_min, message);

            //Get iter of chat_box
            // GtkTextIter start_chat_box;
            // gtk_text_buffer_get_end_iter(chat_buffer, &start_chat_box);

            // gtk_text_buffer_insert(chat_buffer, &start_chat_box, msg_content, -1);

            // get the file name only
            // memmove(message, message + 15, strlen(message) - 15);
            // Initialize
            int fd = open(message + 15, O_WRONLY | O_CREAT | O_EXCL, 0700);
            char fileBuf[BUFFER_SZ];
            memset(fileBuf, 0x0, BUFFER_SZ);
            int bufLen = 0;

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

            // sprintf(msg_content, "%02d:%02d ~ [SYSTEM] File received\n", local->tm_hour, local->tm_min);
            // g_print(msg_content);

            // gtk_text_buffer_get_end_iter(chat_buffer, &start_chat_box);
            // gtk_text_buffer_insert(chat_buffer, &start_chat_box, msg_content, -1);

            // continue;

        } else {
            if (receive > 0) {
                time(&now);
                local = localtime(&now);
                sprintf(msg_content, "%02d:%02d ~ %s\n", local->tm_hour, local->tm_min, message);

                //Get iter of chat_box
                GtkTextIter start_chat_box;
                gtk_text_buffer_get_end_iter(chat_buffer, &start_chat_box);

                gtk_text_buffer_insert(chat_buffer, &start_chat_box, msg_content, -1);

            }
            else if (receive == 0) {
                break;
            }
            bzero(message, BUFFER_SZ);
            bzero(msg_content, 1000);
        }
    }
}

void send_msg_handler() {
    time(&now);
    local = localtime(&now);

    char msg_content[1000] = {};

    //Get iter of msg_box
    GtkTextIter start_msg_box;
    GtkTextIter end_msg_box;
    gtk_text_buffer_get_start_iter (msg_buffer, &start_msg_box);
    gtk_text_buffer_get_end_iter (msg_buffer, &end_msg_box);

    //Get iter of chat_box
    GtkTextIter start_chat_box;
    gtk_text_buffer_get_end_iter(chat_buffer, &start_chat_box);

    strcpy(send_buf, gtk_text_buffer_get_text(msg_buffer, &start_msg_box, &end_msg_box, FALSE));
    str_trim_lf(send_buf, strlen(send_buf));

    //Get content for chat_box
    sprintf(msg_content, "%02d:%02d ~ [You] %s\n", local->tm_hour, local->tm_min, send_buf);

    //Insert into chat box
    gtk_text_buffer_insert(chat_buffer, &start_chat_box, msg_content, -1);

    if (!strcmp(send_buf, ":help") || !strcmp(send_buf, ":h")) {
        bzero(msg_content, 1000);
        strcat(msg_content, "Chatter commands:\n");
        strcat(msg_content, "- :c | :create <password>     - Create a new room with <password>\n");
        strcat(msg_content, "- :j | :join <id> <password>  - Join a room with <id> & <password>\n");
        strcat(msg_content, "- :s | :switch <id>           - Switch to room with <id>\n");
        strcat(msg_content, "- :l | :leave                 - Temporary leave room and return to lobby\n");
        strcat(msg_content, "- :r | :rename <name>         - Rename self to <name>\n");
        strcat(msg_content, "- :q | :quit                  - Quit current room and return to lobby\n");
        strcat(msg_content, "- :f | :file <filename>       - Send file with <filename> to roommate\n");
        strcat(msg_content, "- :i | :info                  - Print room info\n");
        strcat(msg_content, "===\n");

        gtk_text_buffer_get_end_iter(chat_buffer, &start_chat_box);
        gtk_text_buffer_insert(chat_buffer, &start_chat_box, msg_content, -1);
        
    }
    else {
        send(sockfd, send_buf, strlen(send_buf), 0);
    }
    bzero(msg_content, 1000);
    bzero(send_buf, BUFFER_SZ);

    gtk_text_buffer_delete(msg_buffer, &start_msg_box, &end_msg_box);
}

static void *server_connect(){
    const char* ip = SERVER_IP;
    int port = PORT;

    // Socket Settings
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    // Connect to the server
    int err = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (err == -1) {
        g_print("ERROR: connect\n");
        return EXIT_FAILURE;
    }

    send(sockfd, name, NAME_LEN, 0);
}

void on_open_file_activate(GtkMenuItem *menuitem, GtkWidget* w_dlg_file_choose) {
    // Show the "Open Text File" dialog box
    gtk_widget_show(w_dlg_file_choose);
}

void on_open_btn_clicked(GtkButton *button, GtkWidget* w_dlg_file_choose) {
    char path[1000] = {};

    // Get the file name from the dialog box
    if(connected) {
        strcpy(path, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(w_dlg_file_choose)));
        if (strcmp(path, "")) {
            sprintf(send_buf, ":f %s", path);
            send(sockfd, send_buf, strlen(send_buf), 0);
            bzero(send_buf, BUFFER_SZ);

            g_print("File request sent!\n");
        }
    }

    // Finished with the "Open Text File" dialog box, so hide it
    gtk_widget_hide(w_dlg_file_choose);
}

void on_cancel_bt_clicked(GtkButton *button, GtkWidget* w_dlg_file_choose) {
    gtk_widget_hide(w_dlg_file_choose);
}

void on_confirm_btn_clicked(GtkButton *button, GtkWidget *join_dlg) {
    char room[10] = {};
    char pass[PASSWORD_LEN] = {};

    //Get room's iter
    GtkTextIter start_room;
    GtkTextIter end_room;
    gtk_text_buffer_get_start_iter (room_buf, &start_room);
    gtk_text_buffer_get_end_iter (room_buf, &end_room);

    //Get pass's iter
    GtkTextIter start_pass;
    GtkTextIter end_pass;
    gtk_text_buffer_get_start_iter (pass_buf, &start_pass);
    gtk_text_buffer_get_end_iter (pass_buf, &end_pass);

    //Get room's input
    strcpy(room, gtk_text_buffer_get_text(room_buf, &start_room, &end_room, FALSE));

    //Get pass's input
    strcpy(pass, gtk_text_buffer_get_text(pass_buf, &start_pass, &end_pass, FALSE));

    sprintf(send_buf, ":j %s %s", room, pass);
    send(sockfd, send_buf, strlen(send_buf), 0);
    bzero(send_buf, BUFFER_SZ);

    //Close the widget
    gtk_widget_hide(join_dlg);
}

void on_change_name_btn_clicked(GtkButton *button, GtkTextBuffer *buffer) {

    //Get iter
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_start_iter (buffer, &start);
    gtk_text_buffer_get_end_iter (buffer, &end);

    //Get name
    strcpy(name, gtk_text_buffer_get_text(buffer, &start, &end, FALSE));
    g_print("Name changed: %s\n", name);

    if(name_inited) {
        sprintf(send_buf, ":r %s", name);
        send(sockfd, send_buf, strlen(send_buf), 0);
        bzero(send_buf, BUFFER_SZ);
    } else name_inited = 1;
    
}

void on_connect_btn_clicked(GtkButton *button, GtkTextBuffer *buffer) {
    if(!connect_once) {
        pthread_create(&conn, NULL, server_connect, NULL);

        gtk_text_buffer_set_text(buffer, "=== WELCOME TO CHATTER ===\n", -1);
        //Get iter
        GtkTextIter start;
        gtk_text_buffer_get_end_iter (buffer, &start);

        //Get current time
        char time[50];
        sprintf(time, "Current time: %s", ctime(&now));

        gtk_text_buffer_insert(buffer, &start, time, -1);

        //Get iter
        gtk_text_buffer_get_end_iter (buffer, &start);
        gtk_text_buffer_insert(buffer, &start, "Type :h or :help for Chatter commands\n", -1);

        connect_once = 1;
        connected = 1;
    }

}

void on_msg_send_btn_clicked(GtkButton *button, GtkTextBuffer *buffer) {

    //Thread for sending the messages
    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void*)send_msg_handler, NULL) != 0) {
        g_print("ERROR: pthread\n");
        return EXIT_FAILURE;
    }
}

void on_join_btn_activate(GtkMenuItem *join, GtkWidget *join_dlg) {
    gtk_widget_show(join_dlg);
}

void on_cancel_btn_clicked(GtkButton *cancel, GtkWidget *join_dlg) {
    gtk_widget_hide(join_dlg);
}

// void on_open_file_activate(GtkMenuItem *open, GtkWidget *file_dlg) {
//     gtk_widget_show(file_dlg);
// }

void on_client_main_destroy()
{
    close(sockfd);
    gtk_main_quit();
}

// int main(int argc, char const* argv[]) {
int main(int argc, char *argv[]) {

    time(&now);
    local = localtime(&now);

    strcpy(name, "unnamed");

    GtkBuilder *builder; 
    GtkWidget *window;

    gtk_init(&argc, &argv);

    builder = gtk_builder_new();
    gtk_builder_add_from_file (builder, "../glade/client.glade", NULL);

    window = GTK_WIDGET(gtk_builder_get_object(builder, "client_main"));
    gtk_builder_connect_signals(builder, NULL);

    msg_box = GTK_WIDGET(gtk_builder_get_object(builder, "msg_view"));
    chat_box = GTK_WIDGET(gtk_builder_get_object(builder, "chat_view"));
    room_input = GTK_WIDGET(gtk_builder_get_object(builder, "room_input"));
    pass_input = GTK_WIDGET(gtk_builder_get_object(builder, "pass_input"));

    msg_buffer = gtk_text_view_get_buffer(msg_box);
    chat_buffer = gtk_text_view_get_buffer(chat_box);
    room_buf = gtk_text_view_get_buffer(room_input);
    pass_buf = gtk_text_view_get_buffer(pass_input);
    
    g_object_unref(builder);

    gtk_widget_show_all(window);

    pthread_mutex_init(&mutex, NULL);

    // Thread for receiving messages
    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void*)recv_msg_handler, NULL) != 0) {
        g_print("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    gtk_main();

    close(sockfd);

    return EXIT_SUCCESS;
}

