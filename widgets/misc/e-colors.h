#ifndef GNOME_APP_LIBS_COLOR_H
#define GNOME_APP_LIBS_COLOR_H

#include <glib.h>
#include <gdk/gdk.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

void     e_color_init       (void);

/* Return the pixel value for the given red, green and blue */
int      e_color_alloc      (gushort red, gushort green, gushort blue);
void     e_color_alloc_name (const char *name, GdkColor *color);
void     e_color_alloc_gdk  (GdkColor *color);

extern GdkColor e_white, e_dark_gray, e_black;

END_GNOME_DECLS

#endif /* GNOME_APP_LIBS_COLOR_H */
