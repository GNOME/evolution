/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@gtk.org>
 *
 * Copyright 1999, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * Based on gnome-icon-text-item:  an editable text block with word wrapping
 * for the GNOME canvas.
 *
 * Copyright (C) 1998, 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena <federico@gimp.org>
 */

/*
 * EIconBarTextItem - An editable canvas text item for the EIconBar.
 */

#include <math.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include "e-icon-bar-text-item.h"


/* Margins used to display the information */
#define MARGIN_X 2
#define MARGIN_Y 2

/* Default fontset to be used if the user specified fontset is not found */
#define DEFAULT_FONT_NAME "-adobe-helvetica-medium-r-normal--*-100-*-*-*-*-*-*,"	\
			  "-*-*-medium-r-normal--10-*-*-*-*-*-*-*,*"

/* Separators for text layout */
#define DEFAULT_SEPARATORS " \t-.[]#"

/* This is the string to draw when the text is clipped, e.g. '...'. */
static gchar *e_icon_bar_text_item_ellipsis;

/* Aliases to minimize screen use in my laptop */
#define ITI(x)       E_ICON_BAR_TEXT_ITEM (x)
#define ITI_CLASS(x) E_ICON_BAR_TEXT_ITEM_CLASS (x)
#define IS_ITI(x)    E_IS_ICON_BAR_TEXT_ITEM (x)


typedef EIconBarTextItem Iti;

/* Private part of the EIconBarTextItem structure */
typedef struct {
	/* Font */
	GdkFont *font;

	/* Hack: create an offscreen window and place an entry inside it */
	GtkEntry *entry;
	GtkWidget *entry_top;

	/* Whether the user pressed the mouse while the item was unselected */
	guint unselected_click : 1;

	/* Whether we need to update the position */
	guint need_pos_update : 1;

	/* Whether we need to update the font */
	guint need_font_update : 1;

	/* Whether we need to update the text */
	guint need_text_update : 1;

	/* Whether we need to update because the editing/selected state changed */
	guint need_state_update : 1;
} ItiPrivate;

typedef struct _EIconBarTextItemInfoRow   EIconBarTextItemInfoRow;

struct _EIconBarTextItemInfoRow {
	gchar *text;
	gint width;
	GdkWChar *text_wc;	/* text in wide characters */
	gint text_length;	/* number of characters */
};

struct _EIconBarTextItemInfo {
	GList *rows;
	GdkFont *font;
	gint width;
	gint height;
	gint baseline_skip;
};

static GnomeCanvasItemClass *parent_class;

enum {
	ARG_0,
	ARG_XALIGN,
	ARG_JUSTIFY,
	ARG_MAX_LINES,
	ARG_SHOW_ELLIPSIS
};

enum {
	TEXT_CHANGED,
	HEIGHT_CHANGED,
	WIDTH_CHANGED,
	EDITING_STARTED,
	EDITING_STOPPED,
	SELECTION_STARTED,
	SELECTION_STOPPED,
	LAST_SIGNAL
};

static guint iti_signals [LAST_SIGNAL] = { 0 };

static GdkFont *default_font;

static void e_icon_bar_text_item_free_info (EIconBarTextItemInfo *ti);
static EIconBarTextItemInfo *e_icon_bar_text_item_layout_text (EIconBarTextItem *iti, GdkFont *font, const gchar *text, const gchar *separators, gint max_width, gboolean confine);
static void e_icon_bar_text_item_paint_text (EIconBarTextItem	  *iti,
					     EIconBarTextItemInfo *ti,
					     GdkDrawable	  *drawable,
					     GdkGC		  *gc,
					     gint		   x,
					     gint		   y,
					     GtkJustification	   just);


/* Stops the editing state of an icon text item */
static void
iti_stop_editing (Iti *iti)
{
	ItiPrivate *priv;

	priv = iti->priv;

	iti->editing = FALSE;

	gtk_widget_destroy (priv->entry_top);
	priv->entry = NULL;
	priv->entry_top = NULL;

	priv->need_state_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));

	gtk_signal_emit (GTK_OBJECT (iti), iti_signals[EDITING_STOPPED]);
}

/* Lays out the text in an icon item */
static void
layout_text (Iti *iti)
{
	ItiPrivate *priv;
	char *text;
	int old_width, old_height;
	int width, height;

	priv = iti->priv;

	/* Save old size */

	if (iti->ti) {
		old_width = iti->ti->width + 2 * MARGIN_X;
		old_height = iti->ti->height + 2 * MARGIN_Y;

		e_icon_bar_text_item_free_info (iti->ti);
	} else {
		old_width = 2 * MARGIN_X;
		old_height = 2 * MARGIN_Y;
	}

	/* Change the text layout */

	if (iti->editing)
		text = gtk_entry_get_text (priv->entry);
	else
		text = iti->text;

	iti->ti = e_icon_bar_text_item_layout_text (iti, priv->font,
						    text,
						    DEFAULT_SEPARATORS,
						    iti->width - 2 * MARGIN_X,
						    TRUE);

	/* Check the sizes and see if we need to emit any signals */

	width = iti->ti->width + 2 * MARGIN_X;
	height = iti->ti->height + 2 * MARGIN_Y;

	if (width != old_width)
		gtk_signal_emit (GTK_OBJECT (iti), iti_signals[WIDTH_CHANGED]);

	if (height != old_height)
		gtk_signal_emit (GTK_OBJECT (iti), iti_signals[HEIGHT_CHANGED]);
}

