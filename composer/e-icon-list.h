/*
 * Copyright (C) 1998, 1999 Free Software Foundation
 * Copyright (C) 2000 Red Hat, Inc.
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
  @NOTATION@
 */

/* GnomeIconList widget - scrollable icon list
 *
 *
 * Authors:
 *   Federico Mena   <federico@ximian.com>
 *   Miguel de Icaza <miguel@ximian.com>
 */

#ifndef _E_ICON_LIST_H_
#define _E_ICON_LIST_H_

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

BEGIN_GNOME_DECLS

#define E_TYPE_ICON_LIST            (e_icon_list_get_type ())
#define E_ICON_LIST(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_ICON_LIST, EIconList))
#define E_ICON_LIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_ICON_LIST, EIconListClass))
#define E_IS_ICON_LIST(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_ICON_LIST))
#define E_IS_ICON_LIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_ICON_LIST))
#define E_ICON_LIST_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), E_TYPE_ICON_LIST, EIconListClass))

typedef struct _EIconList        EIconList;
typedef struct _EIconListPrivate EIconListPrivate;
typedef struct _EIconListClass   EIconListClass;

typedef enum {
	E_ICON_LIST_ICONS,
	E_ICON_LIST_TEXT_BELOW,
	E_ICON_LIST_TEXT_RIGHT
} EIconListMode;

/* This structure has been converted to use public and private parts.  To avoid
 * breaking binary compatibility, the slots for private fields have been
 * replaced with padding.  Please remove these fields when gnome-libs has
 * reached another major version and it is "fine" to break binary compatibility.
 */
struct _EIconList {
	GnomeCanvas canvas;

	/*< private >*/
	EIconListPrivate * _priv;
};

struct _EIconListClass {
	GnomeCanvasClass parent_class;

	void     (*select_icon)    (EIconList *gil, gint num, GdkEvent *event);
	void     (*unselect_icon)  (EIconList *gil, gint num, GdkEvent *event);
	gboolean (*text_changed)   (EIconList *gil, gint num, const char *new_text);
};

enum {
	E_ICON_LIST_IS_EDITABLE	= 1 << 0,
	E_ICON_LIST_STATIC_TEXT	= 1 << 1
};

guint          e_icon_list_get_type            (void) G_GNUC_CONST;

GtkWidget     *e_icon_list_new                 (guint         icon_width,
						int           flags);
void           e_icon_list_construct           (EIconList *gil,
						guint icon_width,
						int flags);


/* To avoid excesive recomputes during insertion/deletion */
void           e_icon_list_freeze              (EIconList *gil);
void           e_icon_list_thaw                (EIconList *gil);


void           e_icon_list_insert              (EIconList *gil,
						int idx,
						const char *icon_filename,
						const char *text);
void           e_icon_list_insert_pixbuf       (EIconList *gil,
						int idx,
						GdkPixbuf *im,
						const char *icon_filename,
						const char *text);

int            e_icon_list_append              (EIconList *gil,
						const char *icon_filename,
						const char *text);
int            e_icon_list_append_pixbuf       (EIconList *gil,
						GdkPixbuf *im,
						const char *icon_filename,
						const char *text);

void           e_icon_list_clear               (EIconList *gil);
void           e_icon_list_remove              (EIconList *gil,
						int idx);

guint          e_icon_list_get_num_icons       (EIconList *gil);


/* Managing the selection */
void           e_icon_list_set_selection_mode  (EIconList *gil,
						GtkSelectionMode mode);
void           e_icon_list_select_icon         (EIconList *gil,
						int idx);
void           e_icon_list_unselect_icon       (EIconList *gil,
						int idx);
int            e_icon_list_unselect_all        (EIconList *gil);
GList *        e_icon_list_get_selection       (EIconList *gil);

/* Setting the spacing values */
void           e_icon_list_set_icon_width      (EIconList *gil,
						int w);
void           e_icon_list_set_row_spacing     (EIconList *gil,
						int pixels);
void           e_icon_list_set_col_spacing     (EIconList *gil,
						int pixels);
void           e_icon_list_set_text_spacing    (EIconList *gil,
						int pixels);
void           e_icon_list_set_icon_border     (EIconList *gil,
						int pixels);
void           e_icon_list_set_separators      (EIconList *gil,
						const char *sep);
/* Icon filename. */
gchar *        e_icon_list_get_icon_filename   (EIconList *gil,
						int idx);
int            e_icon_list_find_icon_from_filename (EIconList *gil,
						    const char *filename);

/* Attaching information to the icons */
void           e_icon_list_set_icon_data       (EIconList *gil,
						int idx, gpointer data);
void           e_icon_list_set_icon_data_full  (EIconList *gil,
						int idx, gpointer data,
						GtkDestroyNotify destroy);
int            e_icon_list_find_icon_from_data (EIconList *gil,
						gpointer data);
gpointer       e_icon_list_get_icon_data       (EIconList *gil,
						int idx);

/* Visibility */
void           e_icon_list_moveto              (EIconList *gil,
						int idx, double yalign);
GtkVisibility  e_icon_list_icon_is_visible     (EIconList *gil,
						int idx);

int            e_icon_list_get_icon_at         (EIconList *gil,
						int x, int y);

int            e_icon_list_get_items_per_line  (EIconList *gil);

END_GNOME_DECLS

#endif /* _GNOME_ICON_LIST_H_ */
