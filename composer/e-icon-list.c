/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1998, 1999, 2000 Free Software Foundation
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

/*
 * GnomeIconList widget - scrollable icon list
 *
 * Authors:
 *    Federico Mena <federico@nuclecu.unam.mx>
 *    Miguel de Icaza <miguel@nuclecu.unam.mx>
 *
 * Rewrote from scratch from the code written by Federico Mena
 * <federico@nuclecu.unam.mx> to be based on a GnomeCanvas, and
 * to support banding selection and allow inline icon renaming.
 *
 * Redone somewhat by Elliot to support gdk-pixbuf, and to use GArray instead of GList for item storage.
 */

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include "e-icon-list.h"
#include <libgnomeui/gnome-icon-item.h>
#include <libgnomeui/gnome-canvas-pixbuf.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>


/* Aliases to minimize screen use in my laptop */
#define EIL(x)       E_ICON_LIST(x)
#define EIL_CLASS(x) E_ICON_LIST_CLASS(x)
#define IS_EIL(x)    E_IS_ICON_LIST(x)

typedef EIconList Eil;
typedef EIconListClass EilClass;


/* default spacings */
#define DEFAULT_ROW_SPACING  4
#define DEFAULT_COL_SPACING  2
#define DEFAULT_TEXT_SPACING 2
#define DEFAULT_ICON_BORDER  2

/* Autoscroll timeout in milliseconds */
#define SCROLL_TIMEOUT 30


/* Signals */
enum {
	SELECT_ICON,
	UNSELECT_ICON,
	TEXT_CHANGED,
	LAST_SIGNAL
};

typedef enum {
	SYNC_INSERT,
	SYNC_REMOVE
} SyncType;

enum {
	ARG_0,
};

static guint eil_signals[LAST_SIGNAL] = { 0 };


static GnomeCanvasClass *parent_class;


/* Icon structure */
typedef struct {
	/* Icon image and text items */
	GnomeCanvasPixbuf *image;
	GnomeIconTextItem *text;

	/* Filename of the icon file. */
	gchar *icon_filename;

	/* User data and destroy notify function */
	gpointer data;
	GtkDestroyNotify destroy;

	/* ID for the text item's event signal handler */
	guint text_event_id;

	/* Whether the icon is selected, and temporary storage for rubberband
         * selections.
	 */
	guint selected : 1;
	guint tmp_selected : 1;
} Icon;

/* A row of icons */
typedef struct {
	GList *line_icons;
	gint16 y;
	gint16 icon_height, text_height;
} IconLine;

/* Private data of the EIconList structure */
struct _EIconListPrivate {
	/* List of icons */
	GArray *icon_list;

	/* List of rows of icons */
	GList *lines;

	/* Separators used to wrap the text below icons */
	char *separators;

	Icon *last_selected_icon;

	/* Rubberband rectangle */
	GnomeCanvasItem *sel_rect;

	/* Saved event for a pending selection */
	GdkEvent select_pending_event;

	/* Max of the height of all the icon rows and window height */
	int total_height;

	/* Selection mode */
	GtkSelectionMode selection_mode;

	/* A list of integers with the indices of the currently selected icons */
	GList *selection;

	/* Number of icons in the list */
	int icons;

	/* Freeze count */
	int frozen;

	/* Width allocated for icons */
	int icon_width;

	/* Spacing values */
	int row_spacing;
	int col_spacing;
	int text_spacing;
	int icon_border;

	/* Index and pointer to last selected icon */
	int last_selected_idx;

	/* Timeout ID for autoscrolling */
	guint timer_tag;

	/* Change the adjustment value by this amount when autoscrolling */
	int value_diff;

	/* Mouse position for autoscrolling */
	int event_last_x;
	int event_last_y;

	/* Selection start position */
	int sel_start_x;
	int sel_start_y;

	/* Modifier state when the selection began */
	guint sel_state;

	/* Whether the icon texts are editable */
	guint is_editable : 1;

	/* Whether the icon texts need to be copied */
	guint static_text : 1;

	/* Whether the icons need to be laid out */
	guint dirty : 1;

	/* Whether the user is performing a rubberband selection */
	guint selecting : 1;

	/* Whether editing an icon is pending after a button press */
	guint edit_pending : 1;

	/* Whether selection is pending after a button press */
	guint select_pending : 1;

	/* Whether the icon that is pending selection was selected to begin with */
	guint select_pending_was_selected : 1;
};


static inline int
icon_line_height (Eil *eil, IconLine *il)
{
	EIconListPrivate *priv;

	priv = eil->_priv;

	return il->icon_height + il->text_height + priv->row_spacing + priv->text_spacing;
}

static void
icon_get_height (Icon *icon, int *icon_height, int *text_height)
{
	double d_icon_height;
	gtk_object_get(GTK_OBJECT(icon->image), "height", &d_icon_height, NULL);
	*icon_height = d_icon_height;
	*text_height = icon->text->ti->height;
}

static int
eil_get_items_per_line (Eil *eil)
{
	EIconListPrivate *priv;
	int items_per_line;

	priv = eil->_priv;

	items_per_line = GTK_WIDGET (eil)->allocation.width / (priv->icon_width + priv->col_spacing);
	if (items_per_line == 0)
		items_per_line = 1;

	return items_per_line;
}

/**
 * e_icon_list_get_items_per_line:
 * @eil: An icon list.
 *
 * Returns the number of icons that fit in a line or row.
 */
int
e_icon_list_get_items_per_line (EIconList *eil)
{
	g_return_val_if_fail (eil != NULL, 1);
	g_return_val_if_fail (IS_EIL (eil), 1);

	return eil_get_items_per_line (eil);
}

static void
eil_place_icon (Eil *eil, Icon *icon, int x, int y, int icon_height)
{
	EIconListPrivate *priv;
	int x_offset, y_offset;
	double d_icon_image_height;
	double d_icon_image_width;
	int icon_image_height;
	int icon_image_width;

	priv = eil->_priv;

	gtk_object_get(GTK_OBJECT(icon->image), "height", &d_icon_image_height, NULL);
	icon_image_height = d_icon_image_height;
	g_assert(icon_image_height != 0);
	if (icon_height > icon_image_height)
		y_offset = (icon_height - icon_image_height) / 2;
	else
		y_offset = 0;

	gtk_object_get(GTK_OBJECT(icon->image), "width", &d_icon_image_width, NULL);
	icon_image_width = d_icon_image_width;
	g_assert(icon_image_width != 0);
	if (priv->icon_width > icon_image_width)
		x_offset = (priv->icon_width - icon_image_width) / 2;
	else
		x_offset = 0;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->image),
			       "x",  (double) (x + x_offset),
			       "y",  (double) (y + y_offset),
			       NULL);
	gnome_icon_text_item_setxy (icon->text,
				    x,
				    y + icon_height + priv->text_spacing);
}

static void
eil_layout_line (Eil *eil, IconLine *il)
{
	EIconListPrivate *priv;
	GList *l;
	int x;

	priv = eil->_priv;

	x = 0;
	for (l = il->line_icons; l; l = l->next) {
		Icon *icon = l->data;

		eil_place_icon (eil, icon, x, il->y, il->icon_height);
		x += priv->icon_width + priv->col_spacing;
	}
}

static void
eil_add_and_layout_line (Eil *eil, GList *line_icons, int y,
			 int icon_height, int text_height)
{
	EIconListPrivate *priv;
	IconLine *il;

	priv = eil->_priv;

	il = g_new (IconLine, 1);
	il->line_icons = line_icons;
	il->y = y;
	il->icon_height = icon_height;
	il->text_height = text_height;

	eil_layout_line (eil, il);
	priv->lines = g_list_append (priv->lines, il);
}

static void
eil_relayout_icons_at (Eil *eil, int pos, int y)
{
	EIconListPrivate *priv;
	int col, row, text_height, icon_height;
	int items_per_line, n;
	GList *line_icons;

	priv = eil->_priv;
	items_per_line = eil_get_items_per_line (eil);

	col = row = text_height = icon_height = 0;
	line_icons = NULL;

	for (n = pos; n < priv->icon_list->len; n++) {
		Icon *icon = g_array_index(priv->icon_list, Icon*, n);
		int ih, th;

		if (!(n % items_per_line)) {
			if (line_icons) {
				eil_add_and_layout_line (eil, line_icons, y,
							 icon_height, text_height);
				line_icons = NULL;

				y += (icon_height + text_height
				      + priv->row_spacing + priv->text_spacing);
			}

			icon_height = 0;
			text_height = 0;
		}

		icon_get_height (icon, &ih, &th);

		icon_height = MAX (ih, icon_height);
		text_height = MAX (th, text_height);

		line_icons = g_list_append (line_icons, icon);
	}

	if (line_icons)
		eil_add_and_layout_line (eil, line_icons, y, icon_height, text_height);
}

