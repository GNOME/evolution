#ifndef E_GUI_UTILS_H
#define E_GUI_UTILS_H

#include <gtk/gtkwidget.h>
#include <bonobo/bonobo-ui-component.h>

GtkWidget *e_create_image_widget     (gchar *name, gchar *string1, gchar *string2, gint int1, gint int2);

typedef struct _EPixmap EPixmap;

struct _EPixmap {
	const char *path;
	const char *fname;
	char       *pixbuf;
};

#define E_PIXMAP(path,fname)	{ (path), (fname), NULL }
#define E_PIXMAP_END		{ NULL, NULL, NULL }

/* Takes an array of pixmaps, terminated by (NULL, NULL), and loads into uic */
void e_pixmaps_update (BonoboUIComponent *uic, EPixmap *pixcache);

#endif /* E_GUI_UTILS_H */
