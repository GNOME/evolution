/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MINICARD_H
#define E_MINICARD_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>
#include <libgnomecanvas/libgnomecanvas.h>

/* Standard GObject macros */
#define E_TYPE_MINICARD \
	(e_minicard_get_type ())
#define E_MINICARD(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MINICARD, EMinicard))
#define E_MINICARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MINICARD, EMinicardClass))
#define E_IS_MINICARD(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MINICARD))
#define E_IS_MINICARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_MINICARD))
#define E_MINICARD_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MINICARD, EMinicardClass))

G_BEGIN_DECLS

typedef struct _EMinicard EMinicard;
typedef struct _EMinicardClass EMinicardClass;

typedef enum _EMinicardFocusType EMinicardFocusType;

enum _EMinicardFocusType {
	E_MINICARD_FOCUS_TYPE_START,
	E_MINICARD_FOCUS_TYPE_END
};

struct _EMinicard
{
	GnomeCanvasGroup parent;

	/* item specific fields */
	EContact *contact;

	GnomeCanvasItem *rect;
	GnomeCanvasItem *header_rect;
	GnomeCanvasItem *header_text;
	GnomeCanvasItem *list_icon;

	GdkPixbuf *list_icon_pixbuf;
	gdouble list_icon_size;

	GList *fields; /* Of type EMinicardField */
	guint needs_remodeling : 1;

	guint changed : 1;

	guint selected : 1;
	guint has_cursor : 1;

	guint has_focus : 1;

	guint editable : 1;

	guint drag_button_down : 1;
	gint drag_button;

	gint button_x;
	gint button_y;

	gdouble width;
	gdouble height;
};

struct _EMinicardClass
{
	GnomeCanvasGroupClass parent_class;

	gint (* selected) (EMinicard *minicard, GdkEvent *event);
	gint (* drag_begin) (EMinicard *minicard, GdkEvent *event);
	void (* open_contact) (EMinicard *minicard, EContact *contact);

	void (* style_updated) (EMinicard *minicard);
};

typedef struct _EMinicardField EMinicardField;

struct _EMinicardField {
	EContactField field;
	GnomeCanvasItem *label;
};

#define E_MINICARD_FIELD(field) ((EMinicardField *)(field))

GType		e_minicard_get_type		(void);
const gchar *	e_minicard_get_card_id		(EMinicard *minicard);
gint		e_minicard_compare		(EMinicard *minicard1,
						 EMinicard *minicard2);
gint		e_minicard_selected		(EMinicard *minicard,
						 GdkEvent *event);
void		e_minicard_activate_editor	(EMinicard *minicard);

G_END_DECLS

#endif /* E_MINICARD_H */