static void
eil_free_line_info (Eil *eil)
{
	EIconListPrivate *priv;
	GList *l;

	priv = eil->_priv;

	for (l = priv->lines; l; l = l->next) {
		IconLine *il = l->data;

		g_list_free (il->line_icons);
		g_free (il);
	}

	g_list_free (priv->lines);
	priv->lines = NULL;
	priv->total_height = 0;
}

static void
eil_free_line_info_from (Eil *eil, int first_line)
{
	EIconListPrivate *priv;
	GList *l, *ll;

	priv = eil->_priv;
	ll = g_list_nth (priv->lines, first_line);

	for (l = ll; l; l = l->next) {
		IconLine *il = l->data;

		g_list_free (il->line_icons);
		g_free (il);
	}

	if (priv->lines) {
		if (ll->prev)
			ll->prev->next = NULL;
		else
			priv->lines = NULL;
	}

	g_list_free (ll);
}

static void
eil_layout_from_line (Eil *eil, int line)
{
	EIconListPrivate *priv;
	GList *l;
	int height;

	priv = eil->_priv;

	eil_free_line_info_from (eil, line);

	height = 0;
	for (l = priv->lines; l; l = l->next) {
		IconLine *il = l->data;

		height += icon_line_height (eil, il);
	}

	eil_relayout_icons_at (eil, line * eil_get_items_per_line (eil), height);
}

static void
eil_layout_all_icons (Eil *eil)
{
	EIconListPrivate *priv;

	priv = eil->_priv;

	if (!GTK_WIDGET_REALIZED (eil))
		return;

	eil_free_line_info (eil);
	eil_relayout_icons_at (eil, 0, 0);
	priv->dirty = FALSE;
}

static void
eil_scrollbar_adjust (Eil *eil)
{
	EIconListPrivate *priv;
	GtkAdjustment *adj;
	GList *l;
	double wx, wy, wx1, wy1, wx2, wy2;
	int height, step_increment;

	priv = eil->_priv;

	if (!GTK_WIDGET_REALIZED (eil))
		return;

	height = 0;
	step_increment = 0;
	for (l = priv->lines; l; l = l->next) {
		IconLine *il = l->data;

		height += icon_line_height (eil, il);

		if (l == priv->lines)
			step_increment = height;
	}

	if (!step_increment)
		step_increment = 10;

	priv->total_height = MAX (height, GTK_WIDGET (eil)->allocation.height);

	gnome_canvas_c2w (GNOME_CANVAS (eil), 0, 0, &wx1, &wy1);
	gnome_canvas_c2w (GNOME_CANVAS (eil),
			  GTK_WIDGET (eil)->allocation.width,
			  priv->total_height,
			  &wx2, &wy2);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (eil),
					wx1, wy1, wx2, wy2);

	wx = wy = 0;
	gnome_canvas_window_to_world (GNOME_CANVAS (eil), 0, 0, &wx, &wy);

	adj = gtk_layout_get_vadjustment (GTK_LAYOUT (eil));

	adj->upper = priv->total_height;
	adj->step_increment = step_increment;
	adj->page_increment = GTK_WIDGET (eil)->allocation.height;
	adj->page_size = GTK_WIDGET (eil)->allocation.height;

	if (wy > adj->upper - adj->page_size)
		wy = adj->upper - adj->page_size;

	adj->value = wy;

	gtk_adjustment_changed (adj);
}

/* Emits the select_icon or unselect_icon signals as appropriate */
static void
emit_select (Eil *eil, int sel, int i, GdkEvent *event)
{
	gtk_signal_emit (GTK_OBJECT (eil),
			 eil_signals[sel ? SELECT_ICON : UNSELECT_ICON],
			 i,
			 event);
}

static int
eil_unselect_all (EIconList *eil, GdkEvent *event, gpointer keep)
{
	EIconListPrivate *priv;
	Icon *icon;
	int i, idx = 0;

	g_return_val_if_fail (eil != NULL, 0);
	g_return_val_if_fail (IS_EIL (eil), 0);

	priv = eil->_priv;

	for (i = 0; i < priv->icon_list->len; i++) {
		icon = g_array_index(priv->icon_list, Icon*, i);

		if (icon == keep)
			idx = i;
		else if (icon->selected)
			emit_select (eil, FALSE, i, event);
	}

	return idx;
}

/**
 * e_icon_list_unselect_all:
 * @eil:   An icon list.
 *
 * Returns: the number of icons in the icon list
 */
int
e_icon_list_unselect_all (EIconList *eil)
{
	return eil_unselect_all (eil, NULL, NULL);
}

static void
sync_selection (Eil *eil, int pos, SyncType type)
{
	GList *list;

	for (list = eil->_priv->selection; list; list = list->next) {
		if (GPOINTER_TO_INT (list->data) >= pos) {
			int i = GPOINTER_TO_INT (list->data);

			switch (type) {
			case SYNC_INSERT:
				list->data = GINT_TO_POINTER (i + 1);
				break;

			case SYNC_REMOVE:
				list->data = GINT_TO_POINTER (i - 1);
				break;

			default:
				g_assert_not_reached ();
			}
		}
	}
}

static int
eil_icon_to_index (Eil *eil, Icon *icon)
{
	EIconListPrivate *priv;
	int n;

	priv = eil->_priv;

	for (n = 0; n < priv->icon_list->len; n++)
		if (g_array_index(priv->icon_list, Icon*, n) == icon)
			return n;

	g_assert_not_reached ();
	return -1; /* Shut up the compiler */
}

/* Event handler for icons when we are in SINGLE or BROWSE mode */
static gint
selection_one_icon_event (Eil *eil, Icon *icon, int idx, int on_text, GdkEvent *event)
{
	EIconListPrivate *priv;
	GnomeIconTextItem *text;
	int retval;

	priv = eil->_priv;
	retval = FALSE;

	/* We use a separate variable and ref the object because it may be
	 * destroyed by one of the signal handlers.
	 */
	text = icon->text;
	gtk_object_ref (GTK_OBJECT (text));

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		priv->edit_pending = FALSE;
		priv->select_pending = FALSE;

		/* Ignore wheel mouse clicks for now */
		if (event->button.button > 3)
			break;

		if (!icon->selected) {
			eil_unselect_all (eil, NULL, NULL);
			emit_select (eil, TRUE, idx, event);
		} else {
			if (priv->selection_mode == GTK_SELECTION_SINGLE
			    && (event->button.state & GDK_CONTROL_MASK))
				emit_select (eil, FALSE, idx, event);
			else if (on_text && priv->is_editable && event->button.button == 1)
				priv->edit_pending = TRUE;
			else
				emit_select (eil, TRUE, idx, event);
		}

		retval = TRUE;
		break;

	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		/* Ignore wheel mouse clicks for now */
		if (event->button.button > 3)
			break;

		emit_select (eil, TRUE, idx, event);
		retval = TRUE;
		break;

	case GDK_BUTTON_RELEASE:
		if (priv->edit_pending) {
			gnome_icon_text_item_start_editing (text);
			priv->edit_pending = FALSE;
		}

		retval = TRUE;
		break;

	default:
		break;
	}

	/* If the click was on the text and we actually did something, stop the
	 * icon text item's own handler from executing.
	 */
	if (on_text && retval)
		gtk_signal_emit_stop_by_name (GTK_OBJECT (text), "event");

	gtk_object_unref (GTK_OBJECT (text));

	return retval;
}

/* Handles range selections when clicking on an icon */
static void
select_range (Eil *eil, Icon *icon, int idx, GdkEvent *event)
{
	EIconListPrivate *priv;
	int a, b;
	Icon *i;

	priv = eil->_priv;

	if (priv->last_selected_idx == -1) {
		priv->last_selected_idx = idx;
		priv->last_selected_icon = icon;
	}

	if (idx < priv->last_selected_idx) {
		a = idx;
		b = priv->last_selected_idx;
	} else {
		a = priv->last_selected_idx;
		b = idx;
	}

	for (; a <= b; a++) {
		i = g_array_index(priv->icon_list, Icon*, a);

		if (!i->selected)
			emit_select (eil, TRUE, a, NULL);
	}

	/* Actually notify the client of the event */
	emit_select (eil, TRUE, idx, event);
}

