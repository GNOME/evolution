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
 *		Miguel de Icaza <miguel@ximian.com>
 *      Chris Lahey <clahey@ximian.com>
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <libgnomecanvas/libgnomecanvas.h>

#include "gal-a11y-e-cell-registry.h"
#include "gal-a11y-e-cell-text.h"
#include "e-canvas.h"
#include "e-cell-text.h"
#include "e-table-item.h"
#include "e-table.h"
#include "e-text-event-processor-emacs-like.h"
#include "e-text-event-processor.h"
#include "e-text.h"
#include "e-unicode.h"

#define d(x)
#define DO_SELECTION 1
#define VIEW_TO_CELL(view) E_CELL_TEXT (((ECellView *)view)->ecell)

#if d(!)0
#define e_table_item_leave_edit_(x) (e_table_item_leave_edit((x)), g_print ("%s: e_table_item_leave_edit\n", G_STRFUNC))
#else
#define e_table_item_leave_edit_(x) (e_table_item_leave_edit((x)))
#endif

/* This defines a line of text */
struct line {
	gchar *text;	/* Line's text UTF-8, it is a pointer into the text->text string */
	gint length;	/* Line's length in BYTES */
	gint width;	/* Line's width in pixels */
	gint ellipsis_length;  /* Length before adding ellipsis in BYTES */
};

/* Object argument IDs */
enum {
	PROP_0,

	PROP_STRIKEOUT_COLUMN,
	PROP_UNDERLINE_COLUMN,
	PROP_BOLD_COLUMN,
	PROP_COLOR_COLUMN,
	PROP_ITALIC_COLUMN,
	PROP_STRIKEOUT_COLOR_COLUMN,
	PROP_EDITABLE,
	PROP_BG_COLOR_COLUMN,
	PROP_USE_TABULAR_NUMBERS,
	PROP_IS_MARKUP,
	PROP_ELLIPSIZE_MODE
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

static guint signals[LAST_SIGNAL] = { 0 };

static GdkAtom clipboard_atom = GDK_NONE;

typedef struct _ECellTextPrivate {
	PangoEllipsizeMode ellipsize_mode;
} ECellTextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ECellText, e_cell_text, E_TYPE_CELL)

#define UTF8_ATOM  gdk_atom_intern ("UTF8_STRING", FALSE)

#define TEXT_PAD 4

typedef struct {
	gpointer lines;			/* Text split into lines (private field) */
	gint num_lines;			/* Number of lines of text */
	gint max_width;
	gint ref_count;
} ECellTextLineBreaks;

typedef struct _CellEdit CellEdit;

typedef struct {
	ECellView    cell_view;
	GdkCursor *i_cursor;

	GnomeCanvas *canvas;

	/*
	 * During editing.
	 */
	CellEdit    *edit;

	gint xofs, yofs;                 /* This gets added to the x
                                           and y for the cell text. */
	gdouble ellipsis_width[2];      /* The width of the ellipsis. */
} ECellTextView;

struct _CellEdit {

	ECellTextView *text_view;

	gint model_col, view_col, row;
	gint cell_width;

	PangoLayout *layout;

	gchar *text;

	gchar         *old_text;

	/*
	 * Where the editing is taking place
	 */

	gint xofs_edit, yofs_edit;       /* Offset because of editing.
                                           This is negative compared
                                           to the other offsets. */

	/* This needs to be reworked a bit once we get line wrapping. */
	gint selection_start;            /* Start of selection - IN BYTES */
	gint selection_end;              /* End of selection - IN BYTES */
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

	gboolean has_selection;         /* TRUE if we have the selection */

	guint pointer_in : 1;
	guint default_cursor_shown : 1;
	GtkIMContext *im_context;
	gboolean need_im_reset;
	gboolean im_context_signals_registered;

	guint16 preedit_length;       /* length of preedit string, in bytes */
	gint preedit_pos;             /* position of preedit cursor */

	ECellActions actions;
};

static void e_cell_text_view_command (ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data);

static void e_cell_text_view_get_selection (CellEdit *edit, GdkAtom selection, guint32 time);
static void e_cell_text_view_supply_selection (CellEdit *edit, guint time, GdkAtom selection, gchar *data, gint length);

static void _get_tep (CellEdit *edit);

static gint get_position_from_xy (CellEdit *edit, gint x, gint y);
static gboolean _blink_scroll_timeout (gpointer data);

static void e_cell_text_preedit_changed_cb (GtkIMContext *context, ECellTextView *text_view);
static void e_cell_text_commit_cb (GtkIMContext *context, const gchar  *str, ECellTextView *text_view);
static gboolean e_cell_text_retrieve_surrounding_cb (GtkIMContext *context, ECellTextView *text_view);
static gboolean e_cell_text_delete_surrounding_cb   (GtkIMContext *context, gint          offset, gint          n_chars, ECellTextView        *text_view);
static void _insert (ECellTextView *text_view, const gchar *string, gint value);
static void _delete_selection (ECellTextView *text_view);
static void update_im_cursor_location (ECellTextView *tv);

static gchar *
ect_real_get_text (ECellText *cell,
                   ETableModel *model,
                   gint col,
                   gint row)
{
	return e_table_model_value_at (model, col, row);
}

static void
ect_real_free_text (ECellText *cell,
		    ETableModel *model,
		    gint col,
                    gchar *text)
{
	e_table_model_free_value (model, col, text);
}

/* This is the default method for setting the ETableModel value based on
 * the text in the ECellText. This simply uses the text as it is - it assumes
 * the value in the model is a gchar *. Subclasses may parse the text into
 * data structures to pass to the model. */
static void
ect_real_set_value (ECellText *cell,
                    ETableModel *model,
                    gint col,
                    gint row,
                    const gchar *text)
{
	e_table_model_set_value_at (model, col, row, text);
}

static void
ect_queue_redraw (ECellTextView *text_view,
                  gint view_col,
                  gint view_row)
{
	e_table_item_redraw_range (
		text_view->cell_view.e_table_item_view,
		view_col, view_row, view_col, view_row);
}

/*
 * Shuts down the editing process
 */
