/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell-text.c: Text cell renderer.
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 *          Chris Lahey <clahey@ximian.com>
 *
 * A lot of code taken from:
 *
 * Text item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent
 * canvas widget.  Tk is copyrighted by the Regents of the University
 * of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <gdk/gdkx.h> /* for BlackPixel */
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include "e-cell-text.h"
#include "gal/util/e-util.h"
#include "gal/widgets/e-canvas.h"
#include "gal/widgets/e-unicode.h"
#include "e-table-item.h"
#include "gal/util/e-text-event-processor.h"
#include "gal/e-text/e-text.h"
#include "gal/util/e-text-event-processor-emacs-like.h"
#include "gal/util/e-i18n.h"
#include "e-table-tooltip.h"
#include "gal/a11y/e-table/gal-a11y-e-cell-registry.h"
#include "gal/a11y/e-table/gal-a11y-e-cell-text.h"

#define d(x)
#define DO_SELECTION 1
#define VIEW_TO_CELL(view) E_CELL_TEXT (((ECellView *)view)->ecell)

#if d(!)0
#define e_table_item_leave_edit_(x) (e_table_item_leave_edit((x)), g_print ("%s: e_table_item_leave_edit\n", __FUNCTION__))
#else
#define e_table_item_leave_edit_(x) (e_table_item_leave_edit((x)))
#endif

#define ECT_CLASS(c) (E_CELL_TEXT_CLASS(GTK_OBJECT_GET_CLASS ((c))))

/* This defines a line of text */
struct line {
	char *text;	/* Line's text UTF-8, it is a pointer into the text->text string */
	int length;	/* Line's length in BYTES */
	int width;	/* Line's width in pixels */
	int ellipsis_length;  /* Length before adding ellipsis in BYTES */
};

/* Object argument IDs */
enum {
	PROP_0,

	PROP_STRIKEOUT_COLUMN,
	PROP_UNDERLINE_COLUMN,
	PROP_BOLD_COLUMN,
	PROP_COLOR_COLUMN,
	PROP_EDITABLE,
	PROP_BG_COLOR_COLUMN
};


enum {
	E_SELECTION_PRIMARY,
	E_SELECTION_CLIPBOARD
};

/* signals */
enum {
	TEXT_INSERTED,
	TEXT_DELETED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static GdkAtom clipboard_atom = GDK_NONE;

#define PARENT_TYPE e_cell_get_type ()

#define UTF8_ATOM  gdk_atom_intern ("UTF8_STRING", FALSE)

#define TEXT_PAD 4

typedef struct {
	gpointer lines;			/* Text split into lines (private field) */
	int num_lines;			/* Number of lines of text */
	int max_width;
	int ref_count;
} ECellTextLineBreaks;
	

typedef struct _CellEdit CellEdit;

typedef struct {
	ECellView    cell_view;
	GdkGC       *gc;
	GdkCursor *i_cursor;
	GdkBitmap *stipple;		/* Stipple for text */
	
	GnomeCanvas *canvas;

	/*
	 * During editing.
	 */
	CellEdit    *edit;


	int xofs, yofs;                 /* This gets added to the x
                                           and y for the cell text. */
	double ellipsis_width[2];      /* The width of the ellipsis. */

} ECellTextView;

struct _CellEdit {

	ECellTextView *text_view;

	int model_col, view_col, row;
	int cell_width;

	PangoLayout *layout;

	char *text;

	char         *old_text;

	/*
	 * Where the editing is taking place
	 */

	int xofs_edit, yofs_edit;       /* Offset because of editing.
                                           This is negative compared
                                           to the other offsets. */

	/* This needs to be reworked a bit once we get line wrapping. */
	int selection_start;            /* Start of selection - IN BYTES */
	int selection_end;              /* End of selection - IN BYTES */
	gboolean select_by_word;        /* Current selection is by word */

	/* This section is for drag scrolling and blinking cursor. */
	/* Cursor handling. */
	gint timeout_id;                /* Current timeout id for scrolling */
	GTimer *timer;                  /* Timer for blinking cursor and scrolling */

	gint lastx, lasty;              /* Last x and y motion events */
	gint last_state;                /* Last state */
	gulong scroll_start;            /* Starting time for scroll (microseconds) */

	gint show_cursor;               /* Is cursor currently shown */
	gboolean button_down;           /* Is mouse button 1 down */

	ETextEventProcessor *tep;       /* Text Event Processor */

	GtkWidget *invisible;           /* For selection handling */
	gboolean has_selection;         /* TRUE if we have the selection */
	gchar *primary_selection;       /* Primary selection text */
	gint primary_length;            /* Primary selection text length in BYTES */
	gchar *clipboard_selection;     /* Clipboard selection text */
	gint clipboard_length;          /* Clipboard selection text length in BYTES */

	guint pointer_in : 1;
	guint default_cursor_shown : 1;
	GtkIMContext *im_context;
	gboolean need_im_reset;
	gboolean im_context_signals_registered;

	guint16 preedit_length;       /* length of preedit string, in bytes */

	ECellActions actions;
};

static void e_cell_text_view_command (ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data);

static void e_cell_text_view_get_selection (CellEdit *edit, GdkAtom selection, guint32 time);
static void e_cell_text_view_supply_selection (CellEdit *edit, guint time, GdkAtom selection, char *data, gint length);

static void _get_tep (CellEdit *edit);

static gint get_position_from_xy (CellEdit *edit, gint x, gint y);
static gboolean _blink_scroll_timeout (gpointer data);

static void ect_free_color (gchar *color_spec, GdkColor *color, GdkColormap *colormap);
static GdkColor* e_cell_text_get_color (ECellTextView *cell_view, gchar *color_spec);
static void e_cell_text_preedit_changed_cb (GtkIMContext *context, ECellTextView *text_view);
static void e_cell_text_commit_cb (GtkIMContext *context, const gchar  *str, ECellTextView *text_view);
static gboolean e_cell_text_retrieve_surrounding_cb (GtkIMContext *context, ECellTextView *text_view);
static gboolean e_cell_text_delete_surrounding_cb   (GtkIMContext *context, gint          offset, gint          n_chars, ECellTextView        *text_view);
static void _insert (ECellTextView *text_view, char *string, int value);
static void _delete_selection (ECellTextView *text_view);
static PangoAttrList* build_attr_list (ECellTextView *text_view, int row, int text_length);

static ECellClass *parent_class;

char *
e_cell_text_get_text (ECellText *cell, ETableModel *model, int col, int row)
{
	if (ECT_CLASS(cell)->get_text)
		return ECT_CLASS(cell)->get_text (cell, model, col, row);
	else
		return NULL;
}

void
e_cell_text_free_text (ECellText *cell, char *text)
{
	if (ECT_CLASS(cell)->free_text)
		ECT_CLASS(cell)->free_text (cell, text);
}

void
e_cell_text_set_value (ECellText *cell, ETableModel *model, int col, int row,
		       const char *text)
{
	if (ECT_CLASS(cell)->set_value)
		ECT_CLASS(cell)->set_value (cell, model, col, row, text);
}

static char *
ect_real_get_text (ECellText *cell, ETableModel *model, int col, int row)
{
	return e_table_model_value_at(model, col, row);
}

static void
ect_real_free_text (ECellText *cell, char *text)
{
}

/* This is the default method for setting the ETableModel value based on
   the text in the ECellText. This simply uses the text as it is - it assumes
   the value in the model is a char*. Subclasses may parse the text into
   data structures to pass to the model. */
static void
ect_real_set_value (ECellText *cell, ETableModel *model, int col, int row,
		    const char *text)
{
	e_table_model_set_value_at (model, col, row, text);
}

static void
ect_queue_redraw (ECellTextView *text_view, int view_col, int view_row)
{
	e_table_item_redraw_range (
		text_view->cell_view.e_table_item_view,
		view_col, view_row, view_col, view_row);
}

/*
 * Shuts down the editing process
 */
static void
ect_stop_editing (ECellTextView *text_view, gboolean commit)
{
	CellEdit *edit = text_view->edit;
	int row, view_col, model_col;
	char *old_text, *text;

	if (!edit)
		return;

	row = edit->row;
	view_col = edit->view_col;
	model_col = edit->model_col;
	
	old_text = edit->old_text;
	text = edit->text;
	if (edit->invisible)
		gtk_widget_destroy (edit->invisible);
	if (edit->tep)
		g_object_unref (edit->tep);
	if (edit->primary_selection)
		g_free (edit->primary_selection);
	if (edit->clipboard_selection)
		g_free (edit->clipboard_selection);
	if (! edit->default_cursor_shown){
		gdk_window_set_cursor (GTK_WIDGET(text_view->canvas)->window, NULL);
		edit->default_cursor_shown = TRUE;
	}
	if (edit->timeout_id) {
		g_source_remove (edit->timeout_id);
		edit->timeout_id = 0;
	}
	if (edit->timer) {
		g_timer_stop (edit->timer);
		g_timer_destroy (edit->timer);
		edit->timer = NULL;
	}
	
	g_signal_handlers_disconnect_matched (
		edit->im_context,
		G_SIGNAL_MATCH_DATA, 0, 0,
		NULL, NULL, text_view);

	if (edit->layout)
		g_object_unref (edit->layout);

	g_free (edit);

	text_view->edit = NULL;
	if (commit) {
		/*
		 * Accept the currently edited text.  if it's the same as what's in the cell, do nothing.
		 */
		ECellView *ecell_view = (ECellView *) text_view;
		ECellText *ect = (ECellText *) ecell_view->ecell;

		if (strcmp (old_text, text)) {
			e_cell_text_set_value (ect, ecell_view->e_table_model,
					       model_col, row, text);
		}
	}
	g_free (text);
	g_free (old_text);

	ect_queue_redraw (text_view, view_col, row);
}

/*
 * Cancels the edits
 */
static void
ect_cancel_edit (ECellTextView *text_view)
{
	ect_stop_editing (text_view, FALSE);
	e_table_item_leave_edit_ (text_view->cell_view.e_table_item_view);
}

/*
 * ECell::new_view method
 */
static ECellView *
ect_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	ECellTextView *text_view = g_new0 (ECellTextView, 1);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (e_table_item_view)->canvas;
	
	text_view->cell_view.ecell = ecell;
	text_view->cell_view.e_table_model = table_model;
	text_view->cell_view.e_table_item_view = e_table_item_view;

	text_view->canvas = canvas;

	text_view->xofs = 0.0;
	text_view->yofs = 0.0;
	
	return (ECellView *)text_view;
}

/*
 * ECell::kill_view method
 */
static void
ect_kill_view (ECellView *ecv)
{
	ECellTextView *text_view = (ECellTextView *) ecv;

	g_free (text_view);
}