/* Accepts the text in the off-screen entry of an icon text item */
static void
iti_edition_accept (Iti *iti)
{
	ItiPrivate *priv;
	gboolean accept;

	priv = iti->priv;
	accept = TRUE;

	gtk_signal_emit (GTK_OBJECT (iti), iti_signals [TEXT_CHANGED], &accept);

	if (iti->editing){
		if (accept) {
			if (iti->is_text_allocated)
				g_free (iti->text);

			iti->text = g_strdup (gtk_entry_get_text (priv->entry));
			iti->is_text_allocated = 1;
		}

		iti_stop_editing (iti);
	}

	priv->need_text_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/* Callback used when the off-screen entry of an icon text item is activated.
 * When this happens, we have to accept edition.
 */
static void
iti_entry_activate (GtkWidget *entry, Iti *iti)
{
	iti_edition_accept (iti);
}

/* Starts the editing state of an icon text item */
static void
iti_start_editing (Iti *iti)
{
	ItiPrivate *priv;

	priv = iti->priv;

	if (iti->editing)
		return;

	/* Trick: The actual edition of the entry takes place in a GtkEntry
	 * which is placed offscreen.  That way we get all of the advantages
	 * from GtkEntry without duplicating code.  Yes, this is a hack.
	 */
	priv->entry = (GtkEntry *) gtk_entry_new ();
	gtk_entry_set_text (priv->entry, iti->text);
	gtk_signal_connect (GTK_OBJECT (priv->entry), "activate",
			    GTK_SIGNAL_FUNC (iti_entry_activate), iti);

	priv->entry_top = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_container_add (GTK_CONTAINER (priv->entry_top), GTK_WIDGET (priv->entry));
	gtk_widget_set_uposition (priv->entry_top, 20000, 20000);
	gtk_widget_show_all (priv->entry_top);

	gtk_editable_select_region (GTK_EDITABLE (priv->entry), 0, -1);

	iti->editing = TRUE;

	priv->need_text_update = TRUE;
	priv->need_state_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));

	gtk_signal_emit (GTK_OBJECT (iti), iti_signals[EDITING_STARTED]);
}

/* Destroy method handler for the icon text item */
static void
iti_destroy (GtkObject *object)
{
	Iti *iti;
	ItiPrivate *priv;
	GnomeCanvasItem *item;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ITI (object));

	iti = ITI (object);
	priv = iti->priv;
	item = GNOME_CANVAS_ITEM (object);

	/* FIXME: stop selection and editing */

	/* Queue redraw of bounding box */

	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	/* Free everything */

	if (iti->fontname)
		g_free (iti->fontname);

	if (iti->text && iti->is_text_allocated)
		g_free (iti->text);

	if (iti->ti)
		e_icon_bar_text_item_free_info (iti->ti);

	if (priv->font)
		gdk_font_unref (priv->font);

	if (priv->entry_top)
		gtk_widget_destroy (priv->entry_top);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* set_arg handler for the icon text item */
static void
iti_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	Iti *iti;
	GnomeCanvasItem *item;
	ItiPrivate *priv;
	gfloat xalign;
	gint max_lines;
	gboolean show_ellipsis;
	GtkJustification justification;

	iti = ITI (object);
	item = GNOME_CANVAS_ITEM (object);
	priv = iti->priv;

	switch (arg_id) {
	case ARG_XALIGN:
		xalign = GTK_VALUE_FLOAT (*arg);
		if (iti->xalign != xalign) {
			iti->xalign = xalign;
			priv->need_pos_update = TRUE;
			gnome_canvas_item_request_update (item);
		}
		break;
	case ARG_JUSTIFY:
		justification = GTK_VALUE_ENUM (*arg);
		if (iti->justification != justification) {
			iti->justification = justification;
			priv->need_text_update = TRUE;
			gnome_canvas_item_request_update (item);
		}
		break;
	case ARG_MAX_LINES:
		max_lines = GTK_VALUE_INT (*arg);
		if (iti->max_lines != max_lines) {
			iti->max_lines = max_lines;
			priv->need_text_update = TRUE;
			gnome_canvas_item_request_update (item);
		}
		break;
	case ARG_SHOW_ELLIPSIS:
		show_ellipsis = GTK_VALUE_BOOL (*arg);
		if (iti->show_ellipsis != show_ellipsis) {
			iti->show_ellipsis = show_ellipsis;
			priv->need_text_update = TRUE;
			gnome_canvas_item_request_update (item);
		}
		break;
	default:
		break;
	}
}