static void
ect_stop_editing (ECellTextView *text_view,
                  gboolean commit)
{
	GdkWindow *window;
	CellEdit *edit = text_view->edit;
	gint row, view_col, model_col;
	gchar *old_text, *text;

	if (!edit)
		return;

	window = gtk_widget_get_window (GTK_WIDGET (text_view->canvas));

	row = edit->row;
	view_col = edit->view_col;
	model_col = edit->model_col;

	old_text = edit->old_text;
	text = edit->text;
	if (edit->tep)
		g_object_unref (edit->tep);
	if (!edit->default_cursor_shown) {
		gdk_window_set_cursor (window, NULL);
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

		if (g_strcmp0 (old_text, text)) {
			e_cell_text_set_value (
				ect, ecell_view->e_table_model,
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
ect_new_view (ECell *ecell,
              ETableModel *table_model,
              gpointer e_table_item_view)
{
	ECellTextView *text_view = g_new0 (ECellTextView, 1);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (e_table_item_view)->canvas;

	text_view->cell_view.ecell = ecell;
	text_view->cell_view.e_table_model = table_model;
	text_view->cell_view.e_table_item_view = e_table_item_view;
	text_view->cell_view.kill_view_cb = NULL;
	text_view->cell_view.kill_view_cb_data = NULL;

	text_view->canvas = canvas;

	text_view->xofs = 0.0;
	text_view->yofs = 0.0;

	return (ECellView *) text_view;
}

/*
 * ECell::kill_view method
 */
static void
ect_kill_view (ECellView *ecv)
{
	ECellTextView *text_view = (ECellTextView *) ecv;

	if (text_view->cell_view.kill_view_cb)
	    (text_view->cell_view.kill_view_cb)(ecv, text_view->cell_view.kill_view_cb_data);

	if (text_view->cell_view.kill_view_cb_data)
	    g_list_free (text_view->cell_view.kill_view_cb_data);

	g_free (text_view);
}

/*
 * ECell::realize method
 */
static void
ect_realize (ECellView *ecell_view)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;

	text_view->i_cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (((GnomeCanvasItem *) ecell_view->e_table_item_view)->canvas)), "text");

	if (E_CELL_CLASS (e_cell_text_parent_class)->realize)
		(* E_CELL_CLASS (e_cell_text_parent_class)->realize) (ecell_view);
}

/*
 * ECell::unrealize method
 */
static void
ect_unrealize (ECellView *ecv)
{
	ECellTextView *text_view = (ECellTextView *) ecv;

	if (text_view->edit) {
		ect_cancel_edit (text_view);
	}

	g_clear_object (&text_view->i_cursor);

	if (E_CELL_CLASS (e_cell_text_parent_class)->unrealize)
		(* E_CELL_CLASS (e_cell_text_parent_class)->unrealize) (ecv);

}

static PangoAttrList *
build_attr_list (ECellTextView *text_view,
                 gint row,
                 gint text_length,
		 GString **out_attrs_span)
{

	ECellView *ecell_view = (ECellView *) text_view;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	PangoAttrList *attrs = out_attrs_span ? NULL : pango_attr_list_new ();
	gboolean bold, strikeout, underline, italic;
	gint strikeout_color = 0;

	bold = ect->bold_column >= 0 &&
		row >= 0 &&
		e_table_model_value_at (ecell_view->e_table_model, ect->bold_column, row);
	strikeout = ect->strikeout_column >= 0 &&
		row >= 0 &&
		e_table_model_value_at (ecell_view->e_table_model, ect->strikeout_column, row);
	underline = ect->underline_column >= 0 &&
		row >= 0 &&
		e_table_model_value_at (ecell_view->e_table_model, ect->underline_column, row);
	italic = ect->italic_column >= 0 &&
		row >= 0 &&
		e_table_model_value_at (ecell_view->e_table_model, ect->italic_column, row);

	if (strikeout && ect->strikeout_color_column >= 0 && row >= 0)
		strikeout_color = GPOINTER_TO_UINT (e_table_model_value_at (ecell_view->e_table_model, ect->strikeout_color_column, row));

	#define ensure_attrs_span() { if (out_attrs_span && !*out_attrs_span) *out_attrs_span = g_string_new ("<span"); }

	if (bold) {
		if (out_attrs_span) {
			ensure_attrs_span ();
			g_string_append (*out_attrs_span, " weight='bold'");
		} else {
			PangoAttribute *attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
			attr->start_index = 0;
			attr->end_index = text_length;

			pango_attr_list_insert_before (attrs, attr);
		}
	}
	if (strikeout) {
		if (out_attrs_span) {
			ensure_attrs_span ();
			g_string_append (*out_attrs_span, " strikethrough='true'");
		} else {
			PangoAttribute *attr = pango_attr_strikethrough_new (TRUE);
			attr->start_index = 0;
			attr->end_index = text_length;

			pango_attr_list_insert_before (attrs, attr);
		}
	}
	if (underline) {
		if (out_attrs_span) {
			ensure_attrs_span ();
			g_string_append (*out_attrs_span, " underline='single'");
		} else {
			PangoAttribute *attr = pango_attr_underline_new (TRUE);
			attr->start_index = 0;
			attr->end_index = text_length;

			pango_attr_list_insert_before (attrs, attr);
		}
	}
	if (italic) {
		if (out_attrs_span) {
			ensure_attrs_span ();
			g_string_append (*out_attrs_span, " style='italic'");
		} else {
			PangoAttribute *attr = pango_attr_style_new (PANGO_STYLE_ITALIC);
			attr->start_index = 0;
			attr->end_index = text_length;

			pango_attr_list_insert_before (attrs, attr);
		}
	}
	if (strikeout_color) {
		if (out_attrs_span) {
			ensure_attrs_span ();
			g_string_append_printf (*out_attrs_span, " strikethrough_color='#%02x%02x%02x'",
				(strikeout_color >> 16) & 0xFF,
				(strikeout_color >> 8) & 0xFF,
				strikeout_color & 0xFF);
		} else {
			PangoAttribute *attr = pango_attr_strikethrough_color_new (
				((strikeout_color >> 16) & 0xFF) * 0xFF,
				((strikeout_color >> 8) & 0xFF) * 0xFF,
				(strikeout_color & 0xFF) * 0xFF);

			attr->start_index = 0;
			attr->end_index = text_length;

			pango_attr_list_insert_before (attrs, attr);
		}
	}
	if (ect->use_tabular_numbers) {
		if (out_attrs_span) {
			ensure_attrs_span ();
			g_string_append (*out_attrs_span, " font_features='tnum=1'");
		} else {
			PangoAttribute *attr = pango_attr_font_features_new ("tnum=1");

			pango_attr_list_insert_before (attrs, attr);
		}
	}

	#undef ensure_attrs_span

	/* close `<span ....` element */
	if (out_attrs_span && *out_attrs_span)
		g_string_append_c (*out_attrs_span, '>');

	return attrs;
}

static PangoLayout *
layout_with_preedit (ECellTextView *text_view,
                     gint row,
                     const gchar *text,
                     gint width)
{
	CellEdit *edit = text_view->edit;
	PangoAttrList *attrs;
	PangoLayout *layout;
	GString *tmp_string = g_string_new (NULL);
	PangoAttrList *preedit_attrs = NULL;
	gchar *preedit_string = NULL;
	gint preedit_length = 0;
	gint text_length = strlen (text);
	gint mlen = MIN (edit->selection_start,text_length);

	gtk_im_context_get_preedit_string (
		edit->im_context,
		&preedit_string,&preedit_attrs,
		NULL);
	preedit_length = edit->preedit_length = preedit_string ? strlen (preedit_string) : 0;

	layout = edit->layout;

	g_string_prepend_len (tmp_string, text,text_length);

	if (preedit_length) {

		/* mlen is the text_length in bytes, not chars
		 * check whether we are not inserting into
		 * the middle of a utf8 character
		 */

		if (mlen < text_length) {
			if (!g_utf8_validate (text + mlen, -1, NULL)) {
				gchar *tc;
				tc = g_utf8_find_next_char (text + mlen,NULL);
				if (tc) {
					mlen = (gint) (tc - text);
				}
			}
		}

		g_string_insert (tmp_string, mlen, preedit_string);
	}

	pango_layout_set_text (layout, tmp_string->str, tmp_string->len);

	attrs = build_attr_list (text_view, row, text_length, NULL);

	if (preedit_length)
		pango_attr_list_splice (attrs, preedit_attrs, mlen, preedit_length);
	pango_layout_set_attributes (layout, attrs);
	g_string_free (tmp_string, TRUE);
	g_free (preedit_string);
	if (preedit_attrs)
		pango_attr_list_unref (preedit_attrs);
	pango_attr_list_unref (attrs);

	update_im_cursor_location (text_view);

	return layout;
}

static PangoLayout *
build_layout (ECellTextView *text_view,
              gint row,
              const gchar *text,
              gint width)
{
	ECellView *ecell_view = (ECellView *) text_view;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	ECellTextPrivate *priv = e_cell_text_get_instance_private (ect);
	PangoLayout *layout;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (((GnomeCanvasItem *) ecell_view->e_table_item_view)->canvas), ect->is_markup ? NULL : text);

	if (ect->is_markup && text && *text) {
		GString *attrs_span = NULL;

		/* should return NULL, when building attrs as markup snap */
		g_warn_if_fail (!build_attr_list (text_view, row, text ? strlen (text) : 0, &attrs_span));

		if (attrs_span && attrs_span->len) {
			g_string_append (attrs_span, text);
			g_string_append (attrs_span, "</span>");

			pango_layout_set_markup (layout, attrs_span->str, attrs_span->len);
		} else {
			pango_layout_set_markup (layout, text, -1);
		}
		if (attrs_span)
			g_string_free (attrs_span, TRUE);
	} else {
		PangoAttrList *attrs;

		attrs = build_attr_list (text_view, row, text ? strlen (text) : 0, NULL);
		pango_layout_set_attributes (layout, attrs);
		pango_attr_list_unref (attrs);
	}

	if (text_view->edit || width <= 0)
		return layout;

	if (ect->font_name) {
		PangoContext *pango_context;
		PangoFontDescription *desc = NULL, *fixed_desc = NULL;
		gchar *fixed_family = NULL;
		gint fixed_size = 0;
		gboolean fixed_points = TRUE;

		fixed_desc = pango_font_description_from_string (ect->font_name);
		if (fixed_desc) {
			fixed_family = (gchar *) pango_font_description_get_family (fixed_desc);
			fixed_size = pango_font_description_get_size (fixed_desc);
			fixed_points = !pango_font_description_get_size_is_absolute (fixed_desc);
		}

		pango_context = gtk_widget_get_pango_context (GTK_WIDGET (((GnomeCanvasItem *) ecell_view->e_table_item_view)->canvas));
		desc = pango_font_description_copy (pango_context_get_font_description (pango_context));
		pango_font_description_set_family (desc, fixed_family);
		if (fixed_points)
			pango_font_description_set_size (desc, fixed_size);
		else
			pango_font_description_set_absolute_size (desc, fixed_size);
/*		pango_font_description_set_style (desc, PANGO_STYLE_OBLIQUE); */
		pango_layout_set_font_description (layout, desc);
		pango_font_description_free (desc);
		pango_font_description_free (fixed_desc);
	}

	pango_layout_set_width (layout, width * PANGO_SCALE);
	pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);

	pango_layout_set_ellipsize (layout, priv->ellipsize_mode);
	pango_layout_set_height (layout, 0);

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
generate_layout (ECellTextView *text_view,
                 gint model_col,
                 gint view_col,
                 gint row,
                 gint width)
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
		gchar *temp = e_cell_text_get_text (ect, ecell_view->e_table_model, model_col, row);
		layout = build_layout (text_view, row, temp ? temp : "", width);
		e_cell_text_free_text (ect, ecell_view->e_table_model, model_col, temp);
	} else
		layout = build_layout (text_view, row, "Mumbo Jumbo", width);

	return layout;
}