/*
 * ECell::realize method
 */
static void
ect_realize (ECellView *ecell_view)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	
	text_view->gc = gdk_gc_new (GTK_WIDGET (text_view->canvas)->window);

	text_view->i_cursor = gdk_cursor_new (GDK_XTERM);
	
	if (parent_class->realize)
		(* parent_class->realize) (ecell_view);
}

/*
 * ECell::unrealize method
 */
static void
ect_unrealize (ECellView *ecv)
{
	ECellTextView *text_view = (ECellTextView *) ecv;
	ECellText *ect = (ECellText*) ecv->ecell;
	GdkColormap *colormap;

	gdk_gc_unref (text_view->gc);
	text_view->gc = NULL;

	if (text_view->edit){
		ect_cancel_edit (text_view);
	}

	if (text_view->stipple)
		gdk_bitmap_unref (text_view->stipple);

	gdk_cursor_destroy (text_view->i_cursor);

	if (ect->colors) {
		colormap = gtk_widget_get_colormap (GTK_WIDGET (text_view->canvas));
		g_hash_table_foreach (ect->colors, (GHFunc) ect_free_color,
				      colormap);
		g_hash_table_destroy (ect->colors);
		ect->colors = NULL;
	}

	if (parent_class->unrealize)
		(* parent_class->unrealize) (ecv);
}

static void
ect_free_color (gchar *color_spec, GdkColor *color, GdkColormap *colormap)
{
	g_free (color_spec);

	/* This frees the color. Note we don't free it if it is the special
	   value. */
	if (color != (GdkColor*) 1) {
		gulong pix = color->pixel;

		gdk_colors_free (colormap, &pix, 1, 0);

		/* This frees the memory for the GdkColor. */
		gdk_color_free (color);
	}
}


static PangoAttrList*
build_attr_list (ECellTextView *text_view, int row, int text_length) 
{

	ECellView *ecell_view = (ECellView *) text_view;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	PangoAttrList *attrs = pango_attr_list_new ();
	gboolean bold, strikeout, underline;

	bold = ect->bold_column >= 0 &&
		row >= 0 &&
		e_table_model_value_at(ecell_view->e_table_model, ect->bold_column, row);
	strikeout = ect->strikeout_column >= 0 &&
		row >= 0 &&
		e_table_model_value_at(ecell_view->e_table_model, ect->strikeout_column, row);
	underline = ect->underline_column >= 0 &&
		row >= 0 &&
		e_table_model_value_at(ecell_view->e_table_model, ect->underline_column, row);

	if (bold || strikeout || underline) {
		if (bold) {
			PangoAttribute *attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
			attr->start_index = 0;
			attr->end_index = text_length;

			pango_attr_list_insert_before (attrs, attr);
		}
		if (strikeout) {
			PangoAttribute *attr = pango_attr_strikethrough_new (TRUE);
			attr->start_index = 0;
			attr->end_index = text_length;

			pango_attr_list_insert_before (attrs, attr);
		}
		if (underline) {
			PangoAttribute *attr = pango_attr_underline_new (TRUE);
			attr->start_index = 0;
			attr->end_index = text_length;

			pango_attr_list_insert_before (attrs, attr);
		}
	}
	return attrs;
}

static PangoLayout *
layout_with_preedit (ECellTextView *text_view, int row, const char *text, gint width)
{
	CellEdit *edit = text_view->edit;
	PangoAttrList *attrs ;
	PangoLayout *layout;
	GString *tmp_string = g_string_new (NULL);
	PangoAttrList *preedit_attrs = NULL;
	gchar *preedit_string = NULL;
	gint preedit_length = 0;
	gint text_length = strlen (text);
	gint mlen = MIN(edit->selection_start,text_length);
	

	gtk_im_context_get_preedit_string (edit->im_context,
					&preedit_string,&preedit_attrs,
					NULL);
	preedit_length = edit->preedit_length = strlen (preedit_string);;

	layout = edit->layout;

	g_string_prepend_len (tmp_string, text,text_length); 

	if (preedit_length) {
		
		/* mlen is the text_length in bytes, not chars
		 * check whether we are not inserting into
		 * the middle of a utf8 character
		 */

		if (mlen < text_length) {
			if (!g_utf8_validate (text+mlen, -1, NULL)) {
				gchar *tc;
				tc = g_utf8_find_next_char (text+mlen,NULL);
				if (tc) {
					mlen = (gint) (tc - text);
				}
			}
		}

		g_string_insert (tmp_string, mlen, preedit_string);
	} 

	pango_layout_set_text (layout, tmp_string->str, tmp_string->len);

	attrs = (PangoAttrList *) build_attr_list (text_view, row, text_length);

	if (preedit_length)
		pango_attr_list_splice (attrs, preedit_attrs, mlen, preedit_length);
	pango_layout_set_attributes (layout, attrs);
	g_string_free (tmp_string, TRUE);
	if (preedit_string)
		g_free (preedit_string);
	if (preedit_attrs)
		pango_attr_list_unref (preedit_attrs);
	pango_attr_list_unref (attrs);
	return layout;
}

static PangoLayout *
build_layout (ECellTextView *text_view, int row, const char *text, gint width)
{
	ECellView *ecell_view = (ECellView *) text_view;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	PangoAttrList *attrs ;
	PangoLayout *layout;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (((GnomeCanvasItem *)ecell_view->e_table_item_view)->canvas), text);

	attrs = (PangoAttrList *) build_attr_list (text_view, row, text ? strlen (text) : 0);

	pango_layout_set_attributes (layout, attrs);
	pango_attr_list_unref (attrs);

	if (text_view->edit || width <= 0)
		return layout;

	pango_layout_set_width (layout, width * PANGO_SCALE);
	pango_layout_set_wrap (layout, PANGO_WRAP_CHAR);

	if (pango_layout_get_line_count (layout) > 1) {
		PangoLayoutLine *line = pango_layout_get_line (layout, 0);
		gchar *line_text = g_strdup (pango_layout_get_text (layout));
		gchar *last_char = g_utf8_find_prev_char (line_text, line_text + line->length - 1);
		while (last_char && pango_layout_get_line_count (layout) > 1) {
			gchar *new_text;
			last_char = g_utf8_find_prev_char (line_text, last_char);
			if (last_char)
				*last_char = '\0';
			new_text = g_strconcat (line_text, "...", NULL);
			pango_layout_set_text (layout, new_text, -1);
			g_free (new_text);
		}
		g_free (line_text);
	}

	switch (ect->justify) {
	case GTK_JUSTIFY_RIGHT:
		pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);
		break;
	case GTK_JUSTIFY_CENTER:
		pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
		break;
	case GTK_JUSTIFY_LEFT:
	default:
		break;
	}
	
	return layout;
}

static PangoLayout *
generate_layout (ECellTextView *text_view, int model_col, int view_col, int row, int width)
{
	ECellView *ecell_view = (ECellView *) text_view;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	PangoLayout *layout;
	CellEdit *edit = text_view->edit;

	if (edit && edit->layout && edit->model_col == model_col && edit->row == row) {
		g_object_ref (edit->layout);
		return edit->layout;
	}

	if (row >= 0) {
		char *temp = e_cell_text_get_text(ect, ecell_view->e_table_model, model_col, row);
		layout = build_layout (text_view, row, temp ? temp : "?", width);
		e_cell_text_free_text(ect, temp);
	} else
		layout = build_layout (text_view, row, "Mumbo Jumbo", width);

	return layout;
}


static void
draw_pango_rectangle (GdkDrawable *drawable, GdkGC *gc, int x1, int y1, PangoRectangle rect)
{
	int width = rect.width / PANGO_SCALE;
	int height = rect.height / PANGO_SCALE;
	if (width <= 0)
		width = 1;
	if (height <= 0)
		height = 1;
	gdk_draw_rectangle (drawable, gc, TRUE,
			    x1 + rect.x / PANGO_SCALE, y1 + rect.y / PANGO_SCALE, width, height);
}

static gboolean
show_pango_rectangle (CellEdit *edit, PangoRectangle rect)
{
	int x1 = rect.x / PANGO_SCALE;
	int x2 = (rect.x + rect.width) / PANGO_SCALE;
#if 0
	int y1 = rect.y / PANGO_SCALE;
	int y2 = (rect.y + rect.height) / PANGO_SCALE;
#endif

	int new_xofs_edit = edit->xofs_edit;
	int new_yofs_edit = edit->yofs_edit;

	if (x1 < new_xofs_edit)
		new_xofs_edit = x1;
	if (2 + x2 - edit->cell_width > new_xofs_edit)
		new_xofs_edit = 2 + x2 - edit->cell_width;
	if (new_xofs_edit < 0)
		new_xofs_edit = 0;

#if 0
	if (y1 < new_yofs_edit)
		new_yofs_edit = y1;
	if (2 + y2 - edit->cell_height > new_yofs_edit)
		new_yofs_edit = 2 + y2 - edit->cell_height;
	if (new_yofs_edit < 0)
		new_yofs_edit = 0;
#endif

	if (new_xofs_edit != edit->xofs_edit ||
	    new_yofs_edit != edit->yofs_edit) {
		edit->xofs_edit = new_xofs_edit;
		edit->yofs_edit = new_yofs_edit;
		return TRUE;
	}

	return FALSE;
}

/*
 * ECell::draw method
 */