/* Handles icon selection for MULTIPLE or EXTENDED selection modes */
static void
do_select_many (Eil *eil, Icon *icon, int idx, GdkEvent *event, int use_event)
{
	EIconListPrivate *priv;
	int range, additive;

	priv = eil->_priv;

	range = (event->button.state & GDK_SHIFT_MASK) != 0;
	additive = (event->button.state & GDK_CONTROL_MASK) != 0;

	if (!additive) {
		if (icon->selected)
			eil_unselect_all (eil, NULL, icon);
		else
			eil_unselect_all (eil, NULL, NULL);
	}

	if (!range) {
		if (additive)
			emit_select (eil, !icon->selected, idx, use_event ? event : NULL);
		else
			emit_select (eil, TRUE, idx, use_event ? event : NULL);

		priv->last_selected_idx = idx;
		priv->last_selected_icon = icon;
	} else
		select_range (eil, icon, idx, use_event ? event : NULL);
}

/* Event handler for icons when we are in MULTIPLE or EXTENDED mode */
static gint
selection_many_icon_event (Eil *eil, Icon *icon, int idx, int on_text, GdkEvent *event)
{
	EIconListPrivate *priv;
	GnomeIconTextItem *text;
	int retval;
	int additive, range;
	int do_select;

	priv = eil->_priv;
	retval = FALSE;

	/* We use a separate variable and ref the object because it may be
	 * destroyed by one of the signal handlers.
	 */
	text = icon->text;
	gtk_object_ref (GTK_OBJECT (text));

	range = (event->button.state & GDK_SHIFT_MASK) != 0;
	additive = (event->button.state & GDK_CONTROL_MASK) != 0;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		priv->edit_pending = FALSE;
		priv->select_pending = FALSE;

		/* Ignore wheel mouse clicks for now */
		if (event->button.button > 3)
			break;

		do_select = TRUE;

		if (additive || range) {
			if (additive && !range) {
				priv->select_pending = TRUE;
				priv->select_pending_event = *event;
				priv->select_pending_was_selected = icon->selected;

				/* We have to emit this so that the client will
				 * know about the click.
				 */
				emit_select (eil, TRUE, idx, event);
				do_select = FALSE;
			}
		} else if (icon->selected) {
			priv->select_pending = TRUE;
			priv->select_pending_event = *event;
			priv->select_pending_was_selected = icon->selected;

			if (on_text && priv->is_editable && event->button.button == 1)
				priv->edit_pending = TRUE;

			emit_select (eil, TRUE, idx, event);
			do_select = FALSE;
		}
#if 0
		} else if (icon->selected && on_text && priv->is_editable
			   && event->button.button == 1) {
			priv->edit_pending = TRUE;
			do_select = FALSE;
		}
#endif

		if (do_select)
			do_select_many (eil, icon, idx, event, TRUE);

		retval = TRUE;
		break;

	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		/* Ignore wheel mouse clicks for now */
		if (event->button.button > 3)
			break;

		emit_select (eil, TRUE, idx, event);
		retval = TRUE;
		break;

	case GDK_BUTTON_RELEASE:
		if (priv->select_pending) {
			icon->selected = priv->select_pending_was_selected;
			do_select_many (eil, icon, idx, &priv->select_pending_event, FALSE);
			priv->select_pending = FALSE;
			retval = TRUE;
		}

		if (priv->edit_pending) {
			gnome_icon_text_item_start_editing (text);
			priv->edit_pending = FALSE;
			retval = TRUE;
		}
#if 0
		if (priv->select_pending) {
			icon->selected = priv->select_pending_was_selected;
			do_select_many (eil, icon, idx, &priv->select_pending_event);
			priv->select_pending = FALSE;
			retval = TRUE;
		} else if (priv->edit_pending) {
			gnome_icon_text_item_start_editing (text);
			priv->edit_pending = FALSE;
			retval = TRUE;
		}
#endif

		break;

	default:
		break;
	}

	/* If the click was on the text and we actually did something, stop the
	 * icon text item's own handler from executing.
	 */
	if (on_text && retval)
		gtk_signal_emit_stop_by_name (GTK_OBJECT (text), "event");

	gtk_object_unref (GTK_OBJECT (text));

	return retval;
}

/* Event handler for icons in the icon list */
static gint
icon_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	Icon *icon;
	Eil *eil;
	EIconListPrivate *priv;
	int idx;
	int on_text;

	icon = data;
	eil = EIL (item->canvas);
	priv = eil->_priv;
	idx = eil_icon_to_index (eil, icon);
	on_text = item == GNOME_CANVAS_ITEM (icon->text);

	switch (priv->selection_mode) {
	case GTK_SELECTION_SINGLE:
	case GTK_SELECTION_BROWSE:
		return selection_one_icon_event (eil, icon, idx, on_text, event);

	case GTK_SELECTION_MULTIPLE:
	case GTK_SELECTION_EXTENDED:
		return selection_many_icon_event (eil, icon, idx, on_text, event);

	default:
		g_assert_not_reached ();
		return FALSE; /* Shut up the compiler */
	}
}

/* Handler for the editing_started signal of an icon text item.  We block the
 * event handler so that it will not be called while the text is being edited.
 */
static void
editing_started (GnomeIconTextItem *iti, gpointer data)
{
	Icon *icon;

	icon = data;
	gtk_signal_handler_block (GTK_OBJECT (iti), icon->text_event_id);
	eil_unselect_all (EIL (GNOME_CANVAS_ITEM (iti)->canvas), NULL, icon);
}

/* Handler for the editing_stopped signal of an icon text item.  We unblock the
 * event handler so that we can get events from it again.
 */
static void
editing_stopped (GnomeIconTextItem *iti, gpointer data)
{
	Icon *icon;

	icon = data;
	gtk_signal_handler_unblock (GTK_OBJECT (iti), icon->text_event_id);
}

static gboolean
text_changed (GnomeCanvasItem *item, Icon *icon)
{
	Eil *eil;
	gboolean accept;
	int idx;

	eil = EIL (item->canvas);
	accept = TRUE;

	idx = eil_icon_to_index (eil, icon);
	gtk_signal_emit (GTK_OBJECT (eil),
			 eil_signals[TEXT_CHANGED],
			 idx, gnome_icon_text_item_get_text (icon->text),
			 &accept);

	return accept;
}

static void
height_changed (GnomeCanvasItem *item, Icon *icon)
{
	Eil *eil;
	EIconListPrivate *priv;
	int n;

	eil = EIL (item->canvas);
	priv = eil->_priv;

	if (!GTK_WIDGET_REALIZED (eil))
		return;

	if (priv->frozen) {
		priv->dirty = TRUE;
		return;
	}

	n = eil_icon_to_index (eil, icon);
	eil_layout_from_line (eil, n / eil_get_items_per_line (eil));
	eil_scrollbar_adjust (eil);
}

static Icon *
icon_new_from_pixbuf (EIconList *eil, GdkPixbuf *im,
		      const char *icon_filename, const char *text)
{
	EIconListPrivate *priv;
	GnomeCanvas *canvas;
	GnomeCanvasGroup *group;
	Icon *icon;

	priv = eil->_priv;
	canvas = GNOME_CANVAS (eil);
	group = GNOME_CANVAS_GROUP (canvas->root);

	icon = g_new0 (Icon, 1);

	if (icon_filename)
		icon->icon_filename = g_strdup (icon_filename);
	else
		icon->icon_filename = NULL;

	g_print ("%d,%d\n", gdk_pixbuf_get_width (im), gdk_pixbuf_get_height (im));
	icon->image = GNOME_CANVAS_PIXBUF (gnome_canvas_item_new (
		group,
		gnome_canvas_pixbuf_get_type (),
		"x", 0.0,
		"y", 0.0,
		"width", (double) gdk_pixbuf_get_width (im),
		"height", (double) gdk_pixbuf_get_height (im),
		"pixbuf", im,
		NULL));

	icon->text = GNOME_ICON_TEXT_ITEM (gnome_canvas_item_new (
		group,
		gnome_icon_text_item_get_type (),
		NULL));

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->text),
			       "use_broken_event_handling", FALSE,
			       NULL);

	gnome_icon_text_item_configure (icon->text,
					0, 0, priv->icon_width, 
					"-adobe-helvetica-medium-r-normal-*-*-120-*-*-p-*-iso8859-1",
					text, priv->is_editable, priv->static_text);

	gtk_signal_connect (GTK_OBJECT (icon->image), "event",
			    GTK_SIGNAL_FUNC (icon_event),
			    icon);
	icon->text_event_id = gtk_signal_connect (GTK_OBJECT (icon->text), "event",
						  GTK_SIGNAL_FUNC (icon_event),
						  icon);

	gtk_signal_connect (GTK_OBJECT (icon->text), "editing_started",
			    GTK_SIGNAL_FUNC (editing_started),
			    icon);
	gtk_signal_connect (GTK_OBJECT (icon->text), "editing_stopped",
			    GTK_SIGNAL_FUNC (editing_stopped),
			    icon);

	gtk_signal_connect (GTK_OBJECT (icon->text), "text_changed",
			    GTK_SIGNAL_FUNC (text_changed),
			    icon);
	gtk_signal_connect (GTK_OBJECT (icon->text), "height_changed",
			    GTK_SIGNAL_FUNC (height_changed),
			    icon);

	return icon;
}