static void
iti_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	Iti *iti;
	ItiPrivate *priv;

	iti = ITI (object);
	priv = iti->priv;

	switch (arg_id)	{
	case ARG_XALIGN:
		GTK_VALUE_FLOAT (*arg) = iti->xalign;
		break;
	case ARG_JUSTIFY:
		GTK_VALUE_ENUM (*arg) = iti->justification;
		break;
	case ARG_MAX_LINES:
		GTK_VALUE_INT (*arg) = iti->max_lines;
		break;
	case ARG_SHOW_ELLIPSIS:
		GTK_VALUE_BOOL (*arg) = iti->show_ellipsis;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

/* Loads the default font for icon text items if necessary */
static GdkFont *
get_default_font (void)
{
	if (!default_font) {
		/* FIXME: this is never unref-ed */
		default_font = gdk_fontset_load (DEFAULT_FONT_NAME);
		g_assert (default_font != NULL);
	}

	return gdk_font_ref (default_font);
}

/* Recomputes the bounding box of an icon text item */
static void
recompute_bounding_box (Iti *iti)
{
	GnomeCanvasItem *item;
	double affine[6];
	ArtPoint p, q;
	int x1, y1, x2, y2;
	int width, height;

	item = GNOME_CANVAS_ITEM (iti);

	/* Compute width, height, position */

	width = iti->ti->width + 2 * MARGIN_X;
	height = iti->ti->height + 2 * MARGIN_Y;

	x1 = iti->x + (iti->width - width) * iti->xalign;
	y1 = iti->y;
	x2 = x1 + width;
	y2 = y1 + height;

	/* Translate to world coordinates */

	gnome_canvas_item_i2w_affine (item, affine);

	p.x = x1;
	p.y = y1;
	art_affine_point (&q, &p, affine);
	item->x1 = q.x;
	item->y1 = q.y;

	p.x = x2;
	p.y = y2;
	art_affine_point (&q, &p, affine);
	item->x2 = q.x;
	item->y2 = q.y;
}

/* Update method for the icon text item */
static void
iti_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	Iti *iti;
	ItiPrivate *priv;

	iti = ITI (item);
	priv = iti->priv;

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	/* If necessary, queue a redraw of the old bounding box */

	if ((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)
	    || priv->need_pos_update
	    || priv->need_font_update
	    || priv->need_text_update)
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	 if (priv->need_text_update)
		 layout_text (iti);

	/* Compute new bounds */

	if (priv->need_pos_update
	    || priv->need_font_update
	    || priv->need_text_update)
		recompute_bounding_box (iti);

	/* Queue redraw */

	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	priv->need_pos_update = FALSE;
	priv->need_font_update = FALSE;
	priv->need_text_update = FALSE;
	priv->need_state_update = FALSE;
}

/* Draw the icon text item's text when it is being edited */
static void
iti_paint_text (Iti *iti, GdkDrawable *drawable, int x, int y)
{
	ItiPrivate *priv;
        EIconBarTextItemInfoRow *row;
	EIconBarTextItemInfo *ti;
	GtkStyle *style;
	GdkGC *fg_gc, *bg_gc;
	GdkGC *gc, *bgc, *sgc, *bsgc;
        GList *item;
        int xpos, len;

	priv = iti->priv;
	style = GTK_WIDGET (GNOME_CANVAS_ITEM (iti)->canvas)->style;

	ti = iti->ti;
	len = 0;
        y += ti->font->ascent;

	/*
	 * Pointers to all of the GCs we use
	 */
	gc = style->black_gc;
	bgc = style->white_gc;
	sgc = style->fg_gc [GTK_STATE_SELECTED];
	bsgc = style->bg_gc [GTK_STATE_SELECTED];

        for (item = ti->rows; item; item = item->next, len += (row ? row->text_length : 0)) {
		GdkWChar *text_wc;
		int text_length;
		int cursor, offset, i;
		int sel_start, sel_end;

		row = item->data;

                if (!row) {
			y += ti->baseline_skip;
			continue;
		}

		text_wc = row->text_wc;
		text_length = row->text_length;

		switch (iti->justification) {
		case GTK_JUSTIFY_LEFT:
			xpos = 0;
			break;

		case GTK_JUSTIFY_RIGHT:
			xpos = ti->width - row->width;
			break;

		case GTK_JUSTIFY_CENTER:
			xpos = (ti->width - row->width) / 2;
			break;

		default:
			/* Anyone care to implement GTK_JUSTIFY_FILL? */
			g_warning ("Justification type %d not supported.  Using left-justification.",
				   (int) iti->justification);
			xpos = 0;
		}

		sel_start = GTK_EDITABLE (priv->entry)->selection_start_pos - len;
		sel_end = GTK_EDITABLE (priv->entry)->selection_end_pos - len;
		offset = 0;
		cursor = GTK_EDITABLE (priv->entry)->current_pos - len;

		for (i = 0; *text_wc; text_wc++, i++) {
			int size, px;

			size = gdk_text_width_wc (ti->font, text_wc, 1);

			if (i >= sel_start && i < sel_end) {
				fg_gc = sgc;
				bg_gc = bsgc;
			} else {
				fg_gc = gc;
				bg_gc = bgc;
			}

			px = x + xpos + offset;
			gdk_draw_rectangle (drawable,
					    bg_gc,
					    TRUE,
					    px,
					    y - ti->font->ascent,
					    size, ti->baseline_skip);

			gdk_draw_text_wc (drawable,
					  ti->font,
					  fg_gc,
					  px, y,
					  text_wc, 1);

			if (cursor == i)
				gdk_draw_line (drawable,
					       gc,
					       px - 1,
					       y - ti->font->ascent,
					       px - 1,
					       y + ti->font->descent - 1);

			offset += size;
		}

		if (cursor == i) {
			int px = x + xpos + offset;

			gdk_draw_line (drawable,
				       gc,
				       px - 1,
				       y - ti->font->ascent,
				       px - 1,
				       y + ti->font->descent - 1);
		}

		y += ti->baseline_skip;
        }
}