static void
ect_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int model_col, int view_col, int row, ECellFlags flags,
	  int x1, int y1, int x2, int y2)
{
	PangoLayout *layout;
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	CellEdit *edit = text_view->edit;
	gboolean selected;
	GdkColor *foreground, *cursor_color;
	GtkWidget *canvas = GTK_WIDGET (text_view->canvas);
	GdkRectangle clip_rect;
	int x_origin, y_origin;

	selected = flags & E_CELL_SELECTED;

	if (selected) {
		if (flags & E_CELL_FOCUSED)
			foreground = &canvas->style->fg [GTK_STATE_SELECTED];
		else
			foreground = &canvas->style->fg [GTK_STATE_ACTIVE];
		cursor_color = foreground;
	} else {
		foreground = &canvas->style->text [GTK_STATE_NORMAL];
		cursor_color = foreground;

		if (ect->color_column != -1) {
			char *color_spec;
			GdkColor *cell_foreground;

			color_spec = e_table_model_value_at (ecell_view->e_table_model,
							     ect->color_column, row);
			cell_foreground = e_cell_text_get_color (text_view,
								 color_spec);
			if (cell_foreground)
				foreground = cell_foreground;
		}
	}

	gdk_gc_set_foreground (text_view->gc, foreground);

	x1 += 4;
	y1 += 1;
	x2 -= 4;
	y2 -= 1;

	x_origin = x1 + ect->x + text_view->xofs - (edit ? edit->xofs_edit : 0);
	y_origin = y1 + ect->y + text_view->yofs - (edit ? edit->yofs_edit : 0);

	clip_rect.x = x1;
	clip_rect.y = y1;
	clip_rect.width = x2 - x1;
	clip_rect.height = y2 - y1;

	gdk_gc_set_clip_rectangle (text_view->gc, &clip_rect);
	/*	clip_rect = &rect;*/

	layout = generate_layout (text_view, model_col, view_col, row, x2 - x1);

	if (edit && edit->view_col == view_col && edit->row == row) {
		layout = layout_with_preedit  (text_view, row, edit->text ? edit->text : "?",  x2 - x1);
	} 

	gdk_draw_layout (drawable, text_view->gc,
			 x_origin, y_origin,
			 layout);

	if (edit && edit->view_col == view_col && edit->row == row) {
		if (edit->selection_start != edit->selection_end) {
			int start_index, end_index;
			PangoLayoutLine *line;
			gint *ranges;
			gint n_ranges, i;
			PangoRectangle logical_rect;
			GdkRegion *clip_region = gdk_region_new ();
			GdkRegion *rect_region;
			GdkGC *selection_gc;
			GdkGC *text_gc;

			start_index = MIN (edit->selection_start, edit->selection_end);
			end_index = edit->selection_start ^ edit->selection_end ^ start_index;

			if (edit->has_selection) {
				selection_gc = canvas->style->base_gc [GTK_STATE_SELECTED];
				text_gc = canvas->style->text_gc[GTK_STATE_SELECTED];
			} else {
				selection_gc = canvas->style->base_gc [GTK_STATE_ACTIVE];
				text_gc = canvas->style->text_gc[GTK_STATE_ACTIVE];
			}

			gdk_gc_set_clip_rectangle (selection_gc, &clip_rect);

			line = pango_layout_get_lines (layout)->data;

			pango_layout_line_get_x_ranges (line, start_index, end_index, &ranges, &n_ranges);

			pango_layout_get_extents (layout, NULL, &logical_rect);

			for (i=0; i < n_ranges; i++) {
				GdkRectangle sel_rect;

				sel_rect.x = x_origin + ranges[2*i] / PANGO_SCALE;
				sel_rect.y = y_origin;
				sel_rect.width = (ranges[2*i + 1] - ranges[2*i]) / PANGO_SCALE;
				sel_rect.height = logical_rect.height / PANGO_SCALE;

				gdk_draw_rectangle (drawable, selection_gc, TRUE,
						    sel_rect.x, sel_rect.y, sel_rect.width, sel_rect.height);

				gdk_region_union_with_rect (clip_region, &sel_rect);
			}

			rect_region = gdk_region_rectangle (&clip_rect);
			gdk_region_intersect (clip_region, rect_region);
			gdk_region_destroy (rect_region);

			gdk_gc_set_clip_region (text_gc, clip_region);
			gdk_draw_layout (drawable, text_gc, 
					 x_origin, y_origin,
					 layout);
			gdk_gc_set_clip_region (text_gc, NULL);
			gdk_gc_set_clip_region (selection_gc, NULL);

			gdk_region_destroy (clip_region);
			g_free (ranges);
		} else {
			if (edit->show_cursor) {
				PangoRectangle strong_pos, weak_pos;
				pango_layout_get_cursor_pos (layout, edit->selection_start + edit->preedit_length, &strong_pos, &weak_pos);
				
				draw_pango_rectangle (drawable, text_view->gc, x_origin, y_origin, strong_pos);
				if (strong_pos.x != weak_pos.x ||
				    strong_pos.y != weak_pos.y ||
				    strong_pos.width != weak_pos.width ||
				    strong_pos.height != weak_pos.height)
					draw_pango_rectangle (drawable, text_view->gc, x_origin, y_origin, weak_pos);
			}
		}
	}

	g_object_unref (layout);
}

/*
 * Get the background color
 */
static gchar *
ect_get_bg_color(ECellView *ecell_view, int row)
{
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	gchar *color_spec;

	if (ect->bg_color_column == -1)
		return NULL;

	color_spec = e_table_model_value_at (ecell_view->e_table_model,
	                                     ect->bg_color_column, row);

	return color_spec;
}


/*
 * Selects the entire string
 */

static void
ect_edit_select_all (ECellTextView *text_view)
{
	g_assert (text_view->edit);
	
	text_view->edit->selection_start = 0;
	text_view->edit->selection_end = strlen (text_view->edit->text);
}

static gboolean
key_begins_editing (GdkEventKey *event)
{
	if (event->length == 0)
		return FALSE;

	return TRUE;
}

/*
 * ECell::event method
 */
static gint
ect_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row, ECellFlags flags, ECellActions *actions)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	ETextEventProcessorEvent e_tep_event;
	gboolean edit_display = FALSE;
	gint preedit_len;
	CellEdit *edit = text_view->edit;
	GtkWidget *canvas = GTK_WIDGET (text_view->canvas);
	gint return_val = 0;
	d(gboolean press = FALSE);

	if (!(flags & E_CELL_EDITING))
		return 0;
	
	if ( edit && !edit->preedit_length && flags & E_CELL_PREEDIT)
		return TRUE;

	if (edit && edit->view_col == view_col && edit->row == row) {
		edit_display = TRUE;
	}

	e_tep_event.type = event->type;
	switch (event->type) {
	case GDK_FOCUS_CHANGE:
		break;
	case GDK_KEY_PRESS: /* Fall Through */
		if (edit_display) {
			if (edit->im_context && 
				!edit->im_context_signals_registered) {

				g_signal_connect (edit->im_context, 
						"preedit_changed",
						G_CALLBACK (\
						e_cell_text_preedit_changed_cb),
						text_view);

				g_signal_connect (edit->im_context, 
						"commit",
						G_CALLBACK (\
						e_cell_text_commit_cb), 
						text_view);

				g_signal_connect (edit->im_context, 
						"retrieve_surrounding",
						G_CALLBACK (\
						e_cell_text_retrieve_surrounding_cb), 
						text_view);

				g_signal_connect (edit->im_context, 
						"delete_surrounding",
						G_CALLBACK (\
						e_cell_text_delete_surrounding_cb), 
						text_view);

				edit->im_context_signals_registered = TRUE;
			}

			edit->show_cursor = FALSE;

		} else {
			if (edit->im_context) {
				g_signal_handlers_disconnect_matched (
						edit->im_context, 
						G_SIGNAL_MATCH_DATA, 0, 0, 
						NULL, NULL, edit);
				edit->im_context_signals_registered = FALSE;
			}

			ect_stop_editing (text_view, TRUE);
			if (edit->timeout_id) {
				g_source_remove(edit->timeout_id);
				edit->timeout_id = 0;
			}
		}
		return_val = TRUE;
		/* Fallthrough */
	case GDK_KEY_RELEASE:
		preedit_len = edit->preedit_length;
		if (edit_display && edit->im_context &&
				gtk_im_context_filter_keypress (\
					edit->im_context,
					(GdkEventKey*)event)) {

			edit->need_im_reset = TRUE;
			if (preedit_len && flags & E_CELL_PREEDIT)
				return FALSE;
			else
		 		return TRUE;
		}
				
		if (event->key.keyval == GDK_Escape){
			ect_cancel_edit (text_view);
			return_val = TRUE;
			break;
		}

		if ((!edit_display) &&
		    e_table_model_is_cell_editable (ecell_view->e_table_model, model_col, row) &&
		    key_begins_editing (&event->key)) {
			  e_table_item_enter_edit (text_view->cell_view.e_table_item_view, view_col, row);
			  ect_edit_select_all (text_view);
			  edit = text_view->edit;
			  edit_display = TRUE;
		}		
		if (edit_display) {
			GdkEventKey key = event->key;
			if (key.keyval == GDK_KP_Enter || key.keyval == GDK_Return){
				e_table_item_leave_edit_ (text_view->cell_view.e_table_item_view);
			} else {
				e_tep_event.key.time = key.time;
				e_tep_event.key.state = key.state;
				e_tep_event.key.keyval = key.keyval;

				/* This is probably ugly hack, but we have to handle UTF-8 input somehow */
#if 0
				e_tep_event.key.length = key.length;
				e_tep_event.key.string = key.string;
#else
				e_tep_event.key.string = e_utf8_from_gtk_event_key (canvas, key.keyval, key.string);
				if (e_tep_event.key.string != NULL) {
					e_tep_event.key.length = strlen (e_tep_event.key.string);
				} else {
					e_tep_event.key.length = 0;
				}
#endif
				_get_tep (edit);
				return_val = e_text_event_processor_handle_event (edit->tep, &e_tep_event);
				if (e_tep_event.key.string) 
					g_free (e_tep_event.key.string);
				break;
			}
		}

		break;
	case GDK_BUTTON_PRESS: /* Fall Through */
		d(press = TRUE);
	case GDK_BUTTON_RELEASE:
		d(g_print ("%s: %s\n", __FUNCTION__, press ? "GDK_BUTTON_PRESS" : "GDK_BUTTON_RELEASE"));
		event->button.x -= 4;
		event->button.y -= 1;
		if ((!edit_display) 
		    && e_table_model_is_cell_editable (ecell_view->e_table_model, model_col, row)
		    && event->type == GDK_BUTTON_RELEASE
		    && event->button.button == 1) {
			GdkEventButton button = event->button;

			e_table_item_enter_edit (text_view->cell_view.e_table_item_view, view_col, row);
			edit = text_view->edit;
			edit_display = TRUE;
			
			e_tep_event.button.type = GDK_BUTTON_PRESS;
			e_tep_event.button.time = button.time;
			e_tep_event.button.state = button.state;
			e_tep_event.button.button = button.button;
			e_tep_event.button.position = get_position_from_xy (edit, event->button.x, event->button.y);
			_get_tep (edit);
			edit->actions = 0;
			return_val = e_text_event_processor_handle_event (edit->tep,
									  &e_tep_event);
			*actions = edit->actions;
			if (event->button.button == 1) {
				if (event->type == GDK_BUTTON_PRESS)
					edit->button_down = TRUE;
				else
					edit->button_down = FALSE;
			}
			edit->lastx = button.x;
			edit->lasty = button.y;
			edit->last_state = button.state;

			e_tep_event.button.type = GDK_BUTTON_RELEASE;
		}
		if (edit_display) {
			GdkEventButton button = event->button;
			e_tep_event.button.time = button.time;
			e_tep_event.button.state = button.state;
			e_tep_event.button.button = button.button;
			e_tep_event.button.position = get_position_from_xy (edit, event->button.x, event->button.y);
			_get_tep (edit);
			edit->actions = 0;
			return_val = e_text_event_processor_handle_event (edit->tep,
									  &e_tep_event);
			*actions = edit->actions;
			if (event->button.button == 1) {
				if (event->type == GDK_BUTTON_PRESS)
					edit->button_down = TRUE;
				else
					edit->button_down = FALSE;
			}
			edit->lastx = button.x;
			edit->lasty = button.y;
			edit->last_state = button.state;
		}
		break;
	case GDK_MOTION_NOTIFY:
		event->motion.x -= 4;
		event->motion.y -= 1;
		if (edit_display) {
			GdkEventMotion motion = event->motion;
			e_tep_event.motion.time = motion.time;
			e_tep_event.motion.state = motion.state;
			e_tep_event.motion.position = get_position_from_xy (edit, event->motion.x, event->motion.y);
			_get_tep (edit);
			edit->actions = 0;
			return_val = e_text_event_processor_handle_event (edit->tep,
									  &e_tep_event);
			*actions = edit->actions;
			edit->lastx = motion.x;
			edit->lasty = motion.y;
			edit->last_state = motion.state;
		}
		break;
	case GDK_ENTER_NOTIFY:
#if 0
		edit->pointer_in = TRUE;
#endif
		if (edit_display) {
			if (edit->default_cursor_shown){
				gdk_window_set_cursor (canvas->window, text_view->i_cursor);
				edit->default_cursor_shown = FALSE;
			}
		}
		break;
	case GDK_LEAVE_NOTIFY:
#if 0
		text_view->pointer_in = FALSE;
#endif
		if (edit_display) {
			if (! edit->default_cursor_shown){
				gdk_window_set_cursor (canvas->window, NULL);
				edit->default_cursor_shown = TRUE;
			}
		}
		break;
	default:
		break;
	}

	return return_val;
}