static Icon *
icon_new (Eil *eil, const char *icon_filename, const char *text)
{
	GdkPixbuf *im;
	Icon *retval;

	if (icon_filename)
		im = gdk_pixbuf_new_from_file (icon_filename);
	else
		im = NULL;

	retval = icon_new_from_pixbuf (eil, im, icon_filename, text);

	if(im)
		gdk_pixbuf_unref(im);

	return retval;
}

static int
icon_list_append (Eil *eil, Icon *icon)
{
	EIconListPrivate *priv;
	int pos;

	priv = eil->_priv;

	pos = priv->icons++;
	g_array_append_val(priv->icon_list, icon);

	switch (priv->selection_mode) {
	case GTK_SELECTION_BROWSE:
		e_icon_list_select_icon (eil, 0);
		break;

	default:
		break;
	}

	if (!priv->frozen) {
		/* FIXME: this should only layout the last line */
		eil_layout_all_icons (eil);
		eil_scrollbar_adjust (eil);
	} else
		priv->dirty = TRUE;

	return priv->icons - 1;
}

static void
icon_list_insert (Eil *eil, int pos, Icon *icon)
{
	EIconListPrivate *priv;

	priv = eil->_priv;

	if (pos == priv->icons) {
		icon_list_append (eil, icon);
		return;
	}

	g_array_insert_val(priv->icon_list, pos, icon);
	priv->icons++;

	switch (priv->selection_mode) {
	case GTK_SELECTION_BROWSE:
		e_icon_list_select_icon (eil, 0);
		break;

	default:
		break;
	}

	if (!priv->frozen) {
		/* FIXME: this should only layout the lines from then one
		 * containing the Icon to the end.
		 */
		eil_layout_all_icons (eil);
		eil_scrollbar_adjust (eil);
	} else
		priv->dirty = TRUE;

	sync_selection (eil, pos, SYNC_INSERT);
}

/**
 * e_icon_list_insert_pixbuf:
 * @eil:      An icon list.
 * @pos:      Position at which the new icon should be inserted.
 * @im:       Pixbuf image with the icon image.
 * @filename: Filename of the image file.
 * @text:     Text to be used for the icon's caption.
 *
 * Inserts an icon in the specified icon list.  The icon is created from the
 * specified Imlib image, and it is inserted at the @pos index.
 */
void
e_icon_list_insert_pixbuf (EIconList *eil, int pos, GdkPixbuf *im,
			       const char *icon_filename, const char *text)
{
	Icon *icon;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (im != NULL);

	icon = icon_new_from_pixbuf (eil, im, icon_filename, text);
	icon_list_insert (eil, pos, icon);
	return;
}

/**
 * e_icon_list_insert:
 * @eil:           An icon list.
 * @pos:           Position at which the new icon should be inserted.
 * @icon_filename: Name of the file that holds the icon's image.
 * @text:          Text to be used for the icon's caption.
 *
 * Inserts an icon in the specified icon list.  The icon's image is loaded
 * from the specified file, and it is inserted at the @pos index.
 */
void
e_icon_list_insert (EIconList *eil, int pos, const char *icon_filename, const char *text)
{
	Icon *icon;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	icon = icon_new (eil, icon_filename, text);
	icon_list_insert (eil, pos, icon);
	return;
}

/**
 * e_icon_list_append_pixbuf:
 * @eil:      An icon list.
 * @im:       Pixbuf image with the icon image.
 * @filename: Filename of the image file.
 * @text:     Text to be used for the icon's caption.
 *
 * Appends an icon to the specified icon list.  The icon is created from
 * the specified Imlib image.
 */
int
e_icon_list_append_pixbuf (EIconList *eil, GdkPixbuf *im,
			       const char *icon_filename, const char *text)
{
	Icon *icon;

	g_return_val_if_fail (eil != NULL, -1);
	g_return_val_if_fail (IS_EIL (eil), -1);
	g_return_val_if_fail (im != NULL, -1);

	icon = icon_new_from_pixbuf (eil, im, icon_filename, text);
	return icon_list_append (eil, icon);
}

/**
 * e_icon_list_append:
 * @eil:           An icon list.
 * @icon_filename: Name of the file that holds the icon's image.
 * @text:          Text to be used for the icon's caption.
 *
 * Appends an icon to the specified icon list.  The icon's image is loaded from
 * the specified file, and it is inserted at the @pos index.
 */
int
e_icon_list_append (EIconList *eil, const char *icon_filename,
			const char *text)
{
	Icon *icon;

	g_return_val_if_fail (eil != NULL, -1);
	g_return_val_if_fail (IS_EIL (eil), -1);

	icon = icon_new (eil, icon_filename, text);
	return icon_list_append (eil, icon);
}

static void
icon_destroy (Icon *icon)
{
	if (icon->destroy)
		(* icon->destroy) (icon->data);

	g_free (icon->icon_filename);

	gtk_object_destroy (GTK_OBJECT (icon->image));
	gtk_object_destroy (GTK_OBJECT (icon->text));
	g_free (icon);
}

/**
 * e_icon_list_remove:
 * @eil: An icon list.
 * @pos: Index of the icon that should be removed.
 *
 * Removes the icon at index position @pos.  If a destroy handler was specified
 * for that icon, it will be called with the appropriate data.
 */
void
e_icon_list_remove (EIconList *eil, int pos)
{
	EIconListPrivate *priv;
	int was_selected;
	Icon *icon;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (pos >= 0 && pos < eil->_priv->icons);

	priv = eil->_priv;

	was_selected = FALSE;

	icon = g_array_index(priv->icon_list, Icon*, pos);

	if (icon->selected) {
		was_selected = TRUE;

		switch (priv->selection_mode) {
		case GTK_SELECTION_SINGLE:
		case GTK_SELECTION_BROWSE:
		case GTK_SELECTION_MULTIPLE:
		case GTK_SELECTION_EXTENDED:
			e_icon_list_unselect_icon (eil, pos);
			break;

		default:
			break;
		}
	}

	g_array_remove_index(priv->icon_list, pos);
	priv->icons--;

	sync_selection (eil, pos, SYNC_REMOVE);

	if (was_selected) {
		switch (priv->selection_mode) {
		case GTK_SELECTION_BROWSE:
			if (pos == priv->icons)
				e_icon_list_select_icon (eil, pos - 1);
			else
				e_icon_list_select_icon (eil, pos);

			break;

		default:
			break;
		}
	}

	if (priv->icons >= priv->last_selected_idx)
		priv->last_selected_idx = -1;

	if (priv->last_selected_icon == icon)
		priv->last_selected_icon = NULL;

	icon_destroy (icon);

	if (!priv->frozen) {
		/* FIXME: Optimize, only re-layout from pos to end */
		eil_layout_all_icons (eil);
		eil_scrollbar_adjust (eil);
	} else
		priv->dirty = TRUE;
}

/**
 * e_icon_list_clear:
 * @eil: An icon list.
 *
 * Clears the contents for the icon list by removing all the icons.  If destroy
 * handlers were specified for any of the icons, they will be called with the
 * appropriate data.
 */
void
e_icon_list_clear (EIconList *eil)
{
	EIconListPrivate *priv;
	int i;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	priv = eil->_priv;

	for (i = 0; i < priv->icon_list->len; i++)
		icon_destroy (g_array_index (priv->icon_list, Icon*, i));

	eil_free_line_info (eil);

	g_list_free (priv->selection);
	priv->selection = NULL;
	g_array_set_size(priv->icon_list, 0);
	priv->icons = 0;
	priv->last_selected_idx = -1;
	priv->last_selected_icon = NULL;

	if (!priv->frozen) {
		eil_layout_all_icons (eil);
		eil_scrollbar_adjust (eil);
	} else
		priv->dirty = TRUE;
}