/* Draw method handler for the icon text item */
static void
iti_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height)
{
	Iti *iti;
	GtkStyle *style;
	int w, h;
	int xofs, yofs;

	iti = ITI (item);

	if (iti->ti) {
		w = iti->ti->width + 2 * MARGIN_X;
		h = iti->ti->height + 2 * MARGIN_Y;
	} else {
		w = 2 * MARGIN_X;
		h = 2 * MARGIN_Y;
	}

	xofs = item->x1 - x;
	yofs = item->y1 - y;

	style = GTK_WIDGET (item->canvas)->style;

	if (iti->selected && !iti->editing)
		gdk_draw_rectangle (drawable,
				    style->bg_gc[GTK_STATE_SELECTED],
				    TRUE,
				    xofs, yofs,
				    w, h);

	if (iti->editing) {
		gdk_draw_rectangle (drawable,
				    style->white_gc,
				    TRUE,
				    xofs + 1, yofs + 1,
				    w - 2, h - 2);
		gdk_draw_rectangle (drawable,
				    style->black_gc,
				    FALSE,
				    xofs, yofs,
				    w - 1, h - 1);

		iti_paint_text (iti, drawable, xofs + MARGIN_X, yofs + MARGIN_Y);
	} else
		e_icon_bar_text_item_paint_text (iti, iti->ti,
						 drawable,
						 style->fg_gc[(iti->selected
							       ? GTK_STATE_SELECTED
							       : GTK_STATE_NORMAL)],
						 xofs + MARGIN_X,
						 yofs + MARGIN_Y,
						 iti->justification);
}

/* Point method handler for the icon text item */
static double
iti_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item)
{
	double dx, dy;

	*actual_item = item;

	if (cx < item->x1)
		dx = item->x1 - cx;
	else if (cx > item->x2)
		dx = cx - item->x2;
	else
		dx = 0.0;

	if (cy < item->y1)
		dy = item->y1 - cy;
	else if (cy > item->y2)
		dy = cy - item->y2;
	else
		dy = 0.0;

	return sqrt (dx * dx + dy * dy);
}

/* Given X, Y, a mouse position, return a valid index inside the edited text */
static int
iti_idx_from_x_y (Iti *iti, int x, int y)
{
	ItiPrivate *priv;
        EIconBarTextItemInfoRow *row;
	int lines;
	int line, col, i, idx;
	GList *l;

	priv = iti->priv;

	if (iti->ti->rows == NULL)
		return 0;

	lines = g_list_length (iti->ti->rows);
	line = y / iti->ti->baseline_skip;

	if (line < 0)
		line = 0;
	else if (lines < line + 1)
		line = lines - 1;

	/* Compute the base index for this line */
	for (l = iti->ti->rows, idx = i = 0; i < line; l = l->next, i++) {
		row = l->data;
		idx += row->text_length;
	}

	row = g_list_nth (iti->ti->rows, line)->data;
	col = 0;
	if (row != NULL) {
		int first_char;
		int last_char;

		first_char = (iti->ti->width - row->width) / 2;
		last_char = first_char + row->width;

		if (x < first_char) {
			/* nothing */
		} else if (x > last_char) {
			col = row->text_length;
		} else {
			GdkWChar *s = row->text_wc;
			int pos = first_char;

			while (pos < last_char) {
				pos += gdk_text_width_wc (iti->ti->font, s, 1);
				if (pos > x)
					break;
				col++;
				s++;
			}
		}
	}

	idx += col;

	g_assert (idx <= priv->entry->text_size);

	return idx;
}

/* Starts the selection state in the icon text item */
static void
iti_start_selecting (Iti *iti, int idx, guint32 event_time)
{
	ItiPrivate *priv;
	GtkEditable *e;
	GdkCursor *ibeam;

	priv = iti->priv;
	e = GTK_EDITABLE (priv->entry);

	gtk_editable_select_region (e, idx, idx);
	gtk_editable_set_position (e, idx);
	ibeam = gdk_cursor_new (GDK_XTERM);
	gnome_canvas_item_grab (GNOME_CANVAS_ITEM (iti),
				GDK_BUTTON_RELEASE_MASK |
				GDK_POINTER_MOTION_MASK,
				ibeam, event_time);
	gdk_cursor_destroy (ibeam);

	gtk_editable_select_region (e, idx, idx);
	e->current_pos = e->selection_start_pos;
	e->has_selection = TRUE;
	iti->selecting = TRUE;

	priv->need_state_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));

	gtk_signal_emit (GTK_OBJECT (iti), iti_signals[SELECTION_STARTED]);
}

/* Stops the selection state in the icon text item */
static void
iti_stop_selecting (Iti *iti, guint32 event_time)
{
	ItiPrivate *priv;
	GnomeCanvasItem *item;
	GtkEditable *e;

	priv = iti->priv;
	item = GNOME_CANVAS_ITEM (iti);
	e = GTK_EDITABLE (priv->entry);

	gnome_canvas_item_ungrab (item, event_time);
	e->has_selection = FALSE;
	iti->selecting = FALSE;

	priv->need_state_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
	gtk_signal_emit (GTK_OBJECT (iti), iti_signals[SELECTION_STOPPED]);
}