/*
 * ECell::height method
 */
static int
ect_height (ECellView *ecell_view, int model_col, int view_col, int row) 
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	gint height;
	PangoLayout *layout;

	layout = generate_layout (text_view, model_col, view_col, row, 0);
	pango_layout_get_pixel_size (layout, NULL, &height);
	g_object_unref (layout);
	return height + 2;
}

/*
 * ECellView::enter_edit method
 */
static void *
ect_enter_edit (ECellView *ecell_view, int model_col, int view_col, int row)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	CellEdit *edit;
	ECellText *ect = E_CELL_TEXT(ecell_view->ecell);
	char *temp;

	edit = g_new0 (CellEdit, 1);
	text_view->edit = edit;

	edit->im_context =  E_CANVAS (text_view->canvas)->im_context;
	edit->need_im_reset = FALSE;
	edit->im_context_signals_registered = FALSE;
	edit->view_col = -1;
	edit->model_col = -1;
	edit->row = -1;

	edit->text_view = text_view;
	edit->model_col = model_col;
	edit->view_col = view_col;
	edit->row = row;
	edit->cell_width = e_table_header_get_column (
		((ETableItem *)ecell_view->e_table_item_view)->header,
		view_col)->width - 8;

	edit->layout = generate_layout (text_view, model_col, view_col, row, edit->cell_width);
	
	edit->xofs_edit = 0.0;
	edit->yofs_edit = 0.0;
	
	edit->selection_start = 0;
	edit->selection_end = 0;
	edit->select_by_word = FALSE;

	edit->timeout_id = g_timeout_add (10, _blink_scroll_timeout, text_view);
	edit->timer = g_timer_new ();
	g_timer_elapsed (edit->timer, &(edit->scroll_start));
	g_timer_start (edit->timer);

	edit->lastx = 0;
	edit->lasty = 0;
	edit->last_state = 0;

	edit->scroll_start = 0;
	edit->show_cursor = TRUE;
	edit->button_down = FALSE;
	
	edit->tep = NULL;

	edit->has_selection = FALSE;
	
	edit->invisible = NULL;
	edit->primary_selection = NULL;
	edit->primary_length = 0;
	edit->clipboard_selection = NULL;
	edit->clipboard_length = 0;

	edit->pointer_in = FALSE;
	edit->default_cursor_shown = TRUE;
	
	temp = e_cell_text_get_text(ect, ecell_view->e_table_model, model_col, row);
	edit->old_text = g_strdup (temp);
	e_cell_text_free_text(ect, temp);
	edit->text = g_strdup (edit->old_text);

#if 0
	if (edit->pointer_in){
		if (edit->default_cursor_shown){
			gdk_window_set_cursor (GTK_WIDGET(item->canvas)->window, text_view->i_cursor);
			edit->default_cursor_shown = FALSE;
		}
	}
#endif
	ect_queue_redraw (text_view, view_col, row);
	
	return NULL;
}

/*
 * ECellView::leave_edit method
 */
static void
ect_leave_edit (ECellView *ecell_view, int model_col, int view_col, int row, void *edit_context)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	CellEdit *edit = text_view->edit;

	if (edit){
		ect_stop_editing (text_view, TRUE);
	} else {
		/*
		 * We did invoke this leave edit internally
		 */
	}
}

/*
 * ECellView::save_state method
 */
static void *
ect_save_state (ECellView *ecell_view, int model_col, int view_col, int row, void *edit_context)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	CellEdit *edit = text_view->edit;

	int *save_state = g_new (int, 2);

	save_state[0] = edit->selection_start;
	save_state[1] = edit->selection_end;
	return save_state;
}

/*
 * ECellView::load_state method
 */
static void
ect_load_state (ECellView *ecell_view, int model_col, int view_col, int row, void *edit_context, void *save_state)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	CellEdit *edit = text_view->edit;
	int length;
	int *selection = save_state;

	length = strlen (edit->text);

	edit->selection_start = MIN (selection[0], length);
	edit->selection_end = MIN (selection[1], length);

	ect_queue_redraw (text_view, view_col, row);
}

/*
 * ECellView::free_state method
 */
static void
ect_free_state (ECellView *ecell_view, int model_col, int view_col, int row, void *save_state)
{
	g_free (save_state);
}

#define FONT_NAME "Sans Regular"

static GnomeFont *
get_font_for_size (double h)
{
	GnomeFontFace *face;
	GnomeFont *font;
	double asc, desc, size;

	face = gnome_font_face_find (FONT_NAME);

	asc = gnome_font_face_get_ascender (face);
	desc = abs (gnome_font_face_get_descender (face));
	size = h * 1000 / (asc + desc);

	font = gnome_font_find_closest (FONT_NAME, size);

	g_object_unref (face);
	return font;
}

static void
ect_print (ECellView *ecell_view, GnomePrintContext *context, 
	   int model_col, int view_col, int row,
	   double width, double height)
{
	GnomeFont *font = get_font_for_size (16);
	char *string;
	ECellText *ect = E_CELL_TEXT(ecell_view->ecell);
	double ty, ly, text_width;
	gboolean strikeout, underline;

	string = e_cell_text_get_text(ect, ecell_view->e_table_model, model_col, row);
	gnome_print_gsave(context);
	if (gnome_print_moveto(context, 2, 2) == -1)
				/* FIXME */;
	if (gnome_print_lineto(context, width - 2, 2) == -1)
				/* FIXME */;
	if (gnome_print_lineto(context, width - 2, height - 2) == -1)
				/* FIXME */;
	if (gnome_print_lineto(context, 2, height - 2) == -1)
				/* FIXME */;
	if (gnome_print_lineto(context, 2, 2) == -1)
				/* FIXME */;
	if (gnome_print_clip(context) == -1)
				/* FIXME */;

	ty = (height - gnome_font_get_ascender(font) - gnome_font_get_descender(font)) / 2;
	text_width = gnome_font_get_width_utf8 (font, string);

	strikeout = ect->strikeout_column >= 0 && row >= 0 &&
		e_table_model_value_at (ecell_view->e_table_model, ect->strikeout_column, row);
	underline = ect->underline_column >= 0 && row >= 0 &&
		e_table_model_value_at(ecell_view->e_table_model, ect->underline_column, row);

	if (underline) {
		ly = ty + gnome_font_get_underline_position (font);
		gnome_print_newpath (context);
		gnome_print_moveto (context, 2, ly);
		gnome_print_lineto (context, MIN (2 + text_width, width - 2), ly);
		gnome_print_setlinewidth (context, gnome_font_get_underline_thickness (font));
		gnome_print_stroke (context);
	}

	if (strikeout) {
		ly = ty + (gnome_font_get_ascender (font)  - gnome_font_get_underline_thickness (font))/ 2.0;
		gnome_print_newpath (context);
		gnome_print_moveto (context, 2, ly);
		gnome_print_lineto (context, MIN (2 + text_width, width - 2), ly);
		gnome_print_setlinewidth (context, gnome_font_get_underline_thickness (font));
		gnome_print_stroke (context);
	}

	gnome_print_moveto(context, 2, ty);
	gnome_print_setfont(context, font);
	gnome_print_show(context, string);
	gnome_print_grestore(context);
	e_cell_text_free_text(ect, string);
	g_object_unref (font);
}

static gdouble
ect_print_height (ECellView *ecell_view, GnomePrintContext *context, 
		  int model_col, int view_col, int row,
		  double width)
{
	return 16;
}

static int
ect_max_width (ECellView *ecell_view,
	       int model_col,
	       int view_col)
{
	/* New ECellText */
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	int row;
	int number_of_rows;
	int max_width = 0;

	number_of_rows = e_table_model_row_count (ecell_view->e_table_model);

	for (row = 0; row < number_of_rows; row++) {
		PangoLayout *layout = generate_layout (text_view, model_col, view_col, row, 0);
		int width;

		pango_layout_get_pixel_size (layout, &width, NULL);

		max_width = MAX (max_width, width);
		g_object_unref (layout);
	}
	
	return max_width + 8;
}

static int
ect_max_width_by_row (ECellView *ecell_view,
		      int model_col,
		      int view_col,
		      int row)
{
	/* New ECellText */
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	int width;
	PangoLayout *layout;

	if (row >= e_table_model_row_count (ecell_view->e_table_model))
		return 0;

	layout = generate_layout (text_view, model_col, view_col, row, 0);
	pango_layout_get_pixel_size (layout, &width, NULL);
	g_object_unref (layout);
	
	return width + 8;
}

