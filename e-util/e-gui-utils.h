#ifndef E_GUI_UTILS_H
#define E_GUI_UTILS_H

#include <gtk/gtkmenu.h>
#include <gtk/gtkwindow.h>

void  e_popup_menu                   (GtkMenu *menu, GdkEventButton *event);
void  e_auto_kill_popup_menu_on_hide (GtkMenu *menu);
void  e_notice                       (GtkWindow *window, const char *type, const char *format, ...);
GtkWidget *e_create_image_widget(gchar *name, gchar *string1, gchar *string2, gint int1, gint int2);


#endif /* E_GUI_UTILS_H */