/* Handles selection range changes on the icon text item */
static void
iti_selection_motion (Iti *iti, int idx)
{
	ItiPrivate *priv;
	GtkEditable *e;

	priv = iti->priv;
	e = GTK_EDITABLE (priv->entry);

	if (idx < e->current_pos) {
		e->selection_start_pos = idx;
		e->selection_end_pos   = e->current_pos;
	} else {
		e->selection_start_pos = e->current_pos;
		e->selection_end_pos  = idx;
	}

	priv->need_state_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/* Event handler for icon text items */
static gint
iti_event (GnomeCanvasItem *item, GdkEvent *event)
{
	Iti *iti;
	ItiPrivate *priv;
	int idx;
	double x, y;

	iti = ITI (item);
	priv = iti->priv;

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (!iti->editing)
			break;

		if (event->key.keyval == GDK_Escape)
			iti_stop_editing (iti);
		else
			gtk_widget_event (GTK_WIDGET (priv->entry), event);

		priv->need_text_update = TRUE;
		gnome_canvas_item_request_update (item);
		return TRUE;

	case GDK_BUTTON_PRESS:
		if (!iti->editing)
			break;

		if (iti->editing && event->button.button == 1) {
			x = event->button.x - (item->x1 + MARGIN_X);
			y = event->button.y - (item->y1 + MARGIN_Y);
			idx = iti_idx_from_x_y (iti, x, y);

			iti_start_selecting (iti, idx, event->button.time);
		}

		return TRUE;

	case GDK_MOTION_NOTIFY:
		if (!iti->selecting)
			break;

		x = event->motion.x - (item->x1 + MARGIN_X);
		y = event->motion.y - (item->y1 + MARGIN_Y);
		idx = iti_idx_from_x_y (iti, x, y);
		iti_selection_motion (iti, idx);
		return TRUE;

	case GDK_BUTTON_RELEASE:
		if (iti->selecting && event->button.button == 1)
			iti_stop_selecting (iti, event->button.time);
		else
			break;

		return TRUE;

	default:
		break;
	}

	return FALSE;
}

/* Bounds method handler for the icon text item */
static void
iti_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	Iti *iti;
	ItiPrivate *priv;
	int width, height;

	iti = ITI (item);
	priv = iti->priv;

	if (priv->need_text_update) {
		layout_text (iti);
		priv->need_text_update = FALSE;
	}

	if (iti->ti) {
		width = iti->ti->width + 2 * MARGIN_X;
		height = iti->ti->height + 2 * MARGIN_Y;
	} else {
		width = 2 * MARGIN_X;
		height = 2 * MARGIN_Y;
	}

	*x1 = iti->x + (iti->width - width) * iti->xalign;
	*y1 = iti->y;
	*x2 = *x1 + width;
	*y2 = *y1 + height;
}

/* Class initialization function for the icon text item */
static void
iti_class_init (EIconBarTextItemClass *text_item_class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) text_item_class;
	item_class   = (GnomeCanvasItemClass *) text_item_class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("EIconBarTextItem::xalign", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_XALIGN);
	gtk_object_add_arg_type ("EIconBarTextItem::justify", GTK_TYPE_JUSTIFICATION, GTK_ARG_READWRITE, ARG_JUSTIFY);
	gtk_object_add_arg_type ("EIconBarTextItem::max_lines", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_MAX_LINES);
	gtk_object_add_arg_type ("EIconBarTextItem::show_ellipsis", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_SHOW_ELLIPSIS);

	iti_signals [TEXT_CHANGED] =
		gtk_signal_new (
			"text_changed",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconBarTextItemClass, text_changed),
			gtk_marshal_BOOL__NONE,
			GTK_TYPE_BOOL, 0);

	iti_signals [HEIGHT_CHANGED] =
		gtk_signal_new (
			"height_changed",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconBarTextItemClass, height_changed),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals [WIDTH_CHANGED] =
		gtk_signal_new (
			"width_changed",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconBarTextItemClass, width_changed),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals[EDITING_STARTED] =
		gtk_signal_new (
			"editing_started",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconBarTextItemClass, editing_started),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals[EDITING_STOPPED] =
		gtk_signal_new (
			"editing_stopped",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconBarTextItemClass, editing_stopped),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals[SELECTION_STARTED] =
		gtk_signal_new (
			"selection_started",
			GTK_RUN_FIRST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconBarTextItemClass, selection_started),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals[SELECTION_STOPPED] =
		gtk_signal_new (
			"selection_stopped",
			GTK_RUN_FIRST,
			object_class->type,
			GTK_SIGNAL_OFFSET (EIconBarTextItemClass, selection_stopped),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, iti_signals, LAST_SIGNAL);

	object_class->destroy = iti_destroy;
	object_class->get_arg = iti_get_arg;
	object_class->set_arg = iti_set_arg;

	item_class->update = iti_update;
	item_class->draw = iti_draw;
	item_class->point = iti_point;
	item_class->bounds = iti_bounds;
	item_class->event = iti_event;

	e_icon_bar_text_item_ellipsis = _("...");
}

/* Object initialization function for the icon text item */
static void
iti_init (EIconBarTextItem *iti)
{
	ItiPrivate *priv;

	priv = g_new0 (ItiPrivate, 1);
	iti->priv = priv;

	iti->xalign = 0.5;
	iti->justification = GTK_JUSTIFY_CENTER;
	iti->max_lines = -1;
	iti->show_ellipsis = TRUE;
}

/**
 * e_icon_bar_text_item_configure:
 * @iti: An #EIconBarTextItem.
 * @x: X position in which to place the item.
 * @y: Y position in which to place the item.
 * @width: Maximum width allowed for this item, to be used for word wrapping.
 * @fontname: Name of the fontset that should be used to display the text.
 * @text: Text that is going to be displayed.
 * @is_static: Whether @text points to a static string or not.
 *
 * This routine is used to configure an #EIconBarTextItem.
 *
 * @x and @y specify the coordinates where the item is placed in the canvas.
 * The @x coordinate should be the leftmost position that the item can
 * assume at any one time, that is, the left margin of the column in which the
 * icon is to be placed.  The @y coordinate specifies the top of the item.
 *
 * @width is the maximum width allowed for this icon text item. The coordinates
 * define the upper-left corner of an item with maximum width; this may
 * actually be outside the bounding box of the item if the text is narrower
 * than the maximum width.
 *
 * If @is_static is true, it means that there is no need for the item to
 * allocate memory for the string (it is a guarantee that the text is allocated
 * by the caller and it will not be deallocated during the lifetime of this
 * item).  This is an optimization to reduce memory usage for large icon sets.
 */