static gint
tooltip_event (GtkWidget *window,
	       GdkEvent *event,
	       ETableTooltip *tooltip)
{
	gint ret_val = FALSE;
	
	switch (event->type) {
	case GDK_LEAVE_NOTIFY:
		e_canvas_hide_tooltip (E_CANVAS(GNOME_CANVAS_ITEM(tooltip->eti)->canvas));
		break;
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		if (event->type == GDK_BUTTON_RELEASE) {
			e_canvas_hide_tooltip (E_CANVAS(GNOME_CANVAS_ITEM(tooltip->eti)->canvas));
		}

		event->button.x = tooltip->cx;
		event->button.y = tooltip->cy;
		g_signal_emit_by_name (tooltip->eti, "event",
				       event, &ret_val);
		if (!ret_val)
			gtk_propagate_event (GTK_WIDGET(GNOME_CANVAS_ITEM(tooltip->eti)->canvas), event);
		ret_val = TRUE;
		break;
	case GDK_KEY_PRESS:
		e_canvas_hide_tooltip (E_CANVAS(GNOME_CANVAS_ITEM(tooltip->eti)->canvas));
		g_signal_emit_by_name (tooltip->eti, "event",
				       event, &ret_val);
		if (!ret_val)
			gtk_propagate_event (GTK_WIDGET(GNOME_CANVAS_ITEM(tooltip->eti)->canvas), event);
		ret_val = TRUE;
		break;
	default:
		break;
	}

	return ret_val;
}

static void
ect_show_tooltip (ECellView *ecell_view, 
		  int model_col,
		  int view_col,
		  int row,
		  int col_width,
		  ETableTooltip *tooltip)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	GtkWidget *canvas;
	double i2c[6];
	ArtPoint origin = {0, 0};
	ArtPoint pixel_origin;
	int canvas_x, canvas_y;
	GnomeCanvasItem *tooltip_text;
	double tooltip_width;
	double tooltip_height;
	double tooltip_x;
	double tooltip_y;
	GnomeCanvasItem *rect;
	ECellText *ect = E_CELL_TEXT(ecell_view->ecell);
	GtkWidget *window;
	PangoLayout *layout;
	int width, height;

	tooltip->timer = 0;

	layout = generate_layout (text_view, model_col, view_col, row, col_width);

	pango_layout_get_pixel_size (layout, &width, &height);
	if (width < col_width - 8) {
		return;
	}

	gnome_canvas_item_i2c_affine (GNOME_CANVAS_ITEM (tooltip->eti), i2c);
	art_affine_point (&pixel_origin, &origin, i2c);

	gdk_window_get_origin (GTK_WIDGET (text_view->canvas)->window,
			       &canvas_x, &canvas_y);
	pixel_origin.x += canvas_x;
	pixel_origin.y += canvas_y;
	pixel_origin.x -= (int) gtk_layout_get_hadjustment (GTK_LAYOUT (text_view->canvas))->value;
	pixel_origin.y -= (int) gtk_layout_get_vadjustment (GTK_LAYOUT (text_view->canvas))->value;

	window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_container_set_border_width (GTK_CONTAINER (window), 1);

	canvas = e_canvas_new ();
	gtk_container_add (GTK_CONTAINER (window), canvas);
	GTK_WIDGET_UNSET_FLAGS (canvas, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (window, GTK_CAN_FOCUS);

	rect = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas)),
				      gnome_canvas_rect_get_type (),
				      "x1", (double) 0.0,
				      "y1", (double) 0.0,
				      "x2", (double) width + 4,
				      "y2", (double) height,
				      "fill_color_gdk", tooltip->background,
				      NULL);

	tooltip_text = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas)),
					      e_text_get_type (),
					      "anchor", GTK_ANCHOR_NW,
					      "bold", (gboolean) ect->bold_column >= 0 && e_table_model_value_at(ecell_view->e_table_model, ect->bold_column, row),
					      "strikeout", (gboolean) ect->strikeout_column >= 0 && e_table_model_value_at(ecell_view->e_table_model, ect->strikeout_column, row),
					      "underline", (gboolean) ect->underline_column >= 0 && e_table_model_value_at(ecell_view->e_table_model, ect->underline_column, row),
					      "fill_color_gdk", tooltip->foreground,
					      "text", pango_layout_get_text (layout),
					      "editable", FALSE,
					      "clip_width", (double) width,
					      "clip_height", (double) height,
					      "clip", TRUE,
					      "line_wrap", FALSE,
  					      "justification", E_CELL_TEXT (text_view->cell_view.ecell)->justify,
					      "draw_background", FALSE,
					      NULL);

	tooltip_width = width;
	tooltip_height = height;
	tooltip_y = tooltip->y;

	switch (E_CELL_TEXT (text_view->cell_view.ecell)->justify) {
	case GTK_JUSTIFY_CENTER:
		tooltip_x = - tooltip_width / 2;
		break;
	case GTK_JUSTIFY_RIGHT:
		tooltip_x = tooltip_width / 2;
		break;
	case GTK_JUSTIFY_FILL:
	case GTK_JUSTIFY_LEFT:
		tooltip_x = tooltip->x;
		break;
	}

	gnome_canvas_item_move (tooltip_text, 3.0, 1.0);
	gnome_canvas_item_set (rect,
			       "x2", (double) tooltip_width + 6,
			       "y2", (double) tooltip->row_height + 1,
			       NULL);
	gtk_widget_set_usize (window, tooltip_width + 6,
			      tooltip->row_height + 1);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas), 0.0, 0.0,
					(double) tooltip_width + 6,
					(double) tooltip_height);
	gtk_widget_show (canvas);
	gtk_widget_realize (window);
	g_signal_connect (window, "event",
			  G_CALLBACK (tooltip_event), tooltip);

	e_canvas_popup_tooltip (E_CANVAS(text_view->canvas), window, pixel_origin.x + tooltip->x,
				pixel_origin.y + tooltip->y - 1);

	return;
}

/*
 * GtkObject::destroy method
 */
static void
ect_finalize (GObject *object)
{
	ECellText *ect = E_CELL_TEXT (object);

	g_free (ect->font_name);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}
/* Set_arg handler for the text item */
static void
ect_set_property (GObject *object,
		  guint prop_id,
		  const GValue *value,
		  GParamSpec *pspec)
{
	ECellText *text;

	text = E_CELL_TEXT (object);

	switch (prop_id) {
	case PROP_STRIKEOUT_COLUMN:
		text->strikeout_column = g_value_get_int (value);
		break;

	case PROP_UNDERLINE_COLUMN:
		text->underline_column = g_value_get_int (value);
		break;

	case PROP_BOLD_COLUMN:
		text->bold_column = g_value_get_int (value);
		break;

	case PROP_COLOR_COLUMN:
		text->color_column = g_value_get_int (value);
		break;

	case PROP_EDITABLE:
		text->editable = g_value_get_boolean (value);
		break;

	case PROP_BG_COLOR_COLUMN:
		text->bg_color_column = g_value_get_int (value);
		break;

	default:
		return;
	}
}

