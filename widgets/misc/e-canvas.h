/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CANVAS_H__
#define __E_CANVAS_H__

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

/* ECanvas - A class derived from canvas for the purpose of adding
 * evolution specific canvas hacks.
 */

#define E_CANVAS_TYPE			(e_canvas_get_type ())
#define E_CANVAS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_CANVAS_TYPE, ECanvas))
#define E_CANVAS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_CANVAS_TYPE, ECanvasClass))
#define E_IS_CANVAS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_CANVAS_TYPE))
#define E_IS_CANVAS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_CANVAS_TYPE))

typedef void		(*ECanvasItemReflowFunc)		(GnomeCanvasItem *item,
								 gint	  flags);

typedef void            (*ECanvasItemSelectionFunc)             (GnomeCanvasItem *item,
								 gint             flags,
								 gpointer         user_data);
/* Returns the same as strcmp does. */
typedef gint            (*ECanvasItemSelectionCompareFunc)      (GnomeCanvasItem *item,
								 gpointer         data1,
								 gpointer         data2,
								 gint             flags);

typedef struct _ECanvas       ECanvas;
typedef struct _ECanvasClass  ECanvasClass;

/* Object flags for items */
enum {
	E_CANVAS_ITEM_NEEDS_REFLOW            = 1 << 13,
	E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW = 1 << 14
};

enum {
	E_CANVAS_ITEM_SELECTION_SELECT        = 1 << 0, /* TRUE = select.  FALSE = unselect. */
	E_CANVAS_ITEM_SELECTION_CURSOR        = 1 << 1, /* TRUE = has become cursor.  FALSE = not cursor. */
	E_CANVAS_ITEM_SELECTION_DELETE_DATA   = 1 << 2
};

typedef struct {
	GnomeCanvasItem *item;
	gpointer         id;
} ECanvasSelectionInfo;

typedef void (*ECanvasItemGrabCancelled) (ECanvas *canvas, GnomeCanvasItem *item, gpointer data);

struct _ECanvas
{
	GnomeCanvas parent;
	gint                   idle_id;
	GList                *selection;
	ECanvasSelectionInfo *cursor;

	GtkWidget            *tooltip_window;
	gint                   visibility_notify_id;
	GtkWidget            *toplevel;

	guint visibility_first : 1;

	/* Input context for dead key support */
	GtkIMContext *im_context;

	ECanvasItemGrabCancelled grab_cancelled_cb;
	guint grab_cancelled_check_id;
	guint32 grab_cancelled_time;
	gpointer grab_cancelled_data;
};

struct _ECanvasClass
{
	GnomeCanvasClass parent_class;
	void (* reflow) (ECanvas *canvas);
};

GType      e_canvas_get_type                             (void);
GtkWidget *e_canvas_new                                  (void);

/* Used to send all of the keystroke events to a specific item as well as
 * GDK_FOCUS_CHANGE events.
 */
void       e_canvas_item_grab_focus                      (GnomeCanvasItem                 *item,
							  gboolean                         widget_too);
void       e_canvas_item_request_reflow                  (GnomeCanvasItem                 *item);
void       e_canvas_item_request_parent_reflow           (GnomeCanvasItem                 *item);
void       e_canvas_item_set_reflow_callback             (GnomeCanvasItem                 *item,
							  ECanvasItemReflowFunc            func);
void       e_canvas_item_set_selection_callback          (GnomeCanvasItem                 *item,
							  ECanvasItemSelectionFunc         func);
void       e_canvas_item_set_selection_compare_callback  (GnomeCanvasItem                 *item,
							  ECanvasItemSelectionCompareFunc  func);
void       e_canvas_item_set_cursor                      (GnomeCanvasItem                 *item,
							  gpointer                         id);
void       e_canvas_item_add_selection                   (GnomeCanvasItem                 *item,
							  gpointer                         id);
void       e_canvas_item_remove_selection                (GnomeCanvasItem                 *item,
							  gpointer                         id);

gint        e_canvas_item_grab                            (ECanvas                         *canvas,
							  GnomeCanvasItem                 *item,
							  guint                            event_mask,
							  GdkCursor                       *cursor,
							  guint32                          etime,
							  ECanvasItemGrabCancelled         cancelled,
							  gpointer                         cancelled_data);
void       e_canvas_item_ungrab                          (ECanvas                         *canvas,
							  GnomeCanvasItem                 *item,
							  guint32                          etime);

/* Not implemented yet. */
void       e_canvas_item_set_cursor_end                  (GnomeCanvasItem                 *item,
							  gpointer                         id);
void       e_canvas_popup_tooltip                        (ECanvas                         *canvas,
							  GtkWidget                       *widget,
							  gint                              x,
							  gint                              y);
void       e_canvas_hide_tooltip                         (ECanvas                         *canvas);

G_END_DECLS

#endif /* __E_CANVAS_H__ */