void
e_icon_bar_text_item_configure (EIconBarTextItem *iti, int x, int y,
				int width, const char *fontname,
				const char *text,
				gboolean is_static)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));
	g_return_if_fail (width > 2 * MARGIN_X);
	g_return_if_fail (text != NULL);

	priv = iti->priv;

	iti->x = x;
	iti->y = y;
	iti->width = width;

	if (iti->text && iti->is_text_allocated)
		g_free (iti->text);

	iti->is_text_allocated = !is_static;

	/* This cast is to shut up the compiler */
	if (is_static)
		iti->text = (char *) text;
	else
		iti->text = g_strdup (text);

	if (iti->fontname)
		g_free (iti->fontname);

	iti->fontname = g_strdup (fontname ? fontname : DEFAULT_FONT_NAME);

	if (priv->font)
		gdk_font_unref (priv->font);

	priv->font = NULL;
	if (fontname)
		priv->font = gdk_fontset_load (iti->fontname);
	if (!priv->font)
		priv->font = get_default_font ();

	/* Request update */

	priv->need_pos_update = TRUE;
	priv->need_font_update = TRUE;
	priv->need_text_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * e_icon_bar_text_item_set_width:
 * @iti: An #EIconBarTextItem.
 * @width: Maximum width allowed for this item, to be used for word wrapping.
 *
 * This routine is used to set the maximum width of an #EIconBarTextItem.
 */
void
e_icon_bar_text_item_set_width (EIconBarTextItem *iti, int width)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));
	g_return_if_fail (width > 2 * MARGIN_X);

	priv = iti->priv;

	if (iti->width == width)
		return;

	iti->width = width;

	/* Request update */
	priv->need_text_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * e_icon_bar_text_item_setxy:
 * @iti: An #EIconBarTextItem.
 * @x: X position.
 * @y: Y position.
 *
 * Sets the coordinates at which the #EIconBarTextItem should be placed.
 *
 * See also: e_icon_bar_text_item_configure().
 */
void
e_icon_bar_text_item_setxy (EIconBarTextItem *iti, int x, int y)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	priv = iti->priv;

	iti->x = x;
	iti->y = y;

	priv->need_pos_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * e_icon_bar_text_item_select:
 * @iti: An #EIconBarTextItem.
 * @sel: Whether the item should be displayed as selected.
 *
 * This function is used to control whether an icon text item is displayed as
 * selected or not. Mouse events are ignored by the item when it is unselected;
 * when the user clicks on a selected icon text item, it will start the text
 * editing process.
 */
void
e_icon_bar_text_item_select (EIconBarTextItem *iti, int sel)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	priv = iti->priv;

	if (!iti->selected == !sel)
		return;

	iti->selected = sel ? TRUE : FALSE;

	if (!iti->selected && iti->editing)
		iti_edition_accept (iti);

	priv->need_state_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * e_icon_bar_text_item_get_text:
 * @iti: An #EIconBarTextItem.
 *
 * Returns the current text.  The client should not free this string, as it is
 * internal to the #EIconBarTextItem.
 */
char *
e_icon_bar_text_item_get_text (EIconBarTextItem *iti)
{
	ItiPrivate *priv;

	g_return_val_if_fail (iti != NULL, NULL);
	g_return_val_if_fail (IS_ITI (iti), NULL);

	priv = iti->priv;

	if (iti->editing)
		return gtk_entry_get_text (priv->entry);
	else
		return iti->text;
}


/**
 * e_icon_bar_text_item_set_text:
 * @iti: An #EIconBarTextItem.
 * @text: Text that is going to be displayed.
 * @is_static: Whether @text points to a static string or not.
 *
 * If @is_static is true, it means that there is no need for the item to
 * allocate memory for the string (it is a guarantee that the text is allocated
 * by the caller and it will not be deallocated during the lifetime of this
 * item).  This is an optimization to reduce memory usage for large icon sets.
 */
void
e_icon_bar_text_item_set_text (EIconBarTextItem *iti, const char *text,
			       gboolean is_static)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));
	g_return_if_fail (text != NULL);

	priv = iti->priv;

	if (iti->text && iti->is_text_allocated)
		g_free (iti->text);

	iti->is_text_allocated = !is_static;

	/* This cast is to shut up the compiler */
	if (is_static)
		iti->text = (char *) text;
	else
		iti->text = g_strdup (text);

	/* Request update */

	priv->need_text_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}


/**
 * e_icon_bar_text_item_start_editing:
 * @iti: An #EIconBarTextItem.
 *
 * Starts the editing state of an #EIconBarTextItem.
 **/
void
e_icon_bar_text_item_start_editing (EIconBarTextItem *iti)
{
	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	if (iti->editing)
		return;

	iti->selected = TRUE; /* Ensure that we are selected */
	gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (iti));
	iti_start_editing (iti);
}

/**
 * e_icon_bar_text_item_stop_editing:
 * @iti: An #EIconBarTextItem.
 * @accept: Whether to accept the current text or to discard it.
 *
 * Terminates the editing state of an icon text item.  The @accept argument
 * controls whether the item's current text should be accepted or discarded.
 * If it is discarded, then the icon's original text will be restored.
 **/
