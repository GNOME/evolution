#ifndef GNOME_APP_LIBS_COLOR_H
#define GNOME_APP_LIBS_COLOR_H

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

void     e_color_init       (void);

/* Return the pixel value for the given red, green and blue */
gulong   e_color_alloc      (gushort red, gushort green, gushort blue);
void     e_color_alloc_name (GtkWidget *widget, const char *name, GdkColor *color);
void     e_color_alloc_gdk  (GtkWidget *widget, GdkColor *color);

extern GdkColor e_white, e_dark_gray, e_black;

G_END_DECLS

#endif /* GNOME_APP_LIBS_COLOR_H */
