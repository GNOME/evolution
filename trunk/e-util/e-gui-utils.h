#ifndef E_GUI_UTILS_H
#define E_GUI_UTILS_H

#include <gtk/gtkwidget.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-control.h>

GtkWidget *e_create_image_widget         (gchar *name, gchar *string1, gchar *string2, gint int1, gint int2);

GtkWidget *e_button_new_with_stock_icon  (const char *label_str, const char *stockid);

GdkPixbuf *e_icon_for_mime_type          (const char *mime_type, int size);

#endif /* E_GUI_UTILS_H */
