#ifndef E_GUI_UTILS_H
#define E_GUI_UTILS_H

#include <gtk/gtkmenu.h>
#include <gtk/gtkwindow.h>

void  e_popup_menu                   (GtkMenu *menu, GdkEventButton *event);
void  e_auto_kill_popup_menu_on_hide (GtkMenu *menu);
void  e_notice                       (GtkWindow *window, const char *type, const char *format, ...);


#endif /* E_GUI_UTILS_H */
