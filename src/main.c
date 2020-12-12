// Include gtk
#include <gtk/gtk.h>

// Global stuff
GtkWidget* window_main;
GtkWidget* fixed_1;
GtkWidget* button_1;
GtkWidget* label_1;
GtkBuilder* builder;

int main(int argc, char* argv[]) {

    gtk_init(&argc, &argv);

    // Connect to xml file
    builder = gtk_builder_new_from_file("window_main.glade");

    // Build the widgets
    window_main = GTK_WIDGET(gtk_builder_get_object(builder, "window_main"));

    g_signal_connect(window_main, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_builder_connect_signals(builder, NULL);

    fixed_1 = GTK_WIDGET(gtk_builder_get_object(builder, "fixed_1"));
    button_1 = GTK_WIDGET(gtk_builder_get_object(builder, "button_1"));
    label_1 = GTK_WIDGET(gtk_builder_get_object(builder, "label_1"));

    gtk_widget_show(window_main);
    gtk_main();
    return EXIT_SUCCESS;

}

void on_button_1_clicked(GtkButton* b) {
    gtk_label_set_text(GTK_LABEL(label_1), (const char*)"Hello GTK");
}