static void
eil_destroy (GtkObject *object)
{
	Eil *eil;

	/* remember, destroy can be run multiple times! */

	eil = EIL (object);

	g_free (eil->_priv->separators);
	eil->_priv->separators = NULL;

	eil->_priv->frozen = 1;
	eil->_priv->dirty  = TRUE;
	if(eil->_priv->icon_list) {
		e_icon_list_clear (eil);
		g_array_free(eil->_priv->icon_list, TRUE);
	}
	eil->_priv->icon_list = NULL;

	if (eil->_priv->timer_tag != 0) {
		gtk_timeout_remove (eil->_priv->timer_tag);
		eil->_priv->timer_tag = 0;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
eil_finalize (GtkObject *object)
{
	Eil *eil;

	eil = EIL (object);

	g_free (eil->_priv);
	eil->_priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(*GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
select_icon (Eil *eil, int pos, GdkEvent *event)
{
	EIconListPrivate *priv;
	gint i;
	Icon *icon;

	priv = eil->_priv;

	switch (priv->selection_mode) {
	case GTK_SELECTION_SINGLE:
	case GTK_SELECTION_BROWSE:
		i = 0;

		for (i = 0; i < priv->icon_list->len; i++) {
			icon = g_array_index (priv->icon_list, Icon*, i);

			if (i != pos && icon->selected)
				emit_select (eil, FALSE, i, event);
		}

		emit_select (eil, TRUE, pos, event);
		break;

	case GTK_SELECTION_MULTIPLE:
	case GTK_SELECTION_EXTENDED:
		emit_select (eil, TRUE, pos, event);
		break;

	default:
		break;
	}
}

/**
 * e_icon_list_select_icon:
 * @eil: An icon list.
 * @pos: Index of the icon to be selected.
 *
 * Selects the icon at the index specified by @pos.
 */
void
e_icon_list_select_icon (EIconList *eil, int pos)
{
	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (pos >= 0 && pos < eil->_priv->icons);

	select_icon (eil, pos, NULL);
}

static void
unselect_icon (Eil *eil, int pos, GdkEvent *event)
{
	EIconListPrivate *priv;

	priv = eil->_priv;

	switch (priv->selection_mode) {
	case GTK_SELECTION_SINGLE:
	case GTK_SELECTION_BROWSE:
	case GTK_SELECTION_MULTIPLE:
	case GTK_SELECTION_EXTENDED:
		emit_select (eil, FALSE, pos, event);
		break;

	default:
		break;
	}
}

/**
 * e_icon_list_unselect_icon:
 * @eil: An icon list.
 * @pos: Index of the icon to be unselected.
 *
 * Unselects the icon at the index specified by @pos.
 */
void
e_icon_list_unselect_icon (EIconList *eil, int pos)
{
	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (pos >= 0 && pos < eil->_priv->icons);

	unselect_icon (eil, pos, NULL);
}

static void
eil_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	requisition->width = 1;
	requisition->height = 1;
}

static void
eil_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	Eil *eil;
	EIconListPrivate *priv;

	eil = EIL (widget);
	priv = eil->_priv;

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	if (priv->frozen)
		return;

	eil_layout_all_icons (eil);
}

static void
eil_realize (GtkWidget *widget)
{
	Eil *eil;
	EIconListPrivate *priv;
	GtkStyle *style;

	eil = EIL (widget);
	priv = eil->_priv;

	priv->frozen++;

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	priv->frozen--;

	/* Change the style to use the base color as the background */

	style = gtk_style_copy (gtk_widget_get_style (widget));
	style->bg[GTK_STATE_NORMAL] = style->base[GTK_STATE_NORMAL];
	gtk_widget_set_style (widget, style);

	gdk_window_set_background (GTK_LAYOUT (eil)->bin_window,
				   &widget->style->bg[GTK_STATE_NORMAL]);

	if (priv->frozen)
		return;

	if (priv->dirty) {
		eil_layout_all_icons (eil);
		eil_scrollbar_adjust (eil);
	}
}

static void
real_select_icon (Eil *eil, gint num, GdkEvent *event)
{
	EIconListPrivate *priv;
	Icon *icon;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (num >= 0 && num < eil->_priv->icons);

	priv = eil->_priv;

	icon = g_array_index (priv->icon_list, Icon*, num);

	if (icon->selected)
		return;

	icon->selected = TRUE;
	gnome_icon_text_item_select (icon->text, TRUE);
	priv->selection = g_list_append (priv->selection, GINT_TO_POINTER (num));
}

static void
real_unselect_icon (Eil *eil, gint num, GdkEvent *event)
{
	EIconListPrivate *priv;
	Icon *icon;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (num >= 0 && num < eil->_priv->icons);

	priv = eil->_priv;

	icon = g_array_index (priv->icon_list, Icon*, num);

	if (!icon->selected)
		return;

	icon->selected = FALSE;
	gnome_icon_text_item_select (icon->text, FALSE);
	priv->selection = g_list_remove (priv->selection, GINT_TO_POINTER (num));
}

/* Saves the selection of the icon list to temporary storage */
static void
store_temp_selection (Eil *eil)
{
	EIconListPrivate *priv;
	int i;
	Icon *icon;

	priv = eil->_priv;

	for (i = 0; i < priv->icon_list->len; i++) {
		icon = g_array_index(priv->icon_list, Icon*, i);

		icon->tmp_selected = icon->selected;
	}
}

#define gray50_width 2
#define gray50_height 2
static const char gray50_bits[] = {
  0x02, 0x01, };

/* Button press handler for the icon list */
static gint
eil_button_press (GtkWidget *widget, GdkEventButton *event)
{
	Eil *eil;
	EIconListPrivate *priv;
	int only_one;
	GdkBitmap *stipple;
	double tx, ty;

	eil = EIL (widget);
	priv = eil->_priv;

	/* Invoke the canvas event handler and see if an item picks up the event */

	if ((* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event))
		return TRUE;

	if (!(event->type == GDK_BUTTON_PRESS
	      && event->button == 1
	      && priv->selection_mode != GTK_SELECTION_BROWSE))
		return FALSE;

	only_one = priv->selection_mode == GTK_SELECTION_SINGLE;

	if (only_one || (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == 0)
		eil_unselect_all (eil, NULL, NULL);

	if (only_one)
		return TRUE;

	if (priv->selecting)
		return FALSE;

	gnome_canvas_window_to_world (GNOME_CANVAS (eil), event->x, event->y, &tx, &ty);
	priv->sel_start_x = tx;
	priv->sel_start_y = ty;
	priv->sel_state = event->state;
	priv->selecting = TRUE;

	store_temp_selection (eil);

	stipple = gdk_bitmap_create_from_data (NULL, gray50_bits, gray50_width, gray50_height);
	priv->sel_rect = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (eil)),
						gnome_canvas_rect_get_type (),
						"x1", tx,
						"y1", ty,
						"x2", tx,
						"y2", ty,
						"outline_color", "black",
						"width_pixels", 1,
						"outline_stipple", stipple,
						NULL);
	gdk_bitmap_unref (stipple);

	gnome_canvas_item_grab (priv->sel_rect, GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
				NULL, event->time);

	return TRUE;
}

/* Returns whether the specified icon is at least partially inside the specified
 * rectangle.
 */
static int
icon_is_in_area (Icon *icon, int x1, int y1, int x2, int y2)
{
	double ix1, iy1, ix2, iy2;

	if (x1 == x2 && y1 == y2)
		return FALSE;

	gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (icon->image), &ix1, &iy1, &ix2, &iy2);

	if (ix1 <= x2 && iy1 <= y2 && ix2 >= x1 && iy2 >= y1)
		return TRUE;

	gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (icon->text), &ix1, &iy1, &ix2, &iy2);

	if (ix1 <= x2 && iy1 <= y2 && ix2 >= x1 && iy2 >= y1)
		return TRUE;

	return FALSE;
}

/* Updates the rubberband selection to the specified point */
static void
update_drag_selection (Eil *eil, int x, int y)
{
	EIconListPrivate *priv;
	int x1, x2, y1, y2;
	int i;
	Icon *icon;
	int additive, invert;

	priv = eil->_priv;

	/* Update rubberband */

	if (priv->sel_start_x < x) {
		x1 = priv->sel_start_x;
		x2 = x;
	} else {
		x1 = x;
		x2 = priv->sel_start_x;
	}

	if (priv->sel_start_y < y) {
		y1 = priv->sel_start_y;
		y2 = y;
	} else {
		y1 = y;
		y2 = priv->sel_start_y;
	}

	if (x1 < 0)
		x1 = 0;

	if (y1 < 0)
		y1 = 0;

	if (x2 >= GTK_WIDGET (eil)->allocation.width)
		x2 = GTK_WIDGET (eil)->allocation.width - 1;

	if (y2 >= priv->total_height)
		y2 = priv->total_height - 1;

	gnome_canvas_item_set (priv->sel_rect,
			       "x1", (double) x1,
			       "y1", (double) y1,
			       "x2", (double) x2,
			       "y2", (double) y2,
			       NULL);

	/* Select or unselect icons as appropriate */

	additive = priv->sel_state & GDK_SHIFT_MASK;
	invert = priv->sel_state & GDK_CONTROL_MASK;

	for (i = 0; i < priv->icon_list->len; i++) {
		icon = g_array_index(priv->icon_list, Icon*, i);

		if (icon_is_in_area (icon, x1, y1, x2, y2)) {
			if (invert) {
				if (icon->selected == icon->tmp_selected)
					emit_select (eil, !icon->selected, i, NULL);
			} else if (additive) {
				if (!icon->selected)
					emit_select (eil, TRUE, i, NULL);
			} else {
				if (!icon->selected)
					emit_select (eil, TRUE, i, NULL);
			}
		} else if (icon->selected != icon->tmp_selected)
			emit_select (eil, icon->tmp_selected, i, NULL);
	}
}