void
e_icon_bar_text_item_stop_editing (EIconBarTextItem *iti,
				   gboolean accept)
{
	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	if (!iti->editing)
		return;

	if (accept)
		iti_edition_accept (iti);
	else
		iti_stop_editing (iti);
}


/**
 * e_icon_bar_text_item_get_type:
 *
 * Registers the &EIconBarTextItem class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: the type ID of the #EIconBarTextItem class.
 **/
GtkType
e_icon_bar_text_item_get_type (void)
{
	static GtkType iti_type = 0;

	if (!iti_type) {
		static const GtkTypeInfo iti_info = {
			"EIconBarTextItem",
			sizeof (EIconBarTextItem),
			sizeof (EIconBarTextItemClass),
			(GtkClassInitFunc) iti_class_init,
			(GtkObjectInitFunc) iti_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		iti_type = gtk_type_unique (gnome_canvas_item_get_type (), &iti_info);
	}

	return iti_type;
}


static void
free_row (gpointer data, gpointer user_data)
{
	EIconBarTextItemInfoRow *row;

	if (data) {
		row = data;
		g_free (row->text);
		g_free (row->text_wc);
		g_free (row);
	}
}

/*
 * e_icon_bar_text_item_free_info:
 * @ti: An icon text info structure.
 *
 * Frees a &EIconBarTextItemInfo structure.  You should call this instead of
 * freeing the structure yourself.
 */
static void
e_icon_bar_text_item_free_info (EIconBarTextItemInfo *ti)
{
	g_list_foreach (ti->rows, free_row, NULL);
	g_list_free (ti->rows);
	g_free (ti);
}

/*
 * e_icon_bar_text_item_layout_text:
 * @font:       Name of the font that will be used to render the text.
 * @text:       Text to be formatted.
 * @separators: Separators used for word wrapping, can be NULL.
 * @max_width:  Width in pixels to be used for word wrapping.
 * @confine:    Whether it is mandatory to wrap at @max_width.
 *
 * Creates a new &EIconBarTextItemInfo structure by wrapping the specified
 * text.  If non-NULL, the @separators argument defines a set of characters
 * to be used as word delimiters for performing word wrapping.  If it is
 * NULL, then only spaces will be used as word delimiters.
 *
 * The @max_width argument is used to specify the width at which word
 * wrapping will be performed.  If there is a very long word that does not
 * fit in a single line, the @confine argument can be used to specify
 * whether the word should be unconditionally split to fit or whether
 * the maximum width should be increased as necessary.
 *
 * Return value: A newly-created &EIconBarTextItemInfo structure.
 */
static EIconBarTextItemInfo *
e_icon_bar_text_item_layout_text (EIconBarTextItem *iti, GdkFont *font,
				  const gchar *text, const gchar *separators,
				  gint max_width, gboolean confine)
{
	EIconBarTextItemInfo *ti;
	EIconBarTextItemInfoRow *row;
	GdkWChar *row_end;
	GdkWChar *s, *word_start, *word_end, *old_word_end;
	GdkWChar *sub_text;
	int i, w_len, w;
	GdkWChar *text_wc, *text_iter, *separators_wc;
	int text_len_wc, separators_len_wc;
	gboolean restrict_lines;
	int lines;

	g_return_val_if_fail (font != NULL, NULL);
	g_return_val_if_fail (text != NULL, NULL);

	if (!separators)
		separators = " ";

	text_wc = g_new (GdkWChar, strlen (text) + 1);
	text_len_wc = gdk_mbstowcs (text_wc, text, strlen (text));
	if (text_len_wc < 0) text_len_wc = 0;
	text_wc[text_len_wc] = 0;

	separators_wc = g_new (GdkWChar, strlen (separators) + 1);
	separators_len_wc = gdk_mbstowcs (separators_wc, separators, strlen (separators));
	if (separators_len_wc < 0) separators_len_wc = 0;
	separators_wc[separators_len_wc] = 0;
	
	ti = g_new (EIconBarTextItemInfo, 1);

	ti->rows = NULL;
	ti->font = font;
	ti->width = 0;
	ti->height = 0;
	ti->baseline_skip = font->ascent + font->descent;

	word_end = NULL;

	if (!iti->editing && iti->max_lines != -1)
		restrict_lines = TRUE;
	else
		restrict_lines = FALSE;

	text_iter = text_wc;
	lines = 0;
	while (*text_iter) {
		/* If we are restricting the height, and this is the last line,
		   and we are displaying the ellipsis, then subtract the width
		   of the ellipsis from our max_width. */
		if (restrict_lines && lines == iti->max_lines - 1
		    && iti->show_ellipsis) {
			max_width -= gdk_string_measure (font, e_icon_bar_text_item_ellipsis);
		}

		for (row_end = text_iter; *row_end != 0 && *row_end != '\n'; row_end++);

		/* Accumulate words from this row until they don't fit in the max_width */

		s = text_iter;

		while (s < row_end) {
			word_start = s;
			old_word_end = word_end;
			for (word_end = word_start; *word_end; word_end++) {
				GdkWChar *p;
				for (p = separators_wc; *p; p++) {
					if (*word_end == *p)
						goto found;
				}
			}
		  found:
			if (word_end < row_end)
				word_end++;

			if (gdk_text_width_wc (font, text_iter, word_end - text_iter) > max_width) {
				if (word_start == text_iter
				    || (restrict_lines
					&& lines == iti->max_lines - 1)) {
					if (confine) {
						/* We must force-split the word.  Look for a proper
                                                 * place to do it.
						 */

						w_len = word_end - text_iter;

						for (i = 1; i < w_len; i++) {
							w = gdk_text_width_wc (font, text_iter, i);
							if (w > max_width) {
								if (i == 1)
									/* Shit, not even a single character fits */
									max_width = w;
								else
									break;
							}
						}

						/* Create sub-row with the chars that fit */

						sub_text = g_new (GdkWChar, i);
						memcpy (sub_text, text_iter, (i - 1) * sizeof (GdkWChar));
						sub_text[i - 1] = 0;
						
						row = g_new (EIconBarTextItemInfoRow, 1);
						row->text_wc = sub_text;
						row->text_length = i - 1;
						row->width = gdk_text_width_wc (font, sub_text, i - 1);
						row->text = gdk_wcstombs(sub_text);
						if (row->text == NULL)
							row->text = g_strdup("");

						ti->rows = g_list_append (ti->rows, row);

						if (row->width > ti->width)
							ti->width = row->width;

						ti->height += ti->baseline_skip;

						/* Bump the text pointer */

						text_iter += i - 1;
						s = text_iter;

						lines++;
						if (restrict_lines
						    && lines >= iti->max_lines)
							break;

						continue;
					} else
						max_width = gdk_text_width_wc (font, word_start, word_end - word_start);

					continue; /* Retry split */
				} else {
					word_end = old_word_end; /* Restore to region that does fit */
					break; /* Stop the loop because we found something that doesn't fit */
				}
			}

			s = word_end;
		}

		if (restrict_lines && lines >= iti->max_lines)
			break;

		/* Append row */

		if (text_iter == row_end) {
			/* We are on a newline, so append an empty row */

			ti->rows = g_list_append (ti->rows, NULL);
			ti->height += ti->baseline_skip;

			/* Next! */

			text_iter = row_end + 1;

			lines++;
			if (restrict_lines && lines >= iti->max_lines)
				break;

		} else {
			/* Create subrow and append it to the list */

			int sub_len;
			sub_len = word_end - text_iter;

			sub_text = g_new (GdkWChar, sub_len + 1);
			memcpy (sub_text, text_iter, sub_len * sizeof (GdkWChar));
			sub_text[sub_len] = 0;

			row = g_new (EIconBarTextItemInfoRow, 1);
			row->text_wc = sub_text;
			row->text_length = sub_len;
			row->width = gdk_text_width_wc (font, sub_text, sub_len);
			row->text = gdk_wcstombs(sub_text);
			if (row->text == NULL)
				row->text = g_strdup("");

			ti->rows = g_list_append (ti->rows, row);

			if (row->width > ti->width)
				ti->width = row->width;

			ti->height += ti->baseline_skip;

			/* Next! */

			text_iter = word_end;

			lines++;
			if (restrict_lines && lines >= iti->max_lines)
				break;
		}
	}

	/* Check if we've had to clip the text. */
	iti->is_clipped = *text_iter ? TRUE : FALSE;

	g_free (text_wc);
	g_free (separators_wc);
	return ti;
}

/*
 * e_icon_bar_text_item_paint_text:
 * @ti:       An icon text info structure.
 * @drawable: Target drawable.
 * @gc:       GC used to render the string.
 * @x:        Left coordinate for text.
 * @y:        Upper coordinate for text.
 * @just:     Justification for text.
 *
 * Paints the formatted text in the icon text info structure onto a drawable.
 * This is just a sample implementation; applications can choose to use other
 * rendering functions.
 */
static void
e_icon_bar_text_item_paint_text (EIconBarTextItem *iti,
				 EIconBarTextItemInfo *ti,
				 GdkDrawable *drawable, GdkGC *gc,
				 gint x, gint y, GtkJustification just)
{
	GList *item;
	EIconBarTextItemInfoRow *row;
	int xpos, line, width;
	gboolean show_ellipsis;

	g_return_if_fail (ti != NULL);
	g_return_if_fail (drawable != NULL);
	g_return_if_fail (gc != NULL);

	y += ti->font->ascent;

	for (item = ti->rows, line = 1; item; item = item->next, line++) {

		if (item->data) {
			row = item->data;
			width = row->width;
		}

		/* If this is the last line, and the text has been clipped,
		   and show_ellipsis is TRUE, display '...' */
		if (line == iti->max_lines && iti->is_clipped) {
			show_ellipsis = TRUE;
			width += gdk_string_measure (ti->font, e_icon_bar_text_item_ellipsis);
		} else {
			show_ellipsis = FALSE;
		}

		switch (just) {
		case GTK_JUSTIFY_LEFT:
			xpos = 0;
			break;

		case GTK_JUSTIFY_RIGHT:
			xpos = ti->width - width;
			break;

		case GTK_JUSTIFY_CENTER:
			xpos = (ti->width - width) / 2;
			break;

		default:
			/* Anyone care to implement GTK_JUSTIFY_FILL? */
			g_warning ("Justification type %d not supported.  Using left-justification.",
				   (int) just);
			xpos = 0;
		}

		if (item->data)
			gdk_draw_text_wc (drawable, ti->font, gc, x + xpos, y, row->text_wc, row->text_length);

		if (show_ellipsis)
			gdk_draw_string (drawable, ti->font, gc,
					 x + xpos + row->width, y,
					 e_icon_bar_text_item_ellipsis);
			
		y += ti->baseline_skip;
	}
}