static void
draw_cursor (cairo_t *cr,
             gint x1,
             gint y1,
             PangoRectangle rect)
{
	gdouble scaled_x;
	gdouble scaled_y;
	gdouble scaled_height;

	/* Pango stores each cursor position as a zero-width rectangle. */
	scaled_x = x1 + ((gdouble) rect.x) / PANGO_SCALE;
	scaled_y = y1 + ((gdouble) rect.y) / PANGO_SCALE;
	scaled_height = ((gdouble) rect.height) / PANGO_SCALE;

	/* Adding 0.5 to scaled_x gives a sharp, one-pixel line. */
	cairo_move_to (cr, scaled_x + 0.5, scaled_y);
	cairo_line_to (cr, scaled_x + 0.5, scaled_y + scaled_height);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

static gboolean
show_pango_rectangle (CellEdit *edit,
                      PangoRectangle rect)
{
	gint x1 = rect.x / PANGO_SCALE;
	gint x2 = (rect.x + rect.width) / PANGO_SCALE;
#if 0
	gint y1 = rect.y / PANGO_SCALE;
	gint y2 = (rect.y + rect.height) / PANGO_SCALE;
#endif

	gint new_xofs_edit = edit->xofs_edit;
	gint new_yofs_edit = edit->yofs_edit;

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

static gint
get_vertical_spacing (GtkWidget *canvas)
{
	GtkWidget *widget;
	gint vspacing = 0;

	g_return_val_if_fail (E_IS_CANVAS (canvas), 3);

	/* The parent should be either an ETable or ETree. */
	widget = gtk_widget_get_parent (canvas);

	gtk_widget_style_get (widget, "vertical-spacing", &vspacing, NULL);

	return vspacing;
}

/*
 * ECell::draw method
 */
static void
ect_draw (ECellView *ecell_view,
          cairo_t *cr,
          gint model_col,
          gint view_col,
          gint row,
          ECellFlags flags,
          gint x1,
          gint y1,
          gint x2,
          gint y2)
{
	PangoLayout *layout;
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	CellEdit *edit = text_view->edit;
	gboolean color_overwritten = FALSE;
	gboolean selected;
	GtkWidget *canvas = GTK_WIDGET (text_view->canvas);
	GdkRGBA fg_rgba, bg_rgba, overwritten_rgba;
	gint x_origin, y_origin, vspacing;

	cairo_save (cr);

	selected = flags & E_CELL_SELECTED;

	e_utils_get_theme_color (canvas, "theme_text_color,theme_fg_color", E_UTILS_DEFAULT_THEME_TEXT_COLOR, &fg_rgba);
	gdk_cairo_set_source_rgba (cr, &fg_rgba);

	if (ect->color_column != -1) {
		gchar *color_spec;

		color_spec = e_table_model_value_at (
			ecell_view->e_table_model,
			ect->color_column, row);
		if (color_spec && gdk_rgba_parse (&overwritten_rgba, color_spec)) {
			if (selected) {
				fg_rgba = e_utils_get_text_color_for_background (&overwritten_rgba);
				gdk_cairo_set_source_rgba (cr, &fg_rgba);
			} else {
				gdk_cairo_set_source_rgba (cr, &overwritten_rgba);
			}
			color_overwritten = TRUE;
		}

		if (color_spec)
			e_table_model_free_value (ecell_view->e_table_model, ect->color_column, color_spec);
	}

	if (!color_overwritten && ect->bg_color_column != -1) {
		gchar *color_spec;

		/* if the background color is overwritten and the text color is not, then
		   pick either black or white text color, because the theme text color might
		   be hard to read on the overwritten background */
		color_spec = e_table_model_value_at (
			ecell_view->e_table_model,
			ect->bg_color_column, row);

		if (color_spec && gdk_rgba_parse (&bg_rgba, color_spec)) {
			color_overwritten = TRUE;
			bg_rgba = e_utils_get_text_color_for_background (&bg_rgba);
			gdk_cairo_set_source_rgba (cr, &bg_rgba);
		}

		if (color_spec)
			e_table_model_free_value (ecell_view->e_table_model, ect->bg_color_column, color_spec);
	}

	if (!color_overwritten && selected) {
		if (gtk_widget_has_focus (canvas))
			e_utils_get_theme_color (canvas, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &fg_rgba);
		else
			e_utils_get_theme_color (canvas, "theme_unfocused_selected_fg_color,theme_selected_fg_color", E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_FG_COLOR, &fg_rgba);
		gdk_cairo_set_source_rgba (cr, &fg_rgba);
	}

	vspacing = get_vertical_spacing (canvas);

	x1 += 4;
	y1 += vspacing;
	x2 -= 4;
	y2 -= vspacing;

	x_origin = x1 + ect->x + text_view->xofs - (edit ? edit->xofs_edit : 0);
	y_origin = y1 + ect->y + text_view->yofs - (edit ? edit->yofs_edit : 0);

	cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
	cairo_clip (cr);

	layout = generate_layout (text_view, model_col, view_col, row, x2 - x1);

	if (edit && edit->view_col == view_col && edit->row == row) {
		layout = layout_with_preedit  (text_view, row, edit->text ? edit->text : "",  x2 - x1);
	}

	cairo_move_to (cr, x_origin, y_origin);
	pango_cairo_show_layout (cr, layout);

	if (edit && edit->view_col == view_col && edit->row == row) {
		if (edit->selection_start != edit->selection_end) {
			cairo_region_t *clip_region;
			gint indices[2];

			if (edit->has_selection) {
				if (gtk_widget_has_focus (canvas)) {
					e_utils_get_theme_color (canvas, "theme_unfocused_selected_bg_color,theme_selected_bg_color", E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_BG_COLOR, &bg_rgba);
					e_utils_get_theme_color (canvas, "theme_unfocused_selected_fg_color,theme_selected_fg_color", E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_FG_COLOR, &fg_rgba);
				} else {
					e_utils_get_theme_color (canvas, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &bg_rgba);
					e_utils_get_theme_color (canvas, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &fg_rgba);
				}
			} else {
				e_utils_get_theme_color (canvas, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &bg_rgba);
				e_utils_get_theme_color (canvas, "theme_text_color,theme_fg_color", E_UTILS_DEFAULT_THEME_TEXT_COLOR, &fg_rgba);
			}

			indices[0] = MIN (edit->selection_start, edit->selection_end);
			indices[1] = MAX (edit->selection_start, edit->selection_end);

			clip_region = gdk_pango_layout_get_clip_region (
				layout, x_origin, y_origin, indices, 1);
			gdk_cairo_region (cr, clip_region);
			cairo_clip (cr);
			cairo_region_destroy (clip_region);

			gdk_cairo_set_source_rgba (cr, &bg_rgba);
			cairo_paint (cr);

			gdk_cairo_set_source_rgba (cr, &fg_rgba);
			cairo_move_to (cr, x_origin, y_origin);
			pango_cairo_show_layout (cr, layout);
		} else {
			if (edit->show_cursor) {
				PangoRectangle strong_pos, weak_pos;
				pango_layout_get_cursor_pos (layout, edit->selection_start + edit->preedit_length, &strong_pos, &weak_pos);

				draw_cursor (cr, x_origin, y_origin, strong_pos);
				if (strong_pos.x != weak_pos.x ||
				    strong_pos.y != weak_pos.y ||
				    strong_pos.width != weak_pos.width ||
				    strong_pos.height != weak_pos.height)
					draw_cursor (cr, x_origin, y_origin, weak_pos);
			}
		}
	}

	g_object_unref (layout);
	cairo_restore (cr);
}

/*
 * Get the background color
 */
static gchar *
ect_get_bg_color (ECellView *ecell_view,
                  gint row)
{
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	gchar *color_spec, *bg_color;

	if (ect->bg_color_column == -1)
		return NULL;

	color_spec = e_table_model_value_at (
		ecell_view->e_table_model,
		ect->bg_color_column, row);

	bg_color = g_strdup (color_spec);

	if (color_spec)
		e_table_model_free_value (ecell_view->e_table_model, ect->bg_color_column, color_spec);

	return bg_color;
}

/*
 * Selects the entire string
 */

static void
ect_edit_select_all (ECellTextView *text_view)
{
	g_return_if_fail (text_view->edit);

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
ect_event (ECellView *ecell_view,
           GdkEvent *event,
           gint model_col,
           gint view_col,
           gint row,
           ECellFlags flags,
           ECellActions *actions)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	ETextEventProcessorEvent e_tep_event;
	gboolean edit_display = FALSE;
	gint preedit_len;
	CellEdit *edit = text_view->edit;
	GtkWidget *canvas = GTK_WIDGET (text_view->canvas);
	gint return_val = 0;
	d (gboolean press = FALSE);

	if (!(flags & E_CELL_EDITING))
		return 0;

	if (edit && !edit->preedit_length && flags & E_CELL_PREEDIT)
		return 1;

	if (edit && edit->view_col == view_col && edit->row == row) {
		edit_display = TRUE;
	}

	e_tep_event.type = event->type;
	switch (event->type) {
	case GDK_FOCUS_CHANGE:
		break;
	case GDK_KEY_PRESS: /* Fall Through */
		if (edit_display) {
			edit->show_cursor = FALSE;
		} else {
			ect_stop_editing (text_view, TRUE);
		}
		return_val = TRUE;
		/* Fallthrough */
	case GDK_KEY_RELEASE:
		preedit_len = edit_display ? edit->preedit_length : 0;
		if (edit_display && edit->im_context &&
				gtk_im_context_filter_keypress (\
					edit->im_context,
					(GdkEventKey *) event)) {

			edit->need_im_reset = TRUE;
			if (preedit_len && flags & E_CELL_PREEDIT)
				return 0;
			else
				return 1;
		}

		if (event->key.keyval == GDK_KEY_Escape) {
			/* if not changed, then pass this even to parent */
			return_val = text_view->edit != NULL && text_view->edit->text && text_view->edit->old_text && 0 != strcmp (text_view->edit->text, text_view->edit->old_text);
			ect_cancel_edit (text_view);
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
			if (key.type == GDK_KEY_PRESS &&
			    (key.keyval == GDK_KEY_KP_Enter || key.keyval == GDK_KEY_Return)) {
				/* stop editing when it's only GDK_KEY_PRESS event */
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
					g_free ((gpointer) e_tep_event.key.string);
				break;
			}
		}

		break;
	case GDK_BUTTON_PRESS: /* Fall Through */
		d (press = TRUE);
	case GDK_BUTTON_RELEASE:
		d (g_print ("%s: %s\n", G_STRFUNC, press ? "GDK_BUTTON_PRESS" : "GDK_BUTTON_RELEASE"));
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
			e_tep_event.button.device =
				gdk_event_get_device (event);
			_get_tep (edit);
			edit->actions = 0;
			return_val = e_text_event_processor_handle_event (
				edit->tep, &e_tep_event);
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
			e_tep_event.button.device =
				gdk_event_get_device (event);
			_get_tep (edit);
			edit->actions = 0;
			return_val = e_text_event_processor_handle_event (
				edit->tep, &e_tep_event);
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
			return_val = e_text_event_processor_handle_event (
				edit->tep, &e_tep_event);
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
			if (edit->default_cursor_shown) {
				GdkWindow *window;

				window = gtk_widget_get_window (canvas);
				gdk_window_set_cursor (window, text_view->i_cursor);
				edit->default_cursor_shown = FALSE;
			}
		}
		break;
	case GDK_LEAVE_NOTIFY:
#if 0
		text_view->pointer_in = FALSE;
#endif
		if (edit_display) {
			if (!edit->default_cursor_shown) {
				GdkWindow *window;

				window = gtk_widget_get_window (canvas);
				gdk_window_set_cursor (window, NULL);
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
static gint
ect_height (ECellView *ecell_view,
            gint model_col,
            gint view_col,
            gint row)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	gint height;
	PangoLayout *layout;

	layout = generate_layout (text_view, model_col, view_col, row, 0);
	pango_layout_get_pixel_size (layout, NULL, &height);
	g_object_unref (layout);
	return height + (get_vertical_spacing (GTK_WIDGET (text_view->canvas)) * 2);
}

/*
 * ECellView::enter_edit method
 */
static gpointer
ect_enter_edit (ECellView *ecell_view,
                gint model_col,
                gint view_col,
                gint row)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	CellEdit *edit;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	gchar *temp;

	edit = g_new0 (CellEdit, 1);
	text_view->edit = edit;

	edit->im_context = E_CANVAS (text_view->canvas)->im_context;
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
		((ETableItem *) ecell_view->e_table_item_view)->header,
		view_col)->width - 8;

	edit->layout = generate_layout (text_view, model_col, view_col, row, edit->cell_width);

	edit->xofs_edit = 0.0;
	edit->yofs_edit = 0.0;

	edit->selection_start = 0;
	edit->selection_end = 0;
	edit->select_by_word = FALSE;

	edit->timeout_id = e_named_timeout_add (
		10, _blink_scroll_timeout, text_view);
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

	edit->pointer_in = FALSE;
	edit->default_cursor_shown = TRUE;

	temp = e_cell_text_get_text (ect, ecell_view->e_table_model, model_col, row);
	edit->old_text = g_strdup (temp ? temp : "");
	e_cell_text_free_text (ect, ecell_view->e_table_model, model_col, temp);
	edit->text = g_strdup (edit->old_text);

	if (edit->im_context) {
		gtk_im_context_reset (edit->im_context);
		if (!edit->im_context_signals_registered) {
			g_signal_connect (
				edit->im_context, "preedit_changed",
				G_CALLBACK (e_cell_text_preedit_changed_cb),
				text_view);
			g_signal_connect (
				edit->im_context, "commit",
				G_CALLBACK (e_cell_text_commit_cb),
				text_view);
			g_signal_connect (
				edit->im_context, "retrieve_surrounding",
				G_CALLBACK (e_cell_text_retrieve_surrounding_cb),
				text_view);
			g_signal_connect (
				edit->im_context, "delete_surrounding",
				G_CALLBACK (e_cell_text_delete_surrounding_cb),
				text_view);

			edit->im_context_signals_registered = TRUE;
		}
		gtk_im_context_focus_in (edit->im_context);
	}

#if 0
	if (edit->pointer_in) {
		if (edit->default_cursor_shown) {
			gdk_window_set_cursor (GTK_WIDGET (item->canvas)->window, text_view->i_cursor);
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
ect_leave_edit (ECellView *ecell_view,
                gint model_col,
                gint view_col,
                gint row,
                gpointer edit_context)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	CellEdit *edit = text_view->edit;

	if (edit) {
		if (edit->im_context) {
			gtk_im_context_focus_out (edit->im_context);

			if (edit->im_context_signals_registered) {
				g_signal_handlers_disconnect_matched (edit->im_context, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, edit);
				edit->im_context_signals_registered = FALSE;
			}
		}
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
static gpointer
ect_save_state (ECellView *ecell_view,
                gint model_col,
                gint view_col,
                gint row,
                gpointer edit_context)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	CellEdit *edit = text_view->edit;

	gint *save_state = g_new (int, 2);

	save_state[0] = edit->selection_start;
	save_state[1] = edit->selection_end;
	return save_state;
}

/*
 * ECellView::load_state method
 */
static void
ect_load_state (ECellView *ecell_view,
                gint model_col,
                gint view_col,
                gint row,
                gpointer edit_context,
                gpointer save_state)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	CellEdit *edit = text_view->edit;
	gint length;
	gint *selection = save_state;

	length = strlen (edit->text);

	edit->selection_start = MIN (selection[0], length);
	edit->selection_end = MIN (selection[1], length);

	ect_queue_redraw (text_view, view_col, row);
}

/*
 * ECellView::free_state method
 */
static void
ect_free_state (ECellView *ecell_view,
                gint model_col,
                gint view_col,
                gint row,
                gpointer save_state)
{
	g_free (save_state);
}

static void
get_font_size (PangoLayout *layout,
               PangoFontDescription *font,
               const gchar *text,
               gdouble *width,
               gdouble *height)
{
	gint w;
	gint h;

	g_return_if_fail (layout != NULL);
	pango_layout_set_font_description (layout, font);
	pango_layout_set_text (layout, text, -1);
	pango_layout_set_width (layout, -1);
	pango_layout_set_indent (layout, 0);

	pango_layout_get_size (layout, &w, &h);

	*width = (gdouble)w/(gdouble)PANGO_SCALE;
	*height = (gdouble)h/(gdouble)PANGO_SCALE;
}

static void
ect_print (ECellView *ecell_view,
           GtkPrintContext *context,
           gint model_col,
           gint view_col,
           gint row,
           gdouble width,
           gdouble height)
{
	PangoFontDescription *font_des;
	PangoLayout *layout;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	ECellTextView *ectView = (ECellTextView *) ecell_view;
	GtkWidget *canvas = GTK_WIDGET (ectView->canvas);
	PangoDirection dir;
	gboolean strikeout, underline;
	cairo_t *cr;
	gchar *string;
	gdouble ty, ly, text_width = 0.0, text_height = 0.0;

	cr = gtk_print_context_get_cairo_context (context);
	string = e_cell_text_get_text (ect, ecell_view->e_table_model, model_col, row);

	cairo_save (cr);
	layout = gtk_print_context_create_pango_layout (context);
	font_des = pango_font_description_from_string ("sans 10"); /* fix me font hardcoded */
	pango_layout_set_font_description (layout, font_des);

	pango_layout_set_text (layout, string, -1);
	get_font_size (layout, font_des, string, &text_width, &text_height);

	cairo_move_to (cr, 2, 2);
	cairo_rectangle (cr, 2, 2, width + 2, height + 2);
	cairo_clip (cr);

	pango_context = gtk_widget_get_pango_context (canvas);
	font_metrics = pango_context_get_metrics (
		pango_context, NULL,
		pango_context_get_language (pango_context));
	ty = (gdouble)(text_height -
		pango_font_metrics_get_ascent (font_metrics) -
		pango_font_metrics_get_descent (font_metrics)) / 2.0 /(gdouble) PANGO_SCALE;

	strikeout = ect->strikeout_column >= 0 && row >= 0 &&
		e_table_model_value_at (ecell_view->e_table_model, ect->strikeout_column, row);
	underline = ect->underline_column >= 0 && row >= 0 &&
		e_table_model_value_at (ecell_view->e_table_model, ect->underline_column, row);

	dir = pango_find_base_dir (string, strlen (string));

	if (underline) {
		ly = ty + (gdouble) pango_font_metrics_get_underline_position (font_metrics) / (gdouble) PANGO_SCALE;
		cairo_new_path (cr);
		if (dir == PANGO_DIRECTION_RTL) {
			cairo_move_to (cr, width - 2, ly + text_height + 6);
			cairo_line_to (cr, MAX (width - 2 - text_width, 2), ly + text_height + 6);
		}
		else {
			cairo_move_to (cr, 2, ly + text_height + 6);
			cairo_line_to (cr, MIN (2 + text_width, width - 2), ly + text_height + 6);
		}
		cairo_set_line_width (cr, (gdouble) pango_font_metrics_get_underline_thickness (font_metrics) / (gdouble) PANGO_SCALE);
		cairo_stroke (cr);
	}

	if (strikeout) {
		ly = ty + (gdouble) pango_font_metrics_get_strikethrough_position (font_metrics) / (gdouble) PANGO_SCALE;
		cairo_new_path (cr);
		if (dir == PANGO_DIRECTION_RTL) {
			cairo_move_to (cr, width - 2, ly + text_height + 6);
			cairo_line_to (cr, MAX (width - 2 - text_width, 2), ly + text_height + 6);
		}
		else {
			cairo_move_to (cr, 2, ly + text_height + 6);
			cairo_line_to (cr, MIN (2 + text_width, width - 2), ly + text_height + 6);
		}
			cairo_set_line_width (cr,(gdouble) pango_font_metrics_get_strikethrough_thickness (font_metrics) / (gdouble) PANGO_SCALE);

			cairo_stroke (cr);
	}

	cairo_move_to (cr, 2, text_height- 5);
	pango_layout_set_width (layout, (width - 4) * PANGO_SCALE);
	pango_layout_set_wrap (layout, PANGO_WRAP_CHAR);
	pango_cairo_show_layout (cr, layout);
	cairo_restore (cr);

	pango_font_description_free (font_des);
	g_object_unref (layout);
	e_cell_text_free_text (ect, ecell_view->e_table_model, model_col, string);
}

static gdouble
ect_print_height (ECellView *ecell_view,
                  GtkPrintContext *context,
                  gint model_col,
                  gint view_col,
                  gint row,
                  gdouble width)
{
	/*
	 * Font size is 16 by default. To leave some margin for cell
	 * text area, 2 for footer, 2 for header, actual print height
	 * should be 16 + 4.
	 * Height of some special font is much higher than others,
	 * such	as Arabic. So leave some more margin for cell.
	 */
	PangoFontDescription *font_des;
	PangoLayout *layout;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	gchar *string;
	gdouble text_width = 0.0, text_height = 0.0;
	gint lines = 1;

	string = e_cell_text_get_text (ect, ecell_view->e_table_model, model_col, row);

	layout = gtk_print_context_create_pango_layout (context);
	font_des = pango_font_description_from_string ("sans 10"); /* fix me font hardcoded */
	pango_layout_set_font_description (layout, font_des);

	pango_layout_set_text (layout, string, -1);
	get_font_size (layout, font_des, string, &text_width, &text_height);
	/* Checking if the text width goes beyond the column width to increase the
	 * number of lines.
	 */
	if (text_width > width - 4)
		lines = (text_width / (width - 4)) + 1;
	return 16 *lines + 8;
}

static gint
ect_max_width (ECellView *ecell_view,
               gint model_col,
               gint view_col)
{
	/* New ECellText */
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	gint row;
	gint number_of_rows;
	gint max_width = 0;

	number_of_rows = e_table_model_row_count (ecell_view->e_table_model);

	for (row = 0; row < number_of_rows; row++) {
		PangoLayout *layout = generate_layout (text_view, model_col, view_col, row, 0);
		gint width;

		pango_layout_get_pixel_size (layout, &width, NULL);

		max_width = MAX (max_width, width);
		g_object_unref (layout);
	}

	return max_width + 8;
}

static gint
ect_max_width_by_row (ECellView *ecell_view,
                      gint model_col,
                      gint view_col,
                      gint row)
{
	/* New ECellText */
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	gint width;
	PangoLayout *layout;

	if (row >= e_table_model_row_count (ecell_view->e_table_model))
		return 0;

	layout = generate_layout (text_view, model_col, view_col, row, 0);
	pango_layout_get_pixel_size (layout, &width, NULL);
	g_object_unref (layout);

	return width + 8;
}

static void
ect_finalize (GObject *object)
{
	ECellText *ect = E_CELL_TEXT (object);

	g_free (ect->font_name);

	G_OBJECT_CLASS (e_cell_text_parent_class)->finalize (object);
}

/* Set_arg handler for the text item */
static void
ect_set_property (GObject *object,
                  guint property_id,
                  const GValue *value,
                  GParamSpec *pspec)
{
	ECellText *text;

	text = E_CELL_TEXT (object);

	switch (property_id) {
	case PROP_STRIKEOUT_COLUMN:
		text->strikeout_column = g_value_get_int (value);
		break;

	case PROP_UNDERLINE_COLUMN:
		text->underline_column = g_value_get_int (value);
		break;

	case PROP_BOLD_COLUMN:
		text->bold_column = g_value_get_int (value);
		break;

	case PROP_ITALIC_COLUMN:
		text->italic_column = g_value_get_int (value);
		break;

	case PROP_STRIKEOUT_COLOR_COLUMN:
		text->strikeout_color_column = g_value_get_int (value);
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

	case PROP_USE_TABULAR_NUMBERS:
		text->use_tabular_numbers = g_value_get_boolean (value);
		break;

	case PROP_IS_MARKUP:
		text->is_markup = g_value_get_boolean (value);
		break;

	case PROP_ELLIPSIZE_MODE:
		e_cell_text_set_ellipsize_mode (text, g_value_get_enum (value));
		break;

	default:
		return;
	}
}

/* Get_arg handler for the text item */
static void
ect_get_property (GObject *object,
                  guint property_id,
                  GValue *value,
                  GParamSpec *pspec)
{
	ECellText *text;

	text = E_CELL_TEXT (object);

	switch (property_id) {
	case PROP_STRIKEOUT_COLUMN:
		g_value_set_int (value, text->strikeout_column);
		break;

	case PROP_UNDERLINE_COLUMN:
		g_value_set_int (value, text->underline_column);
		break;

	case PROP_BOLD_COLUMN:
		g_value_set_int (value, text->bold_column);
		break;

	case PROP_ITALIC_COLUMN:
		g_value_set_int (value, text->italic_column);
		break;

	case PROP_STRIKEOUT_COLOR_COLUMN:
		g_value_set_int (value, text->strikeout_color_column);
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

	case PROP_USE_TABULAR_NUMBERS:
		g_value_set_boolean (value, text->use_tabular_numbers);
		break;

	case PROP_IS_MARKUP:
		g_value_set_boolean (value, text->is_markup);
		break;

	case PROP_ELLIPSIZE_MODE:
		g_value_set_enum (value, e_cell_text_get_ellipsize_mode (text));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static gchar *ellipsis_default = NULL;
static gboolean use_ellipsis_default = TRUE;

static void
e_cell_text_class_init (ECellTextClass *class)
{
	ECellClass *ecc = E_CELL_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	const gchar *ellipsis_env;

	object_class->finalize = ect_finalize;

	ecc->new_view = ect_new_view;
	ecc->kill_view = ect_kill_view;
	ecc->realize = ect_realize;
	ecc->unrealize = ect_unrealize;
	ecc->draw = ect_draw;
	ecc->event = ect_event;
	ecc->height = ect_height;
	ecc->enter_edit = ect_enter_edit;
	ecc->leave_edit = ect_leave_edit;
	ecc->save_state = ect_save_state;
	ecc->load_state = ect_load_state;
	ecc->free_state = ect_free_state;
	ecc->print = ect_print;
	ecc->print_height = ect_print_height;
	ecc->max_width = ect_max_width;
	ecc->max_width_by_row = ect_max_width_by_row;
	ecc->get_bg_color = ect_get_bg_color;

	class->get_text = ect_real_get_text;
	class->free_text = ect_real_free_text;
	class->set_value = ect_real_set_value;

	object_class->get_property = ect_get_property;
	object_class->set_property = ect_set_property;

	signals[TEXT_INSERTED] = g_signal_new (
		"text_inserted",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECellTextClass, text_inserted),
		NULL, NULL,
		e_marshal_VOID__POINTER_INT_INT_INT_INT,
		G_TYPE_NONE, 5,
		G_TYPE_POINTER,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_INT);

	signals[TEXT_DELETED] = g_signal_new (
		"text_deleted",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECellTextClass, text_deleted),
		NULL, NULL,
		e_marshal_VOID__POINTER_INT_INT_INT_INT,
		G_TYPE_NONE, 5,
		G_TYPE_POINTER,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_INT);

	g_object_class_install_property (
		object_class,
		PROP_STRIKEOUT_COLUMN,
		g_param_spec_int (
			"strikeout_column",
			"Strikeout Column",
			NULL,
			-1, G_MAXINT, -1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_UNDERLINE_COLUMN,
		g_param_spec_int (
			"underline_column",
			"Underline Column",
			NULL,
			-1, G_MAXINT, -1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_BOLD_COLUMN,
		g_param_spec_int (
			"bold_column",
			"Bold Column",
			NULL,
			-1, G_MAXINT, -1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ITALIC_COLUMN,
		g_param_spec_int (
			"italic-column",
			"Italic Column",
			NULL,
			-1, G_MAXINT, -1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_STRIKEOUT_COLOR_COLUMN,
		g_param_spec_int (
			"strikeout-color-column",
			"Strikeout Color Column",
			NULL,
			-1, G_MAXINT, -1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_COLOR_COLUMN,
		g_param_spec_int (
			"color_column",
			"Color Column",
			NULL,
			-1, G_MAXINT, -1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_BG_COLOR_COLUMN,
		g_param_spec_int (
			"bg_color_column",
			"BG Color Column",
			NULL,
			-1, G_MAXINT, -1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_USE_TABULAR_NUMBERS,
		g_param_spec_boolean (
			"use-tabular-numbers",
			"Use tabular numbers",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_IS_MARKUP,
		g_param_spec_boolean (
			"is-markup",
			"The text is markup",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ELLIPSIZE_MODE,
		g_param_spec_enum (
			"ellipsize-mode", NULL, NULL,
			PANGO_TYPE_ELLIPSIZE_MODE,
			PANGO_ELLIPSIZE_END,
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

	gal_a11y_e_cell_registry_add_cell_type (NULL, E_TYPE_CELL_TEXT, gal_a11y_e_cell_text_new);
}

/* IM Context Callbacks */

static void
e_cell_text_get_cursor_locations (ECellTextView *tv,
                                  GdkRectangle *strong_pos,
                                  GdkRectangle *weak_pos)
{
	GdkRectangle area;
	CellEdit *edit = tv->edit;
	ECellView *cell_view = (ECellView *) tv;
	ETableItem *item = E_TABLE_ITEM ((cell_view)->e_table_item_view);
	GnomeCanvasItem *parent_item = GNOME_CANVAS_ITEM (item)->parent;
	PangoRectangle pango_strong_pos;
	PangoRectangle pango_weak_pos;
	gint x, y, col, row;
	gdouble x1,y1;
	gint cx, cy;
	gint index;

	row = edit->row;
	col = edit->view_col;

	e_table_item_get_cell_geometry (
		item, &row, &col, &x, &y, NULL, &area.height);

	gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (parent_item), &x1, &y1, NULL, NULL);

	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (GNOME_CANVAS_ITEM (parent_item)->canvas), &cx, &cy);

	index = edit->selection_end + edit->preedit_pos;

	pango_layout_get_cursor_pos (
		edit->layout,
		index,
		strong_pos ? &pango_strong_pos : NULL,
		weak_pos ? &pango_weak_pos : NULL);

	if (strong_pos) {
		strong_pos->x = x + x1 - cx - edit->xofs_edit + pango_strong_pos.x / PANGO_SCALE;
		strong_pos->y = y + y1 - cy - edit->yofs_edit + pango_strong_pos.y / PANGO_SCALE;
		strong_pos->width = 0;
		strong_pos->height = pango_strong_pos.height / PANGO_SCALE;
	}

	if (weak_pos) {
		weak_pos->x = x + x1 - cx - edit->xofs_edit + pango_weak_pos.x / PANGO_SCALE;
		weak_pos->y = y + y1 - cy - edit->yofs_edit + pango_weak_pos.y / PANGO_SCALE;
		weak_pos->width = 0;
		weak_pos->height = pango_weak_pos.height / PANGO_SCALE;
	}
}

static void
update_im_cursor_location (ECellTextView *tv)
{
	CellEdit *edit = tv->edit;
	GdkRectangle area;

	e_cell_text_get_cursor_locations (tv, &area, NULL);

	gtk_im_context_set_cursor_location (edit->im_context, &area);
}

static void
e_cell_text_preedit_changed_cb (GtkIMContext *context,
                                ECellTextView *tv)
{
	gchar *preedit_string;
	gint cursor_pos;
	CellEdit *edit = tv->edit;
	gtk_im_context_get_preedit_string (
		edit->im_context, &preedit_string,
		NULL, &cursor_pos);

	edit->preedit_length = strlen (preedit_string);
	cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (preedit_string, -1));
	edit->preedit_pos = g_utf8_offset_to_pointer (preedit_string, cursor_pos) - preedit_string;
	g_free (preedit_string);

	ect_queue_redraw (tv, edit->view_col, edit->row);
}

static void
e_cell_text_commit_cb (GtkIMContext *context,
                       const gchar *str,
                       ECellTextView *tv)
{
	CellEdit *edit = tv->edit;
	ETextEventProcessorCommand command = { 0 };

	if (g_utf8_validate (str, strlen (str), NULL)) {
		command.action = E_TEP_INSERT;
		command.position = E_TEP_SELECTION;
		command.string = (gchar *) str;
		command.value = strlen (str);
		e_cell_text_view_command (edit->tep, &command, edit);
	}

}

static gboolean
e_cell_text_retrieve_surrounding_cb (GtkIMContext *context,
                                     ECellTextView *tv)
{
	CellEdit *edit = tv->edit;

	gtk_im_context_set_surrounding (
		context,
		edit->text,
		strlen (edit->text),
		MIN (edit->selection_start, edit->selection_end));

	return TRUE;
}

static gboolean
e_cell_text_delete_surrounding_cb (GtkIMContext *context,
                                   gint offset,
                                   gint n_chars,
                                   ECellTextView *tv)
{
	gint begin_pos, end_pos;
	glong text_len;
	CellEdit *edit = tv->edit;

	text_len = g_utf8_strlen (edit->text, -1);
	begin_pos = g_utf8_pointer_to_offset (
		edit->text,
		edit->text + MIN (edit->selection_start, edit->selection_end));
	begin_pos += offset;
	end_pos = begin_pos + n_chars;
	if (begin_pos < 0 || text_len < begin_pos)
		return FALSE;
	if (end_pos > text_len)
		end_pos = text_len;
	edit->selection_start = g_utf8_offset_to_pointer (edit->text, begin_pos)
				- edit->text;
	edit->selection_end = g_utf8_offset_to_pointer (edit->text, end_pos)
			      - edit->text;

	_delete_selection (tv);

	return TRUE;
}

static void
e_cell_text_init (ECellText *ect)
{
	ECellTextPrivate *priv = e_cell_text_get_instance_private (ect);

	priv->ellipsize_mode = PANGO_ELLIPSIZE_END;

	ect->ellipsis = g_strdup (ellipsis_default);
	ect->use_ellipsis = use_ellipsis_default;
	ect->strikeout_column = -1;
	ect->underline_column = -1;
	ect->bold_column = -1;
	ect->italic_column = -1;
	ect->strikeout_color_column = -1;
	ect->color_column = -1;
	ect->bg_color_column = -1;
	ect->editable = TRUE;
	ect->use_tabular_numbers = FALSE;
}

/**
 * e_cell_text_new:
 * @fontname: this param is no longer used, but left here for api stability
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render strings that
 * that come from the model.  The value returned from the model is
 * interpreted as being a gchar *.
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
 * have a string that can be parsed by gdk_rgba_parse().
 *
 * Returns: an ECell object that can be used to render strings.
 */
ECell *
e_cell_text_new (const gchar *fontname,
                 GtkJustification justify)
{
	ECellText *ect = g_object_new (E_TYPE_CELL_TEXT, NULL);

	e_cell_text_construct (ect, fontname, justify);

	return (ECell *) ect;
}

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
e_cell_text_construct (ECellText *cell,
                       const gchar *fontname,
                       GtkJustification justify)
{
	if (!cell)
		return E_CELL (NULL);
	if (fontname)
		cell->font_name = g_strdup (fontname);
	cell->justify = justify;
	return E_CELL (cell);
}

gchar *
e_cell_text_get_text (ECellText *cell,
                      ETableModel *model,
                      gint col,
                      gint row)
{
	ECellTextClass *class;

	g_return_val_if_fail (E_IS_CELL_TEXT (cell), NULL);

	class = E_CELL_TEXT_GET_CLASS (cell);
	if (class->get_text == NULL)
		return NULL;

	return class->get_text (cell, model, col, row);
}

void
e_cell_text_free_text (ECellText *cell,
		       ETableModel *model,
		       gint col,
                       gchar *text)
{
	ECellTextClass *class;

	g_return_if_fail (E_IS_CELL_TEXT (cell));

	class = E_CELL_TEXT_GET_CLASS (cell);
	if (class->free_text == NULL)
		return;

	class->free_text (cell, model, col, text);
}

void
e_cell_text_set_value (ECellText *cell,
                       ETableModel *model,
                       gint col,
                       gint row,
                       const gchar *text)
{
	ECellTextClass *class;

	g_return_if_fail (E_IS_CELL_TEXT (cell));

	class = E_CELL_TEXT_GET_CLASS (cell);
	if (class->set_value == NULL)
		return;

	class->set_value (cell, model, col, row, text);
}

/* fixme: Handle Font attributes */
/* position is in BYTES */

static gint
get_position_from_xy (CellEdit *edit,
                      gint x,
                      gint y)
{
	gint index;
	gint trailing;
	const gchar *text;

	PangoLayout *layout = generate_layout (edit->text_view, edit->model_col, edit->view_col, edit->row, edit->cell_width);
	ECellTextView *text_view = edit->text_view;
	ECellText *ect = (ECellText *) ((ECellView *) text_view)->ecell;

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
	ECellText *ect = E_CELL_TEXT (((ECellView *) text_view)->ecell);
	CellEdit *edit = text_view->edit;

	gulong current_time;
	gboolean scroll = FALSE;
	gboolean redraw = FALSE;
	gint width, height;

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
			e_text_event_processor_handle_event (
				edit->tep,
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
	if (redraw) {
		ect_queue_redraw (text_view, edit->view_col, edit->row);
	}
	return TRUE;
}

static gint
next_word (CellEdit *edit,
           gint start)
{
	gchar *p;
	gint length;

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

static gint
_get_position (ECellTextView *text_view,
               ETextEventProcessorCommand *command)
{
	gint length;
	CellEdit *edit = text_view->edit;
	gchar *p;
	gint unival;
	gint index;
	gint trailing;

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
		pango_layout_move_cursor_visually (
			edit->layout,
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
		pango_layout_move_cursor_visually (
			edit->layout,
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
		break;
	}

	return edit->selection_end;
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

	g_signal_emit (VIEW_TO_CELL (text_view), signals[TEXT_DELETED], 0, text_view, edit->selection_start, ep - sp, edit->row, edit->model_col);
}

/* fixme: */
/* NB! We expect value to be length IN BYTES */

static void
_insert (ECellTextView *text_view,
         const gchar *string,
         gint value)
{
	CellEdit *edit = text_view->edit;
	gchar *temp;

	if (value <= 0) return;

	edit->selection_start = MIN (strlen (edit->text), edit->selection_start);

	temp = g_new (gchar, strlen (edit->text) + value + 1);

	strncpy (temp, edit->text, edit->selection_start);
	strncpy (temp + edit->selection_start, string, value);
	strcpy (temp + edit->selection_start + value, edit->text + edit->selection_end);

	g_free (edit->text);

	edit->text = temp;

	edit->selection_start += value;
	edit->selection_end = edit->selection_start;

	g_signal_emit (VIEW_TO_CELL (text_view), signals[TEXT_INSERTED], 0, text_view, edit->selection_end - value, value, edit->row, edit->model_col);
}

static void
capitalize (CellEdit *edit,
            gint start,
            gint end,
            ETextEventProcessorCaps type)
{
	ECellTextView *text_view = edit->text_view;

	gboolean first = TRUE;
	gint character_length = g_utf8_strlen (edit->text + start, start - end);
	const gchar *p = edit->text + start;
	const gchar *text_end = edit->text + end;
	gchar *new_text = g_new0 (char, character_length * 6 + 1);
	gchar *output = new_text;

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
e_cell_text_view_command (ETextEventProcessor *tep,
                          ETextEventProcessorCommand *command,
                          gpointer data)
{
	CellEdit *edit = (CellEdit *) data;
	ECellTextView *text_view = edit->text_view;
	ECellText *ect = E_CELL_TEXT (text_view->cell_view.ecell);

	gboolean change = FALSE;
	gboolean redraw = FALSE;

	gint sel_start, sel_end;

	/* If the EText isn't editable, then ignore any commands that would
	 * modify the text. */
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
		sel_start = MIN (edit->selection_start, edit->selection_end);
		sel_end = MAX (edit->selection_start, edit->selection_end);
		if (sel_start != sel_end) {
			e_cell_text_view_supply_selection (
				edit, command->time, GDK_SELECTION_PRIMARY,
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
		sel_start = MIN (edit->selection_start, edit->selection_end);
		sel_end = MAX (edit->selection_start, edit->selection_end);
		if (sel_start != sel_end) {
			e_cell_text_view_supply_selection (
				edit, command->time, clipboard_atom,
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
			gint selection_start = MIN (edit->selection_start, edit->selection_end);
			gint selection_end = edit->selection_start + edit->selection_end - selection_start; /* Slightly faster than MAX */
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

	if (redraw) {
		ect_queue_redraw (text_view, edit->view_col, edit->row);
	}
}

static void
e_cell_text_view_supply_selection (CellEdit *edit,
                                   guint time,
                                   GdkAtom selection,
                                   gchar *data,
                                   gint length)
{
#if DO_SELECTION
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (edit->text_view->canvas), selection);

	if (selection == GDK_SELECTION_PRIMARY) {
		edit->has_selection = TRUE;
	}

	gtk_clipboard_set_text (clipboard, data, length);
#endif
}

#ifdef DO_SELECTION
static void
paste_received (GtkClipboard *clipboard,
                const gchar *text,
                gpointer data)
{
	CellEdit *edit;

	g_return_if_fail (data);

	edit = (CellEdit *) data;

	if (text && g_utf8_validate (text, strlen (text), NULL)) {
		ETextEventProcessorCommand command = { 0 };
		command.action = E_TEP_INSERT;
		command.position = E_TEP_SELECTION;
		command.string = (gchar *) text;
		command.value = strlen (text);
		command.time = GDK_CURRENT_TIME;
		e_cell_text_view_command (edit->tep, &command, edit);
	}
}
#endif

static void
e_cell_text_view_get_selection (CellEdit *edit,
                                GdkAtom selection,
                                guint32 time)
{
#if DO_SELECTION
	gtk_clipboard_request_text (
		gtk_widget_get_clipboard (GTK_WIDGET (edit->text_view->canvas),
		selection),
		paste_received, edit);
#endif
}

static void
_get_tep (CellEdit *edit)
{
	if (!edit->tep) {
		edit->tep = e_text_event_processor_emacs_like_new ();
		g_signal_connect (
			edit->tep, "command",
			G_CALLBACK (e_cell_text_view_command), edit);
	}
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
	ETextEventProcessorCommand command1 = { 0 }, command2 = { 0 };

	g_return_val_if_fail (cell_view != NULL, FALSE);

	ectv = (ECellTextView *) cell_view;
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
 * @start: a pointer to an gint value indicates the start offset of the selection
 * @end: a pointer to an gint value indicates the end offset of the selection
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

	g_return_val_if_fail (cell_view != NULL, FALSE);

	ectv = (ECellTextView *) cell_view;
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
e_cell_text_copy_clipboard (ECellView *cell_view,
                            gint col,
                            gint row)
{
	ECellTextView *ectv;
	CellEdit *edit;
	ETextEventProcessorCommand command = { 0 };

	g_return_if_fail (cell_view != NULL);

	ectv = (ECellTextView *) cell_view;
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
e_cell_text_paste_clipboard (ECellView *cell_view,
                             gint col,
                             gint row)
{
	ECellTextView *ectv;
	CellEdit *edit;
	ETextEventProcessorCommand command = { 0 };

	g_return_if_fail (cell_view != NULL);

	ectv = (ECellTextView *) cell_view;
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
e_cell_text_delete_selection (ECellView *cell_view,
                              gint col,
                              gint row)
{
	ECellTextView *ectv;
	CellEdit *edit;
	ETextEventProcessorCommand command = { 0 };

	g_return_if_fail (cell_view != NULL);

	ectv = (ECellTextView *) cell_view;
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
gchar *
e_cell_text_get_text_by_view (ECellView *cell_view,
                              gint col,
                              gint row)
{
	ECellTextView *ectv;
	CellEdit *edit;
	gchar	*ret, *model_text;

	g_return_val_if_fail (cell_view != NULL, NULL);

	ectv = (ECellTextView *) cell_view;
	edit = ectv->edit;

	if (edit && ectv->edit->row == row && ectv->edit->model_col == col) { /* being editted now */
		ret = g_strdup (edit->text);
	} else{
		model_text = e_cell_text_get_text (
			E_CELL_TEXT (cell_view->ecell),
			cell_view->e_table_model, col, row);
		ret = g_strdup (model_text);
		e_cell_text_free_text (E_CELL_TEXT (cell_view->ecell), cell_view->e_table_model, col, model_text);
	}

	return ret;

}

PangoEllipsizeMode
e_cell_text_get_ellipsize_mode (ECellText *self)
{
	ECellTextPrivate *priv;

	g_return_val_if_fail (E_IS_CELL_TEXT (self), PANGO_ELLIPSIZE_NONE);

	priv = e_cell_text_get_instance_private (self);

	return priv->ellipsize_mode;
}

void
e_cell_text_set_ellipsize_mode (ECellText *self,
				PangoEllipsizeMode mode)
{
	ECellTextPrivate *priv;

	g_return_if_fail (E_IS_CELL_TEXT (self));

	priv = e_cell_text_get_instance_private (self);

	priv->ellipsize_mode = mode;
}