/* Button release handler for the icon list */
static gint
eil_button_release (GtkWidget *widget, GdkEventButton *event)
{
	Eil *eil;
	EIconListPrivate *priv;
	double x, y;

	eil = EIL (widget);
	priv = eil->_priv;

	if (!priv->selecting)
		return (* GTK_WIDGET_CLASS (parent_class)->button_release_event) (widget, event);

	if (event->button != 1)
		return FALSE;

	gnome_canvas_window_to_world (GNOME_CANVAS (eil), event->x, event->y, &x, &y);
	update_drag_selection (eil, x, y);
	gnome_canvas_item_ungrab (priv->sel_rect, event->time);

	gtk_object_destroy (GTK_OBJECT (priv->sel_rect));
	priv->sel_rect = NULL;
	priv->selecting = FALSE;

	if (priv->timer_tag != 0) {
		gtk_timeout_remove (priv->timer_tag);
		priv->timer_tag = 0;
	}

	return TRUE;
}

/* Autoscroll timeout handler for the icon list */
static gint
scroll_timeout (gpointer data)
{
	Eil *eil;
	EIconListPrivate *priv;
	GtkAdjustment *adj;
	double x, y;
	int value;

	eil = data;
	priv = eil->_priv;

	GDK_THREADS_ENTER ();

	adj = gtk_layout_get_vadjustment (GTK_LAYOUT (eil));

	value = adj->value + priv->value_diff;
	if (value > adj->upper - adj->page_size)
		value = adj->upper - adj->page_size;

	gtk_adjustment_set_value (adj, value);

	gnome_canvas_window_to_world (GNOME_CANVAS (eil),
				      priv->event_last_x, priv->event_last_y,
				      &x, &y);
	update_drag_selection (eil, x, y);

	GDK_THREADS_LEAVE();

	return TRUE;
}

/* Motion event handler for the icon list */
static gint
eil_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
	Eil *eil;
	EIconListPrivate *priv;
	double x, y;

	eil = EIL (widget);
	priv = eil->_priv;

	if (!priv->selecting)
		return (* GTK_WIDGET_CLASS (parent_class)->motion_notify_event) (widget, event);

	gnome_canvas_window_to_world (GNOME_CANVAS (eil), event->x, event->y, &x, &y);
	update_drag_selection (eil, x, y);

	/* If we are out of bounds, schedule a timeout that will do the scrolling */

	if (event->y < 0 || event->y > widget->allocation.height) {
		if (priv->timer_tag == 0)
			priv->timer_tag = gtk_timeout_add (SCROLL_TIMEOUT, scroll_timeout, eil);

		if (event->y < 0)
			priv->value_diff = event->y;
		else
			priv->value_diff = event->y - widget->allocation.height;

		priv->event_last_x = event->x;
		priv->event_last_y = event->y;

		/* Make the steppings be relative to the mouse distance from the
		 * canvas.  Also notice the timeout above is small to give a
		 * more smooth movement.
		 */
		priv->value_diff /= 5;
	} else if (priv->timer_tag != 0) {
		gtk_timeout_remove (priv->timer_tag);
		priv->timer_tag = 0;
	}

	return TRUE;
}

typedef gboolean (*xGtkSignal_BOOL__INT_POINTER) (GtkObject * object,
						  gint     arg1,
						  gpointer arg2,
						  gpointer user_data);
static void
xgtk_marshal_BOOL__INT_POINTER (GtkObject *object, GtkSignalFunc func, gpointer func_data,
				GtkArg *args)
{
  xGtkSignal_BOOL__INT_POINTER rfunc;
  gboolean *return_val;

  return_val = GTK_RETLOC_BOOL (args[2]);
  rfunc = (xGtkSignal_BOOL__INT_POINTER) func;
  *return_val = (*rfunc) (object,
			  GTK_VALUE_INT (args[0]),
			  GTK_VALUE_POINTER (args[1]),
			  func_data);
}

static void
eil_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EIconList *eil;

	eil = E_ICON_LIST (object);

	switch (arg_id) {
	default:
		break;
	}
}

static void
eil_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EIconList *eil;
	EIconListPrivate *priv;

	eil = E_ICON_LIST (object);
	priv = eil->_priv;

	switch (arg_id) {
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
eil_class_init (EilClass *eil_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkLayoutClass *layout_class;
	GnomeCanvasClass *canvas_class;

	object_class = (GtkObjectClass *)   eil_class;
	widget_class = (GtkWidgetClass *)   eil_class;
	layout_class = (GtkLayoutClass *)   eil_class;
	canvas_class = (GnomeCanvasClass *) eil_class;

	parent_class = gtk_type_class (gnome_canvas_get_type ());

	eil_signals[SELECT_ICON] =
		gtk_signal_new (
			"select_icon",
			GTK_RUN_FIRST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconListClass, select_icon),
			gtk_marshal_NONE__INT_POINTER,
			GTK_TYPE_NONE, 2,
			GTK_TYPE_INT,
			GTK_TYPE_GDK_EVENT);

	eil_signals[UNSELECT_ICON] =
		gtk_signal_new (
			"unselect_icon",
			GTK_RUN_FIRST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconListClass, unselect_icon),
			gtk_marshal_NONE__INT_POINTER,
			GTK_TYPE_NONE, 2,
			GTK_TYPE_INT,
			GTK_TYPE_GDK_EVENT);

	eil_signals[TEXT_CHANGED] =
		gtk_signal_new (
			"text_changed",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconListClass, text_changed),
			xgtk_marshal_BOOL__INT_POINTER,
			GTK_TYPE_BOOL, 2,
			GTK_TYPE_INT,
			GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, eil_signals, LAST_SIGNAL);

	object_class->destroy = eil_destroy;
	object_class->finalize = eil_finalize;
	object_class->set_arg = eil_set_arg;
	object_class->get_arg = eil_get_arg;

	widget_class->size_request = eil_size_request;
	widget_class->size_allocate = eil_size_allocate;
	widget_class->realize = eil_realize;
	widget_class->button_press_event = eil_button_press;
	widget_class->button_release_event = eil_button_release;
	widget_class->motion_notify_event = eil_motion_notify;

	eil_class->select_icon = real_select_icon;
	eil_class->unselect_icon = real_unselect_icon;
}

static void
eil_init (Eil *eil)
{
	eil->_priv = g_new0 (EIconListPrivate, 1);

	eil->_priv->icon_list = g_array_new(FALSE, FALSE, sizeof(gpointer));
	eil->_priv->row_spacing = DEFAULT_ROW_SPACING;
	eil->_priv->col_spacing = DEFAULT_COL_SPACING;
	eil->_priv->text_spacing = DEFAULT_TEXT_SPACING;
	eil->_priv->icon_border = DEFAULT_ICON_BORDER;
	eil->_priv->separators = g_strdup (" ");

	eil->_priv->selection_mode = GTK_SELECTION_SINGLE;
	eil->_priv->dirty = TRUE;

	gnome_canvas_set_scroll_region (GNOME_CANVAS (eil), 0.0, 0.0, 1000000.0, 1000000.0);
	gnome_canvas_scroll_to (GNOME_CANVAS (eil), 0, 0);
}

/**
 * e_icon_list_get_type:
 *
 * Registers the &EIconList class if necessary, and returns the type ID
 * associated to it.
 *
 * Returns: The type ID of the &EIconList class.
 */
guint
e_icon_list_get_type (void)
{
	static guint eil_type = 0;

	if (!eil_type) {
		GtkTypeInfo eil_info = {
			"EIconList",
			sizeof (EIconList),
			sizeof (EIconListClass),
			(GtkClassInitFunc) eil_class_init,
			(GtkObjectInitFunc) eil_init,
			NULL,
			NULL,
			NULL
		};

		eil_type = gtk_type_unique (gnome_canvas_get_type (),
					    &eil_info);
	}

	return eil_type;
}

/**
 * e_icon_list_set_icon_width:
 * @eil: An icon list.
 * @w:   New width for the icon columns.
 *
 * Sets the amount of horizontal space allocated to the icons, i.e. the column
 * width of the icon list.
 */