/* Get_arg handler for the text item */
static void
ect_get_property (GObject *object,
		  guint prop_id,
		  GValue *value,
		  GParamSpec *pspec)
{
	ECellText *text;

	text = E_CELL_TEXT (object);

	switch (prop_id) {
	case PROP_STRIKEOUT_COLUMN:
		g_value_set_int (value, text->strikeout_column);
		break;

	case PROP_UNDERLINE_COLUMN:
		g_value_set_int (value, text->underline_column);
		break;

	case PROP_BOLD_COLUMN:
		g_value_set_int (value, text->bold_column);
		break;

	case PROP_COLOR_COLUMN:
		g_value_set_int (value, text->color_column);
		break;

	case PROP_EDITABLE:
		g_value_set_boolean (value, text->editable);
		break;

	case PROP_BG_COLOR_COLUMN:
		g_value_set_int (value, text->bg_color_column);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static char *ellipsis_default = NULL;
static gboolean use_ellipsis_default = TRUE;

static void
e_cell_text_class_init (GObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;
	ECellTextClass *ectc = (ECellTextClass *) object_class;
	const char *ellipsis_env;

	G_OBJECT_CLASS (object_class)->finalize = ect_finalize;

	ecc->new_view   = ect_new_view;
	ecc->kill_view  = ect_kill_view;
	ecc->realize    = ect_realize;
	ecc->unrealize  = ect_unrealize;
	ecc->draw       = ect_draw;
	ecc->event      = ect_event;
	ecc->height     = ect_height;
	ecc->enter_edit = ect_enter_edit;
	ecc->leave_edit = ect_leave_edit;
	ecc->save_state = ect_save_state;
 	ecc->load_state = ect_load_state;
	ecc->free_state = ect_free_state;
	ecc->print      = ect_print;
	ecc->print_height = ect_print_height;
	ecc->max_width = ect_max_width;
	ecc->max_width_by_row = ect_max_width_by_row;
	ecc->show_tooltip = ect_show_tooltip;
	ecc->get_bg_color = ect_get_bg_color;

	ectc->get_text = ect_real_get_text;
	ectc->free_text = ect_real_free_text;
	ectc->set_value = ect_real_set_value;

	object_class->get_property = ect_get_property;
	object_class->set_property = ect_set_property;

	parent_class = g_type_class_ref (PARENT_TYPE);

	signals [TEXT_INSERTED] = 
		g_signal_new ("text_inserted",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ECellTextClass, text_inserted),
			      NULL, NULL,
			      e_marshal_VOID__POINTER_INT_INT_INT_INT,
			      G_TYPE_NONE, 5,
			      G_TYPE_POINTER, G_TYPE_INT, G_TYPE_INT,
			      G_TYPE_INT, G_TYPE_INT);

	signals [TEXT_DELETED] = 
		g_signal_new ("text_deleted",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ECellTextClass, text_deleted),
			      NULL, NULL,
			      e_marshal_VOID__POINTER_INT_INT_INT_INT,
			      G_TYPE_NONE, 5,
			      G_TYPE_POINTER, G_TYPE_INT, G_TYPE_INT,
			      G_TYPE_INT, G_TYPE_INT);



	g_object_class_install_property (object_class, PROP_STRIKEOUT_COLUMN,
					 g_param_spec_int ("strikeout_column",
							   _("Strikeout Column"),
							   /*_( */"XXX blurb" /*)*/,
							   -1, G_MAXINT, -1,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_UNDERLINE_COLUMN,
					 g_param_spec_int ("underline_column",
							   _("Underline Column"),
							   /*_( */"XXX blurb" /*)*/,
							   -1, G_MAXINT, -1,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_BOLD_COLUMN,
					 g_param_spec_int ("bold_column",
							   _("Bold Column"),
							   /*_( */"XXX blurb" /*)*/,
							   -1, G_MAXINT, -1,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_COLOR_COLUMN,
					 g_param_spec_int ("color_column",
							   _("Color Column"),
							   /*_( */"XXX blurb" /*)*/,
							   -1, G_MAXINT, -1,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_BG_COLOR_COLUMN,
					 g_param_spec_int ("bg_color_column",
							   _("BG Color Column"),
							   /*_( */"XXX blurb" /*)*/,
							   -1, G_MAXINT, -1,
							   G_PARAM_READWRITE));

	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

	ellipsis_env = g_getenv ("GAL_ELLIPSIS");
	if (ellipsis_env) {
		if (*ellipsis_env) {
			ellipsis_default = g_strdup (ellipsis_env);
		} else {
			use_ellipsis_default = FALSE;
		}
	}
	
	gal_a11y_e_cell_registry_add_cell_type (NULL, E_CELL_TEXT_TYPE, gal_a11y_e_cell_text_new);
}


/* IM Context Callbacks */

static void
e_cell_text_preedit_changed_cb (GtkIMContext *context,
		  ECellTextView    *tv)
{
	gchar *preedit_string;
	gint cursor_pos;
	CellEdit *edit=tv->edit;
	gtk_im_context_get_preedit_string (edit->im_context, &preedit_string, 
					NULL, &cursor_pos);
	                                                  
	edit->preedit_length = strlen (preedit_string);
	cursor_pos =  CLAMP (cursor_pos, 0, g_utf8_strlen (preedit_string, -1)); 
	g_free (preedit_string);
	ect_queue_redraw (tv, edit->view_col, edit->row);
}

static void
e_cell_text_commit_cb (GtkIMContext *context,
		  const gchar  *str,
		  ECellTextView    *tv)
{
	CellEdit *edit = tv->edit;
	ETextEventProcessorCommand command;
	                                                  
	if (g_utf8_validate (str, strlen (str), NULL)) {
		command.action = E_TEP_INSERT;
		command.position = E_TEP_SELECTION;
		command.string = (gchar *)str;
		command.value = strlen(str);
		e_cell_text_view_command (edit->tep, &command, edit);
	}

}

static gboolean
e_cell_text_retrieve_surrounding_cb (GtkIMContext *context,
				ECellTextView        *tv)
{
	int cur_pos = 0;
	CellEdit *edit = tv->edit;

	cur_pos = g_utf8_pointer_to_offset (edit->text, edit->text + edit->selection_start);

	gtk_im_context_set_surrounding (context,
					edit->text,
					strlen (edit->text),
					cur_pos
					);
	
	return TRUE;
}

static gboolean
e_cell_text_delete_surrounding_cb   (GtkIMContext *context,
				gint          offset,
				gint          n_chars,
				ECellTextView        *tv)
{
	CellEdit *edit = tv->edit;

	gtk_editable_delete_text (GTK_EDITABLE (edit),
				  edit->selection_end + offset,
				  edit->selection_end + offset + n_chars);

	return TRUE;
}

static void
e_cell_text_init (ECellText *ect)
{
	ect->ellipsis = g_strdup (ellipsis_default);
	ect->use_ellipsis = use_ellipsis_default;
	ect->strikeout_column = -1;
	ect->underline_column = -1;
	ect->bold_column = -1;
	ect->color_column = -1;
	ect->bg_color_column = -1;
	ect->editable = TRUE;
}

E_MAKE_TYPE(e_cell_text, "ECellText", ECellText, e_cell_text_class_init, e_cell_text_init, PARENT_TYPE)

/**
 * e_cell_text_construct:
 * @cell: The cell to construct
 * @fontname: this param is no longer used, but left here for api stability
 * @justify: Justification of the string in the cell
 *
 * constructs the ECellText.  To be used by subclasses and language
 * bindings.
 *
 * Returns: The ECellText.
 */
ECell *
e_cell_text_construct (ECellText *cell, const char *fontname, GtkJustification justify)
{
	if(!cell)
		return E_CELL(NULL);
	if(fontname)
		cell->font_name = g_strdup (fontname);
	cell->justify = justify;
	return E_CELL(cell);
}

/**
 * e_cell_text_new:
 * @fontname: this param is no longer used, but left here for api stability
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render strings that
 * that come from the model.  The value returned from the model is
 * interpreted as being a char *.
 *
 * The ECellText object support a large set of properties that can be
 * configured through the Gtk argument system and allows the user to have
 * a finer control of the way the string is displayed.  The arguments supported
 * allow the control of strikeout, underline, bold, and color.
 *
 * The arguments "strikeout_column", "underline_column", "bold_column"
 * and "color_column" set and return an integer that points to a
 * column in the model that controls these settings.  So controlling
 * the way things are rendered is achieved by having special columns
 * in the model that will be used to flag whether the text should be
 * rendered with strikeout, or bolded.  In the case of the
 * "color_column" argument, the column in the model is expected to
 * have a string that can be parsed by gdk_color_parse().
 * 
 * Returns: an ECell object that can be used to render strings.
 */
ECell *
e_cell_text_new (const char *fontname, GtkJustification justify)
{
	ECellText *ect = g_object_new (E_CELL_TEXT_TYPE, NULL);

	e_cell_text_construct(ect, fontname, justify);

	return (ECell *) ect;
}


/* fixme: Handle Font attributes */
/* position is in BYTES */

static gint
get_position_from_xy (CellEdit *edit, gint x, gint y)
{
	int index;
	int trailing;
	const char *text;

	PangoLayout *layout = generate_layout (edit->text_view, edit->model_col, edit->view_col, edit->row, edit->cell_width);
	ECellTextView *text_view = edit->text_view;
	ECellText *ect = (ECellText *) ((ECellView *)text_view)->ecell;

	x -= (ect->x + text_view->xofs - edit->xofs_edit);
	y -= (ect->y + text_view->yofs - edit->yofs_edit);

	pango_layout_xy_to_index (layout, x * PANGO_SCALE, y * PANGO_SCALE, &index, &trailing);

	text = pango_layout_get_text (layout);

	return g_utf8_offset_to_pointer (text + index, trailing) - text;
}

#define SCROLL_WAIT_TIME 30000

static gboolean
_blink_scroll_timeout (gpointer data)
{
	ECellTextView *text_view = (ECellTextView *) data;
	ECellText *ect = E_CELL_TEXT (((ECellView *)text_view)->ecell);
	CellEdit *edit = text_view->edit;

	gulong current_time;
	gboolean scroll = FALSE;
	gboolean redraw = FALSE;
	int width, height;
	
	g_timer_elapsed (edit->timer, &current_time);

	if (edit->scroll_start + SCROLL_WAIT_TIME > 1000000) {
		if (current_time > edit->scroll_start - (1000000 - SCROLL_WAIT_TIME) &&
		    current_time < edit->scroll_start)
			scroll = TRUE;
	} else {
		if (current_time > edit->scroll_start + SCROLL_WAIT_TIME ||
		    current_time < edit->scroll_start)
			scroll = TRUE;
	}

	pango_layout_get_pixel_size (edit->layout, &width, &height);

	if (scroll && edit->button_down) {
		/* FIXME: Copy this for y. */
		if (edit->lastx - ect->x > edit->cell_width) {
			if (edit->xofs_edit < width - edit->cell_width) {
				edit->xofs_edit += 4;
				if (edit->xofs_edit > width - edit->cell_width + 1)
					edit->xofs_edit = width - edit->cell_width + 1;
				redraw = TRUE;
			}
		}
		if (edit->lastx - ect->x < 0 &&
		    edit->xofs_edit > 0) {
			edit->xofs_edit -= 4;
			if (edit->xofs_edit < 0)
				edit->xofs_edit = 0;
			redraw = TRUE;
		}
		if (redraw) {
			ETextEventProcessorEvent e_tep_event;
			e_tep_event.type = GDK_MOTION_NOTIFY;
			e_tep_event.motion.state = edit->last_state;
			e_tep_event.motion.time = 0;
			e_tep_event.motion.position = get_position_from_xy (edit, edit->lastx, edit->lasty);
			_get_tep (edit);
			e_text_event_processor_handle_event (edit->tep,
							     &e_tep_event);
			edit->scroll_start = current_time;
		}
	}

	if (!((current_time / 500000) % 2)) {
		if (!edit->show_cursor)
			redraw = TRUE;
		edit->show_cursor = TRUE;
	} else {
		if (edit->show_cursor)
			redraw = TRUE;
		edit->show_cursor = FALSE;
	}
	if (redraw){
		ect_queue_redraw (text_view, edit->view_col, edit->row);
	}
	return TRUE;
}

static int
next_word (CellEdit *edit, int start)
{
	char *p;
	int length;

	length = strlen (edit->text);
	if (start >= length)
		return length;

	p = g_utf8_next_char (edit->text + start);

	while (*p && g_unichar_validate (g_utf8_get_char (p))) {
		gunichar unival = g_utf8_get_char (p);
		if (g_unichar_isspace (unival))
			return p - edit->text;
		p = g_utf8_next_char (p);
	}

	return p - edit->text;
}

static int
_get_position (ECellTextView *text_view, ETextEventProcessorCommand *command)
{
	int length;
	CellEdit *edit = text_view->edit;
	gchar *p;
	int unival;
	int index;
	int trailing;
	
	switch (command->position) {
		
	case E_TEP_VALUE:
		return command->value;

	case E_TEP_SELECTION:
		return edit->selection_end;

	case E_TEP_START_OF_BUFFER:
		return 0;

		/* fixme: this probably confuses TEP */

	case E_TEP_END_OF_BUFFER:
		return strlen (edit->text);

	case E_TEP_START_OF_LINE:

		if (edit->selection_end < 1) return 0;

		p = g_utf8_find_prev_char (edit->text, edit->text + edit->selection_end);

		if (p == edit->text) return 0;

		p = g_utf8_find_prev_char (edit->text, p);

		while (p && p > edit->text) {
			if (*p == '\n') return p - edit->text + 1;
			p = g_utf8_find_prev_char (edit->text, p);
		}

		return 0;

	case E_TEP_END_OF_LINE:

		length = strlen (edit->text);
		if (edit->selection_end >= length) return length;

		p = g_utf8_next_char (edit->text + edit->selection_end);

		while (*p && g_unichar_validate (g_utf8_get_char (p))) {
			if (*p == '\n') return p - edit->text;
			p = g_utf8_next_char (p);
		}

		return p - edit->text;

	case E_TEP_FORWARD_CHARACTER:

		length = strlen (edit->text);
		if (edit->selection_end >= length) return length;

		p = g_utf8_next_char (edit->text + edit->selection_end);

		return p - edit->text;

	case E_TEP_BACKWARD_CHARACTER:

		if (edit->selection_end < 1) return 0;

		p = g_utf8_find_prev_char (edit->text, edit->text + edit->selection_end);

		if (p == NULL) return 0;

		return p - edit->text;

	case E_TEP_FORWARD_WORD:
		return next_word (edit, edit->selection_end);

	case E_TEP_BACKWARD_WORD:

		if (edit->selection_end < 1) return 0;

		p = g_utf8_find_prev_char (edit->text, edit->text + edit->selection_end);

		if (p == edit->text) return 0;

		p = g_utf8_find_prev_char (edit->text, p);

		while (p && p > edit->text && g_unichar_validate (g_utf8_get_char (p))) {
			unival = g_utf8_get_char (p);
			if (g_unichar_isspace (unival)) {
				return (g_utf8_next_char (p) - edit->text);
			}
			p = g_utf8_find_prev_char (edit->text, p);
		}

		return 0;

	case E_TEP_FORWARD_LINE:
		pango_layout_move_cursor_visually (edit->layout,
						   TRUE,
						   edit->selection_end,
						   0,
						   TRUE,
						   &index,
						   &trailing);
		index = g_utf8_offset_to_pointer (edit->text + index, trailing) - edit->text;
		if (index < 0)
			return 0;
		length = strlen (edit->text);
		if (index >= length)
			return length;
		return index;
	case E_TEP_BACKWARD_LINE:
		pango_layout_move_cursor_visually (edit->layout,
						   TRUE,
						   edit->selection_end,
						   0,
						   TRUE,
						   &index,
						   &trailing);

		index = g_utf8_offset_to_pointer (edit->text + index, trailing) - edit->text;
		if (index < 0)
			return 0;
		length = strlen (edit->text);
		if (index >= length)
			return length;
		return index;
	case E_TEP_FORWARD_PARAGRAPH:
	case E_TEP_BACKWARD_PARAGRAPH:
		
	case E_TEP_FORWARD_PAGE:
	case E_TEP_BACKWARD_PAGE:
		return edit->selection_end;
	default:
		return edit->selection_end;
	}
	g_assert_not_reached ();
	return 0; /* Kill warning */
}

static void
_delete_selection (ECellTextView *text_view)
{
	CellEdit *edit = text_view->edit;
	gint length;
	gchar *sp, *ep;

	if (edit->selection_end == edit->selection_start) return;

	if (edit->selection_end < edit->selection_start) {
		edit->selection_end ^= edit->selection_start;
		edit->selection_start ^= edit->selection_end;
		edit->selection_end ^= edit->selection_start;
	}

	sp = edit->text + edit->selection_start;
	ep = edit->text + edit->selection_end;
	length = strlen (ep) + 1;

	memmove (sp, ep, length);

	edit->selection_end = edit->selection_start;

	g_signal_emit (VIEW_TO_CELL (text_view), signals[TEXT_DELETED], 0, text_view, edit->selection_start, ep-sp, edit->row, edit->model_col);
}

/* fixme: */
/* NB! We expect value to be length IN BYTES */

static void
_insert (ECellTextView *text_view, char *string, int value)
{
	CellEdit *edit = text_view->edit;
	char *temp;

	if (value <= 0) return;

	edit->selection_start = MIN (strlen(edit->text), edit->selection_start);

	temp = g_new (gchar, strlen (edit->text) + value + 1);

	strncpy (temp, edit->text, edit->selection_start);
	strncpy (temp + edit->selection_start, string, value);
	strcpy (temp + edit->selection_start + value, edit->text + edit->selection_end);

	g_free (edit->text);

	edit->text = temp;

	edit->selection_start += value;
	edit->selection_end = edit->selection_start;

	g_signal_emit (VIEW_TO_CELL (text_view), signals[TEXT_INSERTED], 0, text_view, edit->selection_end-value, value, edit->row, edit->model_col);
}

static void
capitalize (CellEdit *edit, int start, int end, ETextEventProcessorCaps type)
{
	ECellTextView *text_view = edit->text_view;

	gboolean first = TRUE;
	int character_length = g_utf8_strlen (edit->text + start, start - end);
	const char *p = edit->text + start;
	const char *text_end = edit->text + end;
	char *new_text = g_new0 (char, character_length * 6 + 1);
	char *output = new_text;

	while (p && *p && p < text_end && g_unichar_validate (g_utf8_get_char (p))) {
		gunichar unival = g_utf8_get_char (p);
		gunichar newval = unival;

		switch (type) {
		case E_TEP_CAPS_UPPER:
			newval = g_unichar_toupper (unival);
			break;
		case E_TEP_CAPS_LOWER:
			newval = g_unichar_tolower (unival);
			break;
		case E_TEP_CAPS_TITLE:
			if (g_unichar_isalpha (unival)) {
				if (first)
					newval = g_unichar_totitle (unival);
				else
					newval = g_unichar_tolower (unival);
				first = FALSE;
			} else {
				first = TRUE;
			}
			break;
		}
		g_unichar_to_utf8 (newval, output);
		output = g_utf8_next_char (output);

		p = g_utf8_next_char (p);
	}
	*output = 0;

	edit->selection_end = end;
	edit->selection_start = start;
	_delete_selection (text_view);

	_insert (text_view, new_text, output - new_text);

	g_free (new_text);
}

static void
e_cell_text_view_command (ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data)
{
	CellEdit *edit = (CellEdit *) data;
	ECellTextView *text_view = edit->text_view;
	ECellText *ect = E_CELL_TEXT (text_view->cell_view.ecell);

	gboolean change = FALSE;
	gboolean redraw = FALSE;

	int sel_start, sel_end;
	
	/* If the EText isn't editable, then ignore any commands that would
	   modify the text. */
	if (!ect->editable && (command->action == E_TEP_DELETE
			       || command->action == E_TEP_INSERT
			       || command->action == E_TEP_PASTE
			       || command->action == E_TEP_GET_SELECTION))
		return;

	switch (command->action) {
	case E_TEP_MOVE:
		edit->selection_start = _get_position (text_view, command);
		edit->selection_end = edit->selection_start;
		if (edit->timer) {
			g_timer_reset (edit->timer);
		}
		redraw = TRUE;
		break;
	case E_TEP_SELECT:
		edit->selection_end = _get_position (text_view, command);
		sel_start = MIN(edit->selection_start, edit->selection_end);
		sel_end = MAX(edit->selection_start, edit->selection_end);
		if (sel_start != sel_end) {
			e_cell_text_view_supply_selection (edit, command->time, GDK_SELECTION_PRIMARY,
							   edit->text + sel_start,
							   sel_end - sel_start);
		} else if (edit->timer) {
			g_timer_reset (edit->timer);
		}
		redraw = TRUE;
		break;
	case E_TEP_DELETE:
		if (edit->selection_end == edit->selection_start) {
			edit->selection_end = _get_position (text_view, command);
		}
		_delete_selection (text_view);
		if (edit->timer) {
			g_timer_reset (edit->timer);
		}
		redraw = TRUE;
		change = TRUE;
		break;

	case E_TEP_INSERT:
		if (!edit->preedit_length && edit->selection_end != edit->selection_start) {
			_delete_selection (text_view);
		}
		_insert (text_view, command->string, command->value);
		if (edit->timer) {
			g_timer_reset (edit->timer);
		}
		redraw = TRUE;
		change = TRUE;
		break;
	case E_TEP_COPY:
		sel_start = MIN(edit->selection_start, edit->selection_end);
		sel_end = MAX(edit->selection_start, edit->selection_end);
		if (sel_start != sel_end) {
			e_cell_text_view_supply_selection (edit, command->time, clipboard_atom,
							   edit->text + sel_start,
							   sel_end - sel_start);
		}
		if (edit->timer) {
			g_timer_reset (edit->timer);
		}
		break;
	case E_TEP_PASTE:
		e_cell_text_view_get_selection (edit, clipboard_atom, command->time);
		if (edit->timer) {
			g_timer_reset (edit->timer);
		}
		redraw = TRUE;
		change = TRUE;
		break;
	case E_TEP_GET_SELECTION:
		e_cell_text_view_get_selection (edit, GDK_SELECTION_PRIMARY, command->time);
		break;
	case E_TEP_ACTIVATE:
		e_table_item_leave_edit_ (text_view->cell_view.e_table_item_view);
		break;
	case E_TEP_SET_SELECT_BY_WORD:
		edit->select_by_word = command->value;
		break;
	case E_TEP_GRAB:
		edit->actions = E_CELL_GRAB;
		break;
	case E_TEP_UNGRAB:
		edit->actions = E_CELL_UNGRAB;
		break;
	case E_TEP_CAPS:
		if (edit->selection_start == edit->selection_end) {
			capitalize (edit, edit->selection_start, next_word (edit, edit->selection_start), command->value);
		} else {
			int selection_start = MIN (edit->selection_start, edit->selection_end);
			int selection_end = edit->selection_start + edit->selection_end - selection_start; /* Slightly faster than MAX */
			capitalize (edit, selection_start, selection_end, command->value);
		}
		if (edit->timer) {
			g_timer_reset (edit->timer);
		}
		redraw = TRUE;
		change = TRUE;
		break;
	case E_TEP_NOP:
		break;
	}

	if (change) {
		if (edit->layout)
			g_object_unref (edit->layout);
		edit->layout = build_layout (text_view, edit->row, edit->text, edit->cell_width);
	}

	if (!edit->button_down) {
		PangoRectangle strong_pos, weak_pos;
		pango_layout_get_cursor_pos (edit->layout, edit->selection_end, &strong_pos, &weak_pos);
		if (strong_pos.x != weak_pos.x ||
		    strong_pos.y != weak_pos.y ||
		    strong_pos.width != weak_pos.width ||
		    strong_pos.height != weak_pos.height) {
			if (show_pango_rectangle (edit, weak_pos))
				redraw = TRUE;
		}
		if (show_pango_rectangle (edit, strong_pos)) {
			redraw = TRUE;
		}
	}

	if (redraw){
		ect_queue_redraw (text_view, edit->view_col, edit->row);
	}
}

#ifdef DO_SELECTION
static void
_selection_clear_event (GtkInvisible *invisible,
			GdkEventSelection *event,
			CellEdit *edit)
{
	if (event->selection == GDK_SELECTION_PRIMARY) {
		g_free (edit->primary_selection);
		edit->primary_selection = NULL;
		edit->primary_length = 0;

		edit->has_selection = FALSE;
#if 0
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(text));
#endif

	} else if (event->selection == clipboard_atom) {
		g_free (edit->clipboard_selection);
		edit->clipboard_selection = NULL;
		edit->clipboard_length = 0;
	}
}

static void
_selection_get (GtkInvisible *invisible,
		GtkSelectionData *selection_data,
		guint info,
		guint time_stamp,
		CellEdit *edit)
{
	switch (info) {
	case E_SELECTION_PRIMARY:
		gtk_selection_data_set (selection_data, UTF8_ATOM,
					8, edit->primary_selection, 
					edit->primary_length);
		break;
	case E_SELECTION_CLIPBOARD:
		gtk_selection_data_set (selection_data, UTF8_ATOM,
					8, edit->clipboard_selection, 
					edit->clipboard_length);
		break;
	}
}

/* fixme: What happens, if delivered string is not UTF-8? */

static void
_selection_received (GtkInvisible *invisible,
		     GtkSelectionData *selection_data,
		     guint time,
		     CellEdit *edit)
{
	if (selection_data->length < 0 || 
			!(selection_data->type == UTF8_ATOM ||
			selection_data->type == GDK_SELECTION_TYPE_STRING)) {
		return;
	} else {
		ETextEventProcessorCommand command;
		command.action = E_TEP_INSERT;
		command.position = E_TEP_SELECTION;
		command.string = selection_data->data;
		command.value = selection_data->length;
		command.time = time;
		e_cell_text_view_command (edit->tep, &command, edit);
	}
}

static GtkWidget *e_cell_text_view_get_invisible (CellEdit *edit)
{
	if (edit->invisible == NULL) {
		GtkWidget *invisible = gtk_invisible_new ();
		edit->invisible = invisible;
		
		gtk_selection_add_target (invisible,
					  GDK_SELECTION_PRIMARY,
					  UTF8_ATOM,
					  E_SELECTION_PRIMARY);
		gtk_selection_add_target (invisible,
					  clipboard_atom,
					  UTF8_ATOM,
					  E_SELECTION_CLIPBOARD);
		
		g_signal_connect (invisible, "selection_get",
				  G_CALLBACK (_selection_get), 
				  edit);
		g_signal_connect (invisible, "selection_clear_event",
				  G_CALLBACK (_selection_clear_event),
				  edit);
		g_signal_connect (invisible, "selection_received",
				  G_CALLBACK (_selection_received),
				  edit);
	}
	return edit->invisible;
}
#endif

static void
e_cell_text_view_supply_selection (CellEdit *edit, guint time, GdkAtom selection, char *data, gint length)
{
#if DO_SELECTION
	gboolean successful;
	GtkWidget *invisible;

	invisible = e_cell_text_view_get_invisible (edit);

	if (selection == GDK_SELECTION_PRIMARY){
		if (edit->primary_selection) {
			g_free (edit->primary_selection);
		}
		edit->primary_selection = g_strndup (data, length);
		edit->primary_length = length;
	} else if (selection == clipboard_atom) {
		if (edit->clipboard_selection) {
			g_free (edit->clipboard_selection);
		}
		edit->clipboard_selection = g_strndup (data, length);
		edit->clipboard_length = length;
	}

	successful = gtk_selection_owner_set (invisible,
					      selection,
					      time);
	
	if (selection == GDK_SELECTION_PRIMARY)
		edit->has_selection = successful;
#endif
}

static void
e_cell_text_view_get_selection (CellEdit *edit, GdkAtom selection, guint32 time)
{
#if DO_SELECTION
	GtkWidget *invisible;
	invisible = e_cell_text_view_get_invisible (edit);
	gtk_selection_convert (invisible,
			      selection,
			      UTF8_ATOM,
			      time);
#endif
}

static void
_get_tep (CellEdit *edit)
{
	if (!edit->tep) {
		edit->tep = e_text_event_processor_emacs_like_new ();
		g_signal_connect (edit->tep,
				  "command",
				  G_CALLBACK(e_cell_text_view_command),
				  (gpointer) edit);
	}
}

static GdkColor*
e_cell_text_get_color (ECellTextView *cell_view, gchar *color_spec)
{
	ECellText *ect = E_CELL_TEXT (((ECellView*) cell_view)->ecell);
	GdkColormap *colormap;
	GdkColor *color, tmp_color;

	/* If the color spec is NULL we use the default color. */
	if (color_spec == NULL)
		return NULL;

	/* Create the hash table if we haven't already. */
	if (!ect->colors)
		ect->colors = g_hash_table_new (g_str_hash, g_str_equal);

	/* See if we've already allocated the color. Note that we use a
	   special value of (GdkColor*) 1 in the hash to indicate that we've
	   already tried and failed to allocate the color, so we don't keep
	   trying to allocate it. */
	color = g_hash_table_lookup (ect->colors, color_spec);
	if (color == (GdkColor*) 1)
		return NULL;
	if (color)
		return color;

	/* Try to parse the color. */
	if (gdk_color_parse (color_spec, &tmp_color)) {
		colormap = gtk_widget_get_colormap (GTK_WIDGET (cell_view->canvas));

		/* Try to allocate the color. */
		if (gdk_color_alloc (colormap, &tmp_color))
			color = gdk_color_copy (&tmp_color);
	}

	g_hash_table_insert (ect->colors, g_strdup (color_spec),
			     color ? color : (GdkColor*) 1);
	return color;
}

/**
 * e_cell_text_set_selection:
 * @cell_view: the given cell view
 * @col: column of the given cell in the view
 * @row: row of the given cell in the view
 * @start: start offset of the selection
 * @end: end offset of the selection
 *
 * Sets the selection of given text cell.
 * If the current editing cell is not the given cell, this function
 * will return FALSE;
 *
 * If success, the [start, end) part of the text will be selected.
 *
 * This API is most likely to be used by a11y implementations.
 * 
 * Returns: whether the action is successful.
 */
gboolean
e_cell_text_set_selection (ECellView *cell_view,
			   gint col,
			   gint row,
			   gint start,
			   gint end)
{
	ECellTextView *ectv;
	CellEdit *edit;
	ETextEventProcessorCommand command1, command2;

	ectv = (ECellTextView *)cell_view;
	edit = ectv->edit;
	if (!edit)
		return FALSE;

	if (edit->view_col != col || edit->row != row)
		return FALSE;

	command1.action = E_TEP_MOVE;
	command1.position = E_TEP_VALUE;
	command1.value = start;
	e_cell_text_view_command (edit->tep, &command1, edit);

	command2.action = E_TEP_SELECT;
	command2.position = E_TEP_VALUE;
	command2.value = end;
	e_cell_text_view_command (edit->tep, &command2, edit);

	return TRUE;
}

/**
 * e_cell_text_get_selection:
 * @cell_view: the given cell view
 * @col: column of the given cell in the view
 * @row: row of the given cell in the view
 * @start: a pointer to an int value indicates the start offset of the selection
 * @end: a pointer to an int value indicates the end offset of the selection
 *
 * Gets the selection of given text cell.
 * If the current editing cell is not the given cell, this function
 * will return FALSE;
 *
 * This API is most likely to be used by a11y implementations.
 * 
 * Returns: whether the action is successful.
 */
gboolean
e_cell_text_get_selection (ECellView *cell_view,
			   gint col,
			   gint row,
			   gint *start,
			   gint *end)
{
	ECellTextView *ectv;
	CellEdit *edit;

	ectv = (ECellTextView *)cell_view;
	edit = ectv->edit;
	if (!edit)
		return FALSE;

	if (edit->view_col != col || edit->row != row)
		return FALSE;

	if (start)
		*start = edit->selection_start;
	if (end)
		*end = edit->selection_end;
	return TRUE;
}

/**
 * e_cell_text_copy_clipboard:
 * @cell_view: the given cell view
 * @col: column of the given cell in the view
 * @row: row of the given cell in the view
 *
 * Copys the selected text to clipboard.
 *
 * This API is most likely to be used by a11y implementations.
 */
void
e_cell_text_copy_clipboard (ECellView *cell_view, gint col, gint row)
{
	ECellTextView *ectv;
	CellEdit *edit;
	ETextEventProcessorCommand command;

	ectv = (ECellTextView *)cell_view;
	edit = ectv->edit;
	if (!edit)
		return;

	if (edit->view_col != col || edit->row != row)
		return;

	command.action = E_TEP_COPY;
	command.time = GDK_CURRENT_TIME;
	e_cell_text_view_command (edit->tep, &command, edit);
}

/**
 * e_cell_text_paste_clipboard:
 * @cell_view: the given cell view
 * @col: column of the given cell in the view
 * @row: row of the given cell in the view
 *
 * Pastes the text from the clipboardt.
 *
 * This API is most likely to be used by a11y implementations.
 */
void
e_cell_text_paste_clipboard (ECellView *cell_view, gint col, gint row)
{
	ECellTextView *ectv;
	CellEdit *edit;
	ETextEventProcessorCommand command;

	ectv = (ECellTextView *)cell_view;
	edit = ectv->edit;
	if (!edit)
		return;

	if (edit->view_col != col || edit->row != row)
		return;

	command.action = E_TEP_PASTE;
	command.time = GDK_CURRENT_TIME;
	e_cell_text_view_command (edit->tep, &command, edit);
}

/**
 * e_cell_text_delete_selection:
 * @cell_view: the given cell view
 * @col: column of the given cell in the view
 * @row: row of the given cell in the view
 *
 * Deletes the selected text of the cell.
 *
 * This API is most likely to be used by a11y implementations.
 */
void
e_cell_text_delete_selection (ECellView *cell_view, gint col, gint row)
{
	ECellTextView *ectv;
	CellEdit *edit;
	ETextEventProcessorCommand command;

	ectv = (ECellTextView *)cell_view;
	edit = ectv->edit;
	if (!edit)
		return;

	if (edit->view_col != col || edit->row != row)
		return;

	command.action = E_TEP_DELETE;
	command.position = E_TEP_SELECTION;
	e_cell_text_view_command (edit->tep, &command, edit);
}

/**
 * e_cell_text_get_text_by_view:
 * @cell_view: the given cell view
 * @col: column of the given cell in the model
 * @row: row of the given cell in the model
 * 
 * Get the cell's text directly from CellEdit,
 * during editting this cell, the cell's text value maybe inconsistant
 * with the text got from table_model.
 * The caller should free the text after using it.
 *
 * This API is most likely to be used by a11y implementations.
 */
char *
e_cell_text_get_text_by_view (ECellView *cell_view, gint col, gint row)
{
	ECellTextView *ectv;
	CellEdit *edit;
	gchar	*ret, *model_text;

	ectv = (ECellTextView *)cell_view;
	edit = ectv->edit;
	
	if (edit && ectv->edit->row == row && ectv->edit->model_col == col) { /* being editted now */
		ret = g_strdup (edit->text);
	} else{
		model_text = e_cell_text_get_text (E_CELL_TEXT (cell_view->ecell), 
					     cell_view->e_table_model, col, row);
		ret = g_strdup (model_text);
		e_cell_text_free_text (E_CELL_TEXT (cell_view->ecell), model_text);
	}

	return ret;

}
