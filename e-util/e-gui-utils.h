#ifndef E_GUI_UTILS_H
#define E_GUI_UTILS_H

#include <gtk/gtkwidget.h>

GdkPixbuf *e_icon_for_mime_type          (const char *mime_type, int size);
GtkWidget *e_create_image_widget         (gchar *name, gchar *string1, gchar *string2, gint int1, gint int2);

#endif /* E_GUI_UTILS_H */