void
e_icon_list_set_icon_width (EIconList *eil, int w)
{
	EIconListPrivate *priv;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	priv = eil->_priv;

	priv->icon_width  = w;

	if (priv->frozen) {
		priv->dirty = TRUE;
		return;
	}

	eil_layout_all_icons (eil);
	eil_scrollbar_adjust (eil);
}


/**
 * e_icon_list_construct:
 * @eil: An icon list.
 * @icon_width: Width for the icon columns.
 * @flags: A combination of %E_ICON_LIST_IS_EDITABLE and %E_ICON_LIST_STATIC_TEXT.
 *
 * Constructor for the icon list, to be used by derived classes.
 **/
void
e_icon_list_construct (EIconList *eil, guint icon_width, int flags)
{
	EIconListPrivate *priv;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	priv = eil->_priv;

	e_icon_list_set_icon_width (eil, icon_width);
	priv->is_editable = (flags & E_ICON_LIST_IS_EDITABLE) != 0;
	priv->static_text = (flags & E_ICON_LIST_STATIC_TEXT) != 0;
}


/**
 * e_icon_list_new: [constructor]
 * @icon_width: Width for the icon columns.
 * @flags:      A combination of %E_ICON_LIST_IS_EDITABLE and %E_ICON_LIST_STATIC_TEXT.
 *
 * Creates a new icon list widget.  The icon columns are allocated a width of
 * @icon_width pixels.  Icon captions will be word-wrapped to this width as
 * well.
 *
 * If @flags has the %E_ICON_LIST_IS_EDITABLE flag set, then the user will be
 * able to edit the text in the icon captions, and the "text_changed" signal
 * will be emitted when an icon's text is changed.
 *
 * If @flags has the %E_ICON_LIST_STATIC_TEXT flags set, then the text
 * for the icon captions will not be copied inside the icon list; it will only
 * store the pointers to the original text strings specified by the application.
 * This is intended to save memory.  If this flag is not set, then the text
 * strings will be copied and managed internally.
 *
 * Returns: a newly-created icon list widget
 */
GtkWidget *
e_icon_list_new (guint icon_width, int flags)
{
	Eil *eil;

	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	eil = EIL (gtk_type_new (e_icon_list_get_type ()));
	gtk_widget_pop_colormap ();

	e_icon_list_construct (eil, icon_width, flags);

	return GTK_WIDGET (eil);
}


/**
 * e_icon_list_freeze:
 * @eil:  An icon list.
 *
 * Freezes an icon list so that any changes made to it will not be
 * reflected on the screen until it is thawed with e_icon_list_thaw().
 * It is recommended to freeze the icon list before inserting or deleting
 * many icons, for example, so that the layout process will only be executed
 * once, when the icon list is finally thawed.
 *
 * You can call this function multiple times, but it must be balanced with the
 * same number of calls to e_icon_list_thaw() before the changes will take
 * effect.
 */
void
e_icon_list_freeze (EIconList *eil)
{
	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	eil->_priv->frozen++;

	/* We hide the root so that the user will not see any changes while the
	 * icon list is doing stuff.
	 */

	if (eil->_priv->frozen == 1)
		gnome_canvas_item_hide (GNOME_CANVAS (eil)->root);
}

/**
 * e_icon_list_thaw:
 * @eil:  An icon list.
 *
 * Thaws the icon list and performs any pending layout operations.  This
 * is to be used in conjunction with e_icon_list_freeze().
 */
void
e_icon_list_thaw (EIconList *eil)
{
	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (eil->_priv->frozen > 0);

	eil->_priv->frozen--;

	if (eil->_priv->dirty) {
                eil_layout_all_icons (eil);
                eil_scrollbar_adjust (eil);
        }

	if (eil->_priv->frozen == 0)
		gnome_canvas_item_show (GNOME_CANVAS (eil)->root);
}

/**
 * e_icon_list_set_selection_mode
 * @eil:  An icon list.
 * @mode: New selection mode.
 *
 * Sets the selection mode for an icon list.  The %GTK_SELECTION_MULTIPLE and
 * %GTK_SELECTION_EXTENDED modes are considered equivalent.
 */
void
e_icon_list_set_selection_mode (EIconList *eil, GtkSelectionMode mode)
{
	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	eil->_priv->selection_mode = mode;
	eil->_priv->last_selected_idx = -1;
	eil->_priv->last_selected_icon = NULL;
}

/**
 * e_icon_list_set_icon_data_full:
 * @eil:     An icon list.
 * @pos:     Index of an icon.
 * @data:    User data to set on the icon.
 * @destroy: Destroy notification handler for the icon.
 *
 * Associates the @data pointer to the icon at the index specified by @pos.  The
 * @destroy argument points to a function that will be called when the icon is
 * destroyed, or NULL if no function is to be called when this happens.
 */
void
e_icon_list_set_icon_data_full (EIconList *eil,
				    int pos, gpointer data,
				    GtkDestroyNotify destroy)
{
	Icon *icon;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (pos >= 0 && pos < eil->_priv->icons);

	icon = g_array_index (eil->_priv->icon_list, Icon*, pos);
	icon->data = data;
	icon->destroy = destroy;
}

/**
 * e_icon_list_get_icon_data:
 * @eil:  An icon list.
 * @pos:  Index of an icon.
 * @data: User data to set on the icon.
 *
 * Associates the @data pointer to the icon at the index specified by @pos.
 */
void
e_icon_list_set_icon_data (EIconList *eil, int pos, gpointer data)
{
	e_icon_list_set_icon_data_full (eil, pos, data, NULL);
}

/**
 * e_icon_list_get_icon_data:
 * @eil: An icon list.
 * @pos: Index of an icon.
 *
 * Returns the user data pointer associated to the icon at the index specified
 * by @pos.
 */
gpointer
e_icon_list_get_icon_data (EIconList *eil, int pos)
{
	Icon *icon;

	g_return_val_if_fail (eil != NULL, NULL);
	g_return_val_if_fail (IS_EIL (eil), NULL);
	g_return_val_if_fail (pos >= 0 && pos < eil->_priv->icons, NULL);

	icon = g_array_index (eil->_priv->icon_list, Icon*, pos);
	return icon->data;
}

/**
 * e_icon_list_find_icon_from_data:
 * @eil:    An icon list.
 * @data:   Data pointer associated to an icon.
 *
 * Returns the index of the icon whose user data has been set to @data,
 * or -1 if no icon has this data associated to it.
 */
int
e_icon_list_find_icon_from_data (EIconList *eil, gpointer data)
{
	EIconListPrivate *priv;
	int n;
	Icon *icon;

	g_return_val_if_fail (eil != NULL, -1);
	g_return_val_if_fail (IS_EIL (eil), -1);

	priv = eil->_priv;

	for (n = 0; n < priv->icon_list->len; n++) {
		icon = g_array_index(priv->icon_list, Icon*, n);
		if (icon->data == data)
			return n;
	}

	return -1;
}

/* Sets an integer value in the private structure of the icon list, and updates it */
static void
set_value (EIconList *eil, EIconListPrivate *priv, int *dest, int val)
{
	if (val == *dest)
		return;

	*dest = val;

	if (!priv->frozen) {
		eil_layout_all_icons (eil);
		eil_scrollbar_adjust (eil);
	} else
		priv->dirty = TRUE;
}

/**
 * e_icon_list_set_row_spacing:
 * @eil:    An icon list.
 * @pixels: Number of pixels for inter-row spacing.
 *
 * Sets the spacing to be used between rows of icons.
 */
void
e_icon_list_set_row_spacing (EIconList *eil, int pixels)
{
	EIconListPrivate *priv;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	priv = eil->_priv;
	set_value (eil, priv, &priv->row_spacing, pixels);
}

/**
 * e_icon_list_set_col_spacing:
 * @eil:    An icon list.
 * @pixels: Number of pixels for inter-column spacing.
 *
 * Sets the spacing to be used between columns of icons.
 */
void
e_icon_list_set_col_spacing (EIconList *eil, int pixels)
{
	EIconListPrivate *priv;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	priv = eil->_priv;
	set_value (eil, priv, &priv->col_spacing, pixels);
}

/**
 * e_icon_list_set_text_spacing:
 * @eil:    An icon list.
 * @pixels: Number of pixels between an icon's image and its caption.
 *
 * Sets the spacing to be used between an icon's image and its text caption.
 */
void
e_icon_list_set_text_spacing (EIconList *eil, int pixels)
{
	EIconListPrivate *priv;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	priv = eil->_priv;
	set_value (eil, priv, &priv->text_spacing, pixels);
}

/**
 * e_icon_list_set_icon_border:
 * @eil:    An icon list.
 * @pixels: Number of border pixels to be used around an icon's image.
 *
 * Sets the width of the border to be displayed around an icon's image.  This is
 * currently not implemented.
 */
void
e_icon_list_set_icon_border (EIconList *eil, int pixels)
{
	EIconListPrivate *priv;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));

	priv = eil->_priv;
	set_value (eil, priv, &priv->icon_border, pixels);
}

/**
 * e_icon_list_set_separators:
 * @eil: An icon list.
 * @sep: String with characters to be used as word separators.
 *
 * Sets the characters that can be used as word separators when doing
 * word-wrapping in the icon text captions.
 */
void
e_icon_list_set_separators (EIconList *eil, const char *sep)
{
	EIconListPrivate *priv;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (sep != NULL);

	priv = eil->_priv;

	if (priv->separators)
		g_free (priv->separators);

	priv->separators = g_strdup (sep);

	if (priv->frozen) {
		priv->dirty = TRUE;
		return;
	}

	eil_layout_all_icons (eil);
	eil_scrollbar_adjust (eil);
}

/**
 * e_icon_list_moveto:
 * @eil:    An icon list.
 * @pos:    Index of an icon.
 * @yalign: Vertical alignment of the icon.
 *
 * Makes the icon whose index is @pos be visible on the screen.  The icon list
 * gets scrolled so that the icon is visible.  An alignment of 0.0 represents
 * the top of the visible part of the icon list, and 1.0 represents the bottom.
 * An icon can be centered on the icon list.
 */
void
e_icon_list_moveto (EIconList *eil, int pos, double yalign)
{
	EIconListPrivate *priv;
	GtkAdjustment *adj;
	IconLine *il;
	GList *l;
	int i, y, uh, line;

	g_return_if_fail (eil != NULL);
	g_return_if_fail (IS_EIL (eil));
	g_return_if_fail (pos >= 0 && pos < eil->_priv->icons);
	g_return_if_fail (yalign >= 0.0 && yalign <= 1.0);

	priv = eil->_priv;

	g_return_if_fail (priv->lines != NULL);

	line = pos / eil_get_items_per_line (eil);

	y = 0;
	for (i = 0, l = priv->lines; l && i < line; l = l->next, i++) {
		il = l->data;
		y += icon_line_height (eil, il);
	}

	il = l->data;

	uh = GTK_WIDGET (eil)->allocation.height - icon_line_height (eil,il);
	adj = gtk_layout_get_vadjustment (GTK_LAYOUT (eil));
	gtk_adjustment_set_value (adj, y - uh * yalign);
}

/**
 * e_icon_list_is_visible:
 * @eil: An icon list.
 * @pos: Index of an icon.
 *
 * Returns whether the icon at the index specified by @pos is visible.  This
 * will be %GTK_VISIBILITY_NONE if the icon is not visible at all,
 * %GTK_VISIBILITY_PARTIAL if the icon is at least partially shown, and
 * %GTK_VISIBILITY_FULL if the icon is fully visible.
 */
GtkVisibility
e_icon_list_icon_is_visible (EIconList *eil, int pos)
{
	EIconListPrivate *priv;
	GtkAdjustment *adj;
	IconLine *il;
	GList *l;
	int line, y1, y2, i;

	g_return_val_if_fail (eil != NULL, GTK_VISIBILITY_NONE);
	g_return_val_if_fail (IS_EIL (eil), GTK_VISIBILITY_NONE);
	g_return_val_if_fail (pos >= 0 && pos < eil->_priv->icons,
			      GTK_VISIBILITY_NONE);

	priv = eil->_priv;

	if (priv->lines == NULL)
		return GTK_VISIBILITY_NONE;

	line = pos / eil_get_items_per_line (eil);
	y1 = 0;
	for (i = 0, l = priv->lines; l && i < line; l = l->next, i++) {
		il = l->data;
		y1 += icon_line_height (eil, il);
	}

	y2 = y1 + icon_line_height (eil, (IconLine *) l->data);

	adj = gtk_layout_get_vadjustment (GTK_LAYOUT (eil));

	if (y2 < adj->value)
		return GTK_VISIBILITY_NONE;

	if (y1 > adj->value + GTK_WIDGET (eil)->allocation.height)
		return GTK_VISIBILITY_NONE;

	if (y2 <= adj->value + GTK_WIDGET (eil)->allocation.height &&
	    y1 >= adj->value)
		return GTK_VISIBILITY_FULL;

	return GTK_VISIBILITY_PARTIAL;
}

/**
 * e_icon_list_get_icon_at:
 * @eil: An icon list.
 * @x:   X position in the icon list window.
 * @y:   Y position in the icon list window.
 *
 * Returns the index of the icon that is under the specified coordinates, which
 * are relative to the icon list's window.  If there is no icon in that
 * position, -1 is returned.
 */
int
e_icon_list_get_icon_at (EIconList *eil, int x, int y)
{
	EIconListPrivate *priv;
	double wx, wy;
	double dx, dy;
	int cx, cy;
	int n;
	GnomeCanvasItem *item;
	double dist;

	g_return_val_if_fail (eil != NULL, -1);
	g_return_val_if_fail (IS_EIL (eil), -1);

	priv = eil->_priv;

	dx = x;
	dy = y;

	gnome_canvas_window_to_world (GNOME_CANVAS (eil), dx, dy, &wx, &wy);
	gnome_canvas_w2c (GNOME_CANVAS (eil), wx, wy, &cx, &cy);

	for (n = 0; n < priv->icon_list->len; n++) {
		Icon *icon = g_array_index(priv->icon_list, Icon*, n);
		GnomeCanvasItem *image = GNOME_CANVAS_ITEM (icon->image);
		GnomeCanvasItem *text = GNOME_CANVAS_ITEM (icon->text);

		if (wx >= image->x1 && wx <= image->x2 && wy >= image->y1 && wy <= image->y2) {
			dist = (* GNOME_CANVAS_ITEM_CLASS (GTK_OBJECT (image)->klass)->point) (
				image,
				wx, wy,
				cx, cy,
				&item);

			if ((int) (dist * GNOME_CANVAS (eil)->pixels_per_unit + 0.5)
			    <= GNOME_CANVAS (eil)->close_enough)
				return n;
		}

		if (wx >= text->x1 && wx <= text->x2 && wy >= text->y1 && wy <= text->y2) {
			dist = (* GNOME_CANVAS_ITEM_CLASS (GTK_OBJECT (text)->klass)->point) (
				text,
				wx, wy,
				cx, cy,
				&item);

			if ((int) (dist * GNOME_CANVAS (eil)->pixels_per_unit + 0.5)
			    <= GNOME_CANVAS (eil)->close_enough)
				return n;
		}
	}

	return -1;
}


/**
 * e_icon_list_get_num_icons:
 * @eil: An icon list.
 *
 * Returns the number of icons in the icon list.
 */
guint
e_icon_list_get_num_icons (EIconList *eil)
{
	g_return_val_if_fail (E_IS_ICON_LIST (eil), 0);

	return eil->_priv->icons;
}


/**
 * e_icon_list_get_selection:
 * @eil: An icon list.
 *
 * Returns a list of integers with the indices of the currently selected icons.
 */
GList *
e_icon_list_get_selection (EIconList *eil)
{
	g_return_val_if_fail (E_IS_ICON_LIST (eil), NULL);

	return eil->_priv->selection;
}


/**
 * e_icon_list_get_selection:
 * @eil: An icon list.
 * @idx: Index of an @icon.
 *
 * Returns the filename of the icon with index @idx.
 */
gchar *
e_icon_list_get_icon_filename (EIconList *eil, int idx)
{
	Icon *icon;

	g_return_val_if_fail (eil != NULL, NULL);
	g_return_val_if_fail (IS_EIL (eil), NULL);
	g_return_val_if_fail (idx >= 0 && idx < eil->_priv->icons, NULL);

	icon = g_array_index (eil->_priv->icon_list, Icon*, idx);
	return icon->icon_filename;
}


/**
 * e_icon_list_find_icon_from_filename:
 * @eil:       An icon list.
 * @filename:  Filename of an icon.
 *
 * Returns the index of the icon whose filename is @filename or -1 if
 * there is no icon with this filename.
 */
int
e_icon_list_find_icon_from_filename (EIconList *eil,
				     const gchar *filename)
{
	EIconListPrivate *priv;
	int n;
	Icon *icon;

	g_return_val_if_fail (eil != NULL, -1);
	g_return_val_if_fail (IS_EIL (eil), -1);
	g_return_val_if_fail (filename != NULL, -1);

	priv = eil->_priv;

	for (n = 0; n < priv->icon_list->len; n++) {
		icon = g_array_index(priv->icon_list, Icon*, n);
		if (!strcmp (icon->icon_filename, filename))
			return n;
	}

	return -1;
}

