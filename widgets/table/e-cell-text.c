/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-cell-text.c - Text cell renderer
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Miguel de Icaza <miguel@helixcode.com>
 *          Chris Lahey <clahey@helixcode.com>
 *         
 * A majority of code taken from:
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
 * TODO:
 *   Clean up UTF-8 handling
 *   UTF-8 selection
 */

#include <config.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkinvisible.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomeui/gnome-canvas.h>
#include <stdio.h>
#include <unicode.h>
#include "e-cell-text.h"
#include "e-util/e-util.h"
#include "e-util/e-font.h"
#include "e-util/e-unicode.h"
#include "e-table-item.h"
#include "e-text-event-processor.h"
#include "e-text-event-processor-emacs-like.h"

#include <gdk/gdkx.h> /* for BlackPixel */
#include <ctype.h>
#include <math.h>

/* This defines a line of text */
struct line {
	char *text;	/* Line's text UTF-8, it is a pointer into the text->text string */
	int length;	/* Line's length in BYTES */
	int width;	/* Line's width in pixels */
	int ellipsis_length;  /* Length before adding ellipsis in BYTES */
};

/* Object argument IDs */
enum {
	ARG_0,
	ARG_TEXT, /* UTF-8 */
	ARG_X,
	ARG_Y,
	ARG_FONT,
        ARG_FONTSET,
	ARG_FONT_GDK,
	ARG_ANCHOR,
	ARG_JUSTIFICATION,
	ARG_CLIP_WIDTH,
	ARG_CLIP_HEIGHT,
	ARG_CLIP,
	ARG_X_OFFSET,
	ARG_Y_OFFSET,
	ARG_FILL_COLOR,
	ARG_FILL_COLOR_GDK,
	ARG_FILL_COLOR_RGBA,
	ARG_FILL_STIPPLE,
	ARG_TEXT_WIDTH,
	ARG_TEXT_HEIGHT,
	ARG_USE_ELLIPSIS,
	ARG_ELLIPSIS,
	ARG_STRIKEOUT_COLUMN,
	ARG_BOLD_COLUMN,
	ARG_TEXT_FILTER,
	ARG_COLOR_COLUMN,
};


enum {
	E_SELECTION_PRIMARY,
	E_SELECTION_CLIPBOARD
};

static GdkAtom clipboard_atom = GDK_NONE;

#define PARENT_TYPE e_cell_get_type ()

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
#if 0
	GdkFont     *font;
#else
	EFont *font;
#endif
	GdkCursor *i_cursor;
	GdkBitmap *stipple;		/* Stipple for text */
	
	GnomeCanvas *canvas;

	/*
	 * During editing.
	 */
	CellEdit    *edit;


	int xofs, yofs;                 /* This gets added to the x
                                           and y for the cell text. */
	double ellipsis_width;          /* The width of the ellipsis. */

} ECellTextView;

typedef struct _CurrentCell{
	ECellTextView *text_view;
	int            width;
	char          *text;
	int            model_col, view_col, row;
	ECellTextLineBreaks *breaks;
} CurrentCell;

#define CURRENT_CELL(x) ((CurrentCell *)(x))

struct _CellEdit {
	CurrentCell cell;

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

	/* Hmmm... this should probably be in native encoding? */

	GtkWidget *invisible;           /* For selection handling */
	gboolean has_selection;         /* TRUE if we have the selection */
	gchar *primary_selection;       /* Primary selection text */
	gint primary_length;            /* Primary selection text length in BYTES */
	gchar *clipboard_selection;     /* Clipboard selection text */
	gint clipboard_length;          /* Clipboard selection text length in BYTES */

	guint pointer_in : 1;
	guint default_cursor_shown : 1;
};

static void e_cell_text_view_command (ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data);

static void e_cell_text_view_get_selection (CellEdit *edit, GdkAtom selection, guint32 time);
static void e_cell_text_view_supply_selection (CellEdit *edit, guint time, GdkAtom selection, char *data, gint length);

static GtkWidget *e_cell_text_view_get_invisible (CellEdit *edit);
static void _selection_clear_event (GtkInvisible *invisible,
				    GdkEventSelection *event,
				    CellEdit *edit);
static void _selection_get (GtkInvisible *invisible,
			    GtkSelectionData *selection_data,
			    guint info,
			    guint time_stamp,
			    CellEdit *edit);
static void _selection_received (GtkInvisible *invisible,
				 GtkSelectionData *selection_data,
				 guint time,
				 CellEdit *edit);
static int number_of_lines (char *text);
static void split_into_lines (CurrentCell *cell);
static void unref_lines (CurrentCell *cell);
static void calc_line_widths (CurrentCell *cell);
static int get_line_ypos (CurrentCell *cell, struct line *line);
static int get_line_xpos (CurrentCell *cell, struct line *line);
static void _get_tep (CellEdit *edit);

static gint _get_position_from_xy (CurrentCell *cell, gint x, gint y);
static void _get_xy_from_position (CurrentCell *cell, gint position, gint *xp, gint *yp);
static gboolean _blink_scroll_timeout (gpointer data);

static void build_current_cell (CurrentCell *cell, ECellTextView *text_view, int model_col, int view_col, int row);
static void unbuild_current_cell (CurrentCell *cell);
static void calc_ellipsis (ECellTextView *text_view);
static void ect_free_color (gchar *color_spec, GdkColor *color, GdkColormap *colormap);
static GdkColor* e_cell_text_get_color (ECellTextView *cell_view, gchar *color_spec);

static ECellClass *parent_class;

static void
ect_queue_redraw (ECellTextView *text_view, int view_col, int view_row)
{
	e_table_item_redraw_range (
		text_view->cell_view.e_table_item_view,
		view_col, view_row, view_col, view_row);
}

/*
 * Accept the currently edited text.  if it's the same as what's in the cell, do nothing.
 */
static void
ect_accept_edits (ECellTextView *text_view)
{
	CurrentCell *cell = (CurrentCell *) text_view->edit;

	if (strcmp (text_view->edit->old_text, cell->text)) {
		e_table_model_set_value_at (text_view->cell_view.e_table_model,
					    cell->model_col, cell->row, cell->text);
	}
}

/*
 * Shuts down the editing process
 */
static void
ect_stop_editing (ECellTextView *text_view)
{
	CellEdit *edit = text_view->edit;
	int row, view_col;

	if (!edit)
		return;

	row = edit->cell.row;
	view_col = edit->cell.view_col;
	
	g_free (edit->old_text);
	edit->old_text = NULL;
	g_free (edit->cell.text);
	edit->cell.text = NULL;
	if (edit->invisible)
		gtk_widget_unref (edit->invisible);
	if (edit->tep)
		gtk_object_unref (GTK_OBJECT(edit->tep));
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

	g_free (edit);
	
	text_view->edit = NULL;
	ect_queue_redraw (text_view, view_col, row);
}

/*
 * Cancels the edits
 */
static void
ect_cancel_edit (ECellTextView *text_view)
{
	ect_stop_editing (text_view);
}

/*
 * ECell::new_view method
 */
static ECellView *
ect_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	ECellText *ect = E_CELL_TEXT (ecell);
	ECellTextView *text_view = g_new0 (ECellTextView, 1);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (e_table_item_view)->canvas;
	
	text_view->cell_view.ecell = ecell;
	text_view->cell_view.e_table_model = table_model;
	text_view->cell_view.e_table_item_view = e_table_item_view;
	
	if (ect->font_name){
#if 0
		GdkFont *f;

		f = gdk_fontset_load (ect->font_name);
		text_view->font = f;
#endif
		text_view->font = e_font_from_gdk_name (ect->font_name);
	}
	if (!text_view->font){
		text_view->font = e_font_from_gdk_font (GTK_WIDGET (canvas)->style->font);
	}

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
	
	calc_ellipsis (text_view);

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

	if (text_view->font)
		e_font_unref (text_view->font);
	
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
		gdk_colors_free (colormap, &color->pixel, 1, 0);

		/* This frees the memory for the GdkColor. */
		gdk_color_free (color);
	}
}

/*
 * ECell::draw method
 */
static void
ect_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int model_col, int view_col, int row, gboolean selected,
	  int x1, int y1, int x2, int y2)
{
	/* New ECellText */
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	GtkWidget *canvas = GTK_WIDGET (text_view->canvas);
	GdkRectangle rect, *clip_rect;
	struct line *lines;
	int i;
	int xpos, ypos;
	int start_char, end_char;
	int sel_start, sel_end;
	GdkRectangle sel_rect;
	GdkGC *fg_gc;
	EFont *font = text_view->font;
	const int height = e_font_height (text_view->font);
	CellEdit *edit = text_view->edit;
	gboolean edit_display = FALSE;
	ECellTextLineBreaks *linebreaks;
	GdkColor *foreground, *cell_foreground, *cursor_color;
	gchar *color_spec;

	EFontStyle style = E_FONT_PLAIN;

	if (ect->bold_column >= 0 && e_table_model_value_at(ecell_view->e_table_model, ect->bold_column, row))
		style = E_FONT_BOLD;

	if (edit){
		if ((edit->cell.view_col == view_col) && (edit->cell.row == row)) {
			edit_display = TRUE;
			fg_gc = canvas->style->fg_gc[edit->has_selection ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE];
		} else
			fg_gc = canvas->style->fg_gc[GTK_STATE_ACTIVE];
	} else {
		fg_gc = canvas->style->fg_gc[GTK_STATE_ACTIVE];
	}

	/*
	 * Be a nice citizen: clip to the region we are supposed to draw on
	 */
	rect.x = x1;
	rect.y = y1;
	rect.width = x2 - x1;
	rect.height = y2 - y1;
	
	gdk_gc_set_clip_rectangle (text_view->gc, &rect);
	gdk_gc_set_clip_rectangle (fg_gc, &rect);
	clip_rect = &rect;

	if (selected){
		foreground = &canvas->style->text [GTK_STATE_SELECTED];
	} else {
		foreground = &canvas->style->text [GTK_STATE_NORMAL];
	}

	cursor_color = foreground;

	if (ect->color_column != -1) {
		color_spec = e_table_model_value_at (ecell_view->e_table_model,
						     ect->color_column, row);
		cell_foreground = e_cell_text_get_color (text_view,
							 color_spec);
		if (cell_foreground)
			foreground = cell_foreground;
	}

	gdk_gc_set_foreground (text_view->gc, foreground);

	x1 += 4;
	y1 += 1;
	x2 -= 4;
	y2 -= 1;

	rect.x = x1;
	rect.y = y1;
	rect.width = x2 - x1;
	rect.height = y2 - y1;
	
	gdk_gc_set_clip_rectangle (text_view->gc, &rect);
	gdk_gc_set_clip_rectangle (fg_gc, &rect);
	clip_rect = &rect;

	if (edit_display){
		CellEdit *edit = text_view->edit;
		CurrentCell *cell = CURRENT_CELL(edit);

		cell->width = x2 - x1;
		
		split_into_lines (cell);

		linebreaks = cell->breaks;
		
		lines = linebreaks->lines;
		ypos = get_line_ypos (cell, lines);
		ypos += e_font_ascent (text_view->font);
		ypos -= edit->yofs_edit;

		for (i = 0; i < linebreaks->num_lines; i++) {
			xpos = get_line_xpos (cell, lines);
			xpos -= edit->xofs_edit;

			/* start_char, end_char, sel_start and sel_end are IN BYTES */

			start_char = lines->text - cell->text;
			end_char = start_char + lines->length;

			sel_start = edit->selection_start;
			sel_end = edit->selection_end;

			if (sel_start > sel_end){
				sel_start ^= sel_end;
				sel_end ^= sel_start;
				sel_start ^= sel_end;
			}
			if (sel_start < start_char)
				sel_start = start_char;
			if (sel_end > end_char)
				sel_end = end_char;

			if (sel_start < sel_end){
				sel_rect.x = xpos + x1 + e_font_utf8_text_width (font, style, lines->text, sel_start - start_char);
				sel_rect.y = ypos + y1 - e_font_ascent (font);
				sel_rect.width = e_font_utf8_text_width (font, style,
									 lines->text + sel_start - start_char,
									 sel_end - sel_start);
				sel_rect.height = height;
				gtk_paint_flat_box (canvas->style,
						    drawable,
						    edit->has_selection ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE,
						    GTK_SHADOW_NONE,
						    clip_rect,
						    canvas,
						    "text",
						    sel_rect.x,
						    sel_rect.y,
						    sel_rect.width,
						    sel_rect.height);

				e_font_draw_utf8_text (drawable, font, style, text_view->gc, xpos + x1, ypos + y1,
						       lines->text,
						       sel_start - start_char);
				e_font_draw_utf8_text (drawable, font, style, fg_gc,
						       xpos + x1 + e_font_utf8_text_width (font, style, lines->text, sel_start - start_char),
						       ypos + y1,
						       lines->text + sel_start - start_char,
						       sel_end - sel_start);
				e_font_draw_utf8_text (drawable, font, style, text_view->gc,
						       xpos + x1 + e_font_utf8_text_width (font, style, lines->text, sel_end - start_char),
						       ypos + y1,
						       lines->text + sel_end - start_char,
						       end_char - sel_end);
#if 0 /* Do Bold in EFont directly */
				/* Draw 1,0 moved image, to simulate bold font */
				/* We move that to EFont */
				if (bold) {
					e_font_draw_utf8_text (drawable, font, text_view->gc, xpos + x1 + 1, ypos + y1,
						       lines->text,
						       sel_start - start_char);
					e_font_draw_utf8_text (drawable, font, fg_gc,
						       xpos + x1 + 1 + e_font_utf8_text_width (font, lines->text, sel_start - start_char),
						       ypos + y1,
						       g_utf8_offset_to_pointer (lines->text, sel_start - start_char),
						       sel_end - sel_start);
					e_font_draw_utf8_text (drawable, font, text_view->gc,
						       xpos + x1 + 1 + e_font_utf8_text_width (font, lines->text, sel_end - start_char),
						       ypos + y1,
						       g_utf8_offset_to_pointer (lines->text, sel_end - start_char),
						       end_char - sel_end);
				}
#endif
			} else {
				e_font_draw_utf8_text (drawable, font, style, text_view->gc,
						       xpos + x1, ypos + y1,
						       lines->text,
						       lines->length);
#if 0
				if (bold) {
					e_font_draw_ucs2_text (drawable,
						       font,
						       text_view->gc,
						       xpos + x1 + 1,
						       ypos + y1,
						       lines->text,
						       lines->length);
				}
#endif
			}
			if (edit->selection_start == edit->selection_end &&
			    edit->selection_start >= start_char &&
			    edit->selection_start <= end_char &&
			    edit->show_cursor) {
				gdk_gc_set_foreground (text_view->gc, cursor_color);
				gdk_draw_rectangle (drawable,
						    text_view->gc,
						    TRUE,
						    xpos + x1 + e_font_utf8_text_width (font, style, lines->text, sel_start - start_char),
						    ypos + y1 - e_font_ascent (font),
						    1,
						    height);
			}
			if (ect->strikeout_column >= 0 && e_table_model_value_at(ecell_view->e_table_model, ect->strikeout_column, row)) {
				gdk_draw_rectangle (drawable,
						    text_view->gc,
						    TRUE,
						    x1, ypos + y1 - (e_font_ascent (font) / 2),
						    x2 - x1,
						    1);
			}
			ypos += height;
			lines ++;
		}
		unref_lines (cell);
	} else {
		
		ECellTextLineBreaks *linebreaks;
		CurrentCell cell;
		build_current_cell (&cell, text_view, model_col, view_col, row);
		
		cell.width = x2 - x1;
		
		split_into_lines (&cell);
		
		linebreaks = cell.breaks;
		lines = linebreaks->lines;
		ypos = get_line_ypos (&cell, lines);
		ypos += e_font_ascent (text_view->font);
		
		
		for (i = 0; i < linebreaks->num_lines; i++) {
			xpos = get_line_xpos (&cell, lines);
			if (ect->use_ellipsis && lines->ellipsis_length < lines->length) {
				e_font_draw_utf8_text (drawable, font, style, text_view->gc,
					       xpos + x1, ypos + y1,
					       lines->text,
					       lines->ellipsis_length);
				e_font_draw_utf8_text (drawable, font, style, text_view->gc,
					       xpos + x1 + lines->width - text_view->ellipsis_width,
					       ypos + y1,
					       ect->ellipsis ? ect->ellipsis : "...",
					       ect->ellipsis ? strlen (ect->ellipsis) : 3);
#if 0
				if (bold) {
					e_font_draw_ucs2_text (drawable,
						       font,
						       text_view->gc,
						       xpos + x1 + 1,
						       ypos + y1,
						       lines->text,
						       lines->ellipsis_length);
					e_font_draw_utf8_text (drawable,
						       font,
						       text_view->gc,
						       xpos + x1 + 1 +
						       lines->width - text_view->ellipsis_width,
						       ypos + y1,
						       ect->ellipsis ? ect->ellipsis : "...",
						       ect->ellipsis ? strlen (ect->ellipsis) : 3);
				}
#endif
			} else {
				e_font_draw_utf8_text (drawable, font, style, text_view->gc,
					       xpos + x1,
					       ypos + y1,
					       lines->text,
					       lines->length);
#if 0
				if (bold) {
					e_font_draw_ucs2_text (drawable,
						       font,
						       text_view->gc,
						       xpos + x1 + 1,
						       ypos + y1,
						       lines->text,
						       lines->length);
				}
#endif
			}
			if (ect->strikeout_column >= 0 && e_table_model_value_at(ecell_view->e_table_model, ect->strikeout_column, row)) {
				gdk_draw_rectangle (drawable,
						    text_view->gc,
						    TRUE,
						    x1, ypos + y1 - (e_font_ascent (font) / 2),
						    x2 - x1,
						    1);
			}
			ypos += height;
			lines++;
		}
		unref_lines (&cell);
		unbuild_current_cell (&cell);
	}

	gdk_gc_set_clip_rectangle (text_view->gc, NULL);
	gdk_gc_set_clip_rectangle (fg_gc, NULL);
#if 0
	/* Old ECellText */

	int xoff;
	gboolean edit_display = FALSE;

	/*
	 * Figure if this cell is being edited
	 */
	if (edit_display){
		CellEdit *edit = text_view->edit;
		const char *text = gtk_entry_get_text (edit->entry);
		GdkWChar *p, *text_wc = g_new (GdkWChar, strlen (text) + 1);
		int text_wc_len = gdk_mbstowcs (text_wc, text, strlen (text));
		const int cursor_pos = GTK_EDITABLE (edit->entry)->current_pos;
		const int left_len = gdk_text_width_wc (text_view->font, text_wc, cursor_pos);

		text_wc [text_wc_len] = 0;
		/*
		 * Paint
		 */
		gdk_gc_set_foreground (text_view->gc, &w->style->base [GTK_STATE_NORMAL]);
		gdk_draw_rectangle (drawable, text_view->gc, TRUE,
				    rect.x, rect.y, rect.width, rect.height);
		gdk_gc_set_foreground (text_view->gc, &w->style->text [GTK_STATE_NORMAL]);

		{
			GdkGC *gc = text_view->gc;
			const int y = y2 - font->descent - ((y2-y1-height)/2);
			int px, i;

			/*
			 * Border
			 */
			x1 += 2;
			x2--;
			
			px = x1;

			/*
			 * If the cursor is outside the visible range
			 *
			 * FIXME: we really want a better behaviour.
			 */
			if ((px + left_len) > x2)
				px -= left_len - (x2-x1);

			/*
			 * Draw
			 */
			for (i = 0, p = text_wc; *p; p++, i++){
				gdk_draw_text_wc (
						  drawable, font, gc, px, y, p, 1);

				if (i == cursor_pos){
					gdk_draw_line (
						       drawable, gc,
						       px, y - font->ascent,
						       px, y + font->descent - 1);
				}

				px += gdk_text_width_wc (font, p, 1);
			}

			if (i == cursor_pos){
				gdk_draw_line (
					       drawable, gc,
					       px, y - font->ascent,
					       px, y + font->descent - 1);
			}
		}
		g_free (text_wc);
	} else {
		/*
		 * Regular cell
		 */
		GdkColor *background, *foreground;
		int width;

		/*
		 * Border
		 */
		x1++;
		x2--;
		
		/*
		 * Compute draw mode
		 */
		switch (ect->justify){
		case GTK_JUSTIFY_LEFT:
			xoff = 1;
			break;
			
		case GTK_JUSTIFY_RIGHT:
			width = 1 + gdk_text_width (font, str, strlen (str));
			xoff = (x2 - x1) - width;
			break;
			
		case GTK_JUSTIFY_CENTER:
			xoff = ((x2 - x1) - gdk_text_width (font, str, strlen (str))) / 2;
			break;
		default:
			xoff = 0;
			g_warning ("Can not handle GTK_JUSTIFY_FILL");
			break;
		}
		
		
		if (selected){
			background = &w->style->bg [GTK_STATE_SELECTED];
			foreground = &w->style->text [GTK_STATE_SELECTED];
		} else {
			background = &w->style->base [GTK_STATE_NORMAL];
			foreground = &w->style->text [GTK_STATE_NORMAL];
		}
		
		gdk_gc_set_foreground (text_view->gc, background); 
		gdk_draw_rectangle (drawable, text_view->gc, TRUE,
				    rect.x, rect.y, rect.width, rect.height);
		gdk_gc_set_foreground (text_view->gc, foreground);

		gdk_draw_string (
				 drawable, font, text_view->gc,
				 x1 + xoff,
				 y2 - font->descent - ((y2-y1-height)/2), str);
	}
#endif
}

/*
 * Selects the entire string
 */
static void
ect_edit_select_all (ECellTextView *text_view)
{
	g_assert (text_view->edit);
	
	text_view->edit->selection_start = 0;
	text_view->edit->selection_end = strlen (text_view->edit->cell.text);
}

/*
 * ECell::event method
 */
static gint
ect_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	ETextEventProcessorEvent e_tep_event;
	gboolean edit_display = FALSE;
	CellEdit *edit = text_view->edit;
	GtkWidget *canvas = GTK_WIDGET (text_view->canvas);
	gint return_val = 0;

	CurrentCell cell, *cellptr;
	build_current_cell (&cell, text_view, model_col, view_col, row);
	

	if (edit){
		if ((edit->cell.view_col == view_col) && (edit->cell.row == row)) {
			edit_display = TRUE;
			cellptr = CURRENT_CELL(edit);
		} else {
			cellptr = &cell;
		}
	} else {
		cellptr = &cell;
	}

	e_tep_event.type = event->type;
	switch (event->type) {
	case GDK_FOCUS_CHANGE:
		break;
	case GDK_KEY_PRESS: /* Fall Through */
	case GDK_KEY_RELEASE:
		if (event->key.keyval == GDK_Escape){
			ect_cancel_edit (text_view);
			return_val = TRUE;
			break;
		}
		
		if ((!edit_display) && e_table_model_is_cell_editable (ecell_view->e_table_model, model_col, row)) {
			  e_table_item_enter_edit (text_view->cell_view.e_table_item_view, view_col, row);
			  ect_edit_select_all (text_view);
			  edit = text_view->edit;
			  cellptr = CURRENT_CELL(edit);
			  edit_display = TRUE;
		}		
		if (edit_display) {
			GdkEventKey key = event->key;
			if (key.keyval == GDK_KP_Enter || key.keyval == GDK_Return){
				e_table_item_leave_edit (text_view->cell_view.e_table_item_view);
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
				if (e_tep_event.key.string) g_free (e_tep_event.key.string);
				break;
			}
		}

		else
			return 0;
		break;
	case GDK_BUTTON_PRESS: /* Fall Through */
	case GDK_BUTTON_RELEASE:
		event->button.x -= 4;
		event->button.y -= 1;
		if ((!edit_display) 
		    && e_table_model_is_cell_editable (ecell_view->e_table_model, model_col, row)
		    && event->type == GDK_BUTTON_RELEASE
		    && event->button.button == 1) {
			GdkEventButton button = event->button;

			e_table_item_enter_edit (text_view->cell_view.e_table_item_view, view_col, row);
			edit = text_view->edit;
			cellptr = CURRENT_CELL(edit);
			edit_display = TRUE;
			
			e_tep_event.button.type = GDK_BUTTON_PRESS;
			e_tep_event.button.time = button.time;
			e_tep_event.button.state = button.state;
			e_tep_event.button.button = button.button;
			e_tep_event.button.position = _get_position_from_xy (cellptr, button.x, button.y);
			_get_tep (edit);
			return_val = e_text_event_processor_handle_event (edit->tep,
									  &e_tep_event);
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
			e_tep_event.button.position = _get_position_from_xy (cellptr, button.x, button.y);
			_get_tep (edit);
			return_val = e_text_event_processor_handle_event (edit->tep,
									  &e_tep_event);
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
			e_tep_event.motion.position = _get_position_from_xy (cellptr, motion.x, motion.y);
			_get_tep (edit);
			return_val = e_text_event_processor_handle_event (edit->tep,
									  &e_tep_event);
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

	unbuild_current_cell (&cell);
	if (return_val)
		return return_val;
#if 0
	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->event)
		return GNOME_CANVAS_ITEM_CLASS(parent_class)->event (item, event);
#endif
	else
		return 0;
	
#if 0
	switch (event->type){
	case GDK_BUTTON_PRESS:
		/*
		 * Adjust for the border we use
		 */
		event->button.x++;
		
		printf ("Button pressed at %g %g\n", event->button.x, event->button.y);
		if (text_view->edit){
			printf ("FIXME: Should handle click here\n");
		} else 
			e_table_item_enter_edit (text_view->cell_view.e_table_item_view, view_col, row);
		break;

	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_Escape){
			ect_cancel_edit (text_view);
			return TRUE;
		}
		
		if (!text_view->edit){
			e_table_item_enter_edit (text_view->cell_view.e_table_item_view, view_col, row);
			ect_edit_select_all (text_view);
		}

		gtk_widget_event (GTK_WIDGET (text_view->edit->entry), event);
		ect_queue_redraw (text_view, view_col, row);
		break;
#endif
}

/*
 * ECell::height method
 */
static int
ect_height (ECellView *ecell_view, int model_col, int view_col, int row) 
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	EFont *font;
	ECellText *ect = E_CELL_TEXT(ecell_view->ecell);
	
	font = text_view->font;
	if (ect->filter) {
		gchar *string;
		gint value;

		string = (*ect->filter)(e_table_model_value_at (ecell_view->e_table_model, model_col, row));
		value = e_font_height (font) * number_of_lines(string) + TEXT_PAD;

		g_free(string);

		return value;
	} else {
		gchar * string;
		gint value;

		string = e_table_model_value_at (ecell_view->e_table_model, model_col, row);

		value = e_font_height (font) * number_of_lines (string) + TEXT_PAD;

		return value;
	}
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

	edit = g_new (CellEdit, 1);
	text_view->edit = edit;

	build_current_cell (CURRENT_CELL(edit), text_view, model_col, view_col, row);
	
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
	
	if (ect->filter) {
		edit->old_text = (*ect->filter)(e_table_model_value_at (ecell_view->e_table_model, model_col, row));
	} else {
		edit->old_text = g_strdup (e_table_model_value_at (ecell_view->e_table_model, model_col, row));
	}
	edit->cell.text = g_strdup (edit->old_text);

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
		ect_accept_edits (text_view);
		ect_stop_editing (text_view);
		/* FIXME: edit is freed in ect_stop_editing() so I've
		   commented this out - Damon. */
		/*unbuild_current_cell (CURRENT_CELL(edit));*/
	} else {
		/*
		 * We did invoke this leave edit internally
		 */
	}
}

static void
ect_print (ECellView *ecell_view, GnomePrintContext *context, 
	   int model_col, int view_col, int row,
	   double width, double height)
{
	GnomeFont *font = gnome_font_new ("Helvetica", 12);
	char *string;
	ECellText *ect = E_CELL_TEXT(ecell_view->ecell);
	if (ect->filter) {
		string = (*ect->filter)(e_table_model_value_at (ecell_view->e_table_model, model_col, row));
	} else {
		string = e_table_model_value_at (ecell_view->e_table_model, model_col, row);
	}
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
	gnome_print_moveto(context, 2, (height - gnome_font_get_ascender(font) + gnome_font_get_descender(font)) / 2);
	gnome_print_setfont(context, font);
	gnome_print_show(context, string);
	gnome_print_grestore(context);
	if (ect->filter) {
		g_free(string);
	}
}

static gdouble
ect_print_height (ECellView *ecell_view, GnomePrintContext *context, 
		  int model_col, int view_col, int row,
		  double width)
{
	return 16;
}

/*
 * GtkObject::destroy method
 */
static void
ect_destroy (GtkObject *object)
{
	ECellText *ect = E_CELL_TEXT (object);

	g_free (ect->font_name);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}
/* Set_arg handler for the text item */
static void
ect_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ECellText *text;

	text = E_CELL_TEXT (object);

	switch (arg_id) {
	case ARG_STRIKEOUT_COLUMN:
		text->strikeout_column = GTK_VALUE_INT (*arg);
		break;

	case ARG_BOLD_COLUMN:
		text->bold_column = GTK_VALUE_INT (*arg);
		break;

	case ARG_COLOR_COLUMN:
		text->color_column = GTK_VALUE_INT (*arg);
		break;

	case ARG_TEXT_FILTER:
		text->filter = GTK_VALUE_POINTER (*arg);
		break;
	default:
		return;
	}
}

/* Get_arg handler for the text item */
static void
ect_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ECellText *text;

	text = E_CELL_TEXT (object);

	switch (arg_id) {
	case ARG_STRIKEOUT_COLUMN:
		GTK_VALUE_INT (*arg) = text->strikeout_column;
		break;

	case ARG_BOLD_COLUMN:
		GTK_VALUE_INT (*arg) = text->bold_column;
		break;

	case ARG_COLOR_COLUMN:
		GTK_VALUE_INT (*arg) = text->color_column;
		break;

	case ARG_TEXT_FILTER:
		GTK_VALUE_POINTER (*arg) = text->filter;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
e_cell_text_class_init (GtkObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	object_class->destroy = ect_destroy;

	ecc->new_view   = ect_new_view;
	ecc->kill_view  = ect_kill_view;
	ecc->realize    = ect_realize;
	ecc->unrealize  = ect_unrealize;
	ecc->draw       = ect_draw;
	ecc->event      = ect_event;
	ecc->height     = ect_height;
	ecc->enter_edit = ect_enter_edit;
	ecc->leave_edit = ect_leave_edit;
	ecc->print      = ect_print;
	ecc->print_height = ect_print_height;

	object_class->get_arg = ect_get_arg;
	object_class->set_arg = ect_set_arg;

	parent_class = gtk_type_class (PARENT_TYPE);

	gtk_object_add_arg_type ("ECellText::strikeout_column",
				 GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_STRIKEOUT_COLUMN);
	gtk_object_add_arg_type ("ECellText::bold_column",
				 GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_BOLD_COLUMN);
	gtk_object_add_arg_type ("ECellText::color_column",
				 GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_COLOR_COLUMN);
	gtk_object_add_arg_type ("ECellText::text_filter",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_TEXT_FILTER);

	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
}

static void
e_cell_text_init (ECellText *ect)
{
	ect->strikeout_column = -1;
	ect->bold_column = -1;
	ect->color_column = -1;
}

E_MAKE_TYPE(e_cell_text, "ECellText", ECellText, e_cell_text_class_init, e_cell_text_init, PARENT_TYPE);

ECell *
e_cell_text_new (ETableModel *etm, const char *fontname, GtkJustification justify)
{
	ECellText *ect = gtk_type_new (e_cell_text_get_type ());

	ect->ellipsis = NULL;
	ect->use_ellipsis = TRUE;

	ect->font_name = g_strdup (fontname);
	ect->justify = justify;
      
	return (ECell *) ect;
}

/* Calculates the x position of the specified line of text, based on the text's justification */
static int
get_line_xpos (CurrentCell *cell, struct line *line)
{
	int x;
	
	ECellTextView *text_view = cell->text_view;
	ECellText *ect = E_CELL_TEXT (((ECellView *)cell->text_view)->ecell);
	
	x = text_view->xofs + ect->x;

	switch (ect->justify) {
	case GTK_JUSTIFY_RIGHT:
		x += cell->width - line->width;
		break;

	case GTK_JUSTIFY_CENTER:
		x += (cell->width - line->width) / 2;
		break;

	default:
		/* For GTK_JUSTIFY_LEFT, we don't have to do anything.  We do not support
		 * GTK_JUSTIFY_FILL, yet.
		 */
		break;
	}

	return x;
}

/* Calculates the x position of the specified line of text, based on the text's justification */
static int
get_line_ypos (CurrentCell *cell, struct line *line)
{
	int y;

	ECellTextView *text_view = cell->text_view;
	ECellText *ect = E_CELL_TEXT (((ECellView *)cell->text_view)->ecell);
	ECellTextLineBreaks *linebreaks = cell->breaks;

	struct line *lines = linebreaks->lines;

	EFont *font;
	
	font = text_view->font;

	y = text_view->yofs + ect->y;
	y += (line - lines) * e_font_height (font);

	return y;
}

/* fixme: Handle Font attributes */
/* position is in BYTES */

static void
_get_xy_from_position (CurrentCell *cell, gint position, gint *xp, gint *yp)
{
	if (xp || yp) {
		struct line *lines;
		int x, y;
		int j;
		ECellTextView *text_view = cell->text_view;
		ECellTextLineBreaks *linebreaks;
		EFont *font;
		
		font = text_view->font;
		
		split_into_lines (cell);
		
		linebreaks = cell->breaks;
		lines = linebreaks->lines;

		x = get_line_xpos (cell, lines);
		y = get_line_ypos (cell, lines);
		for (j = 0, lines = linebreaks->lines; j < linebreaks->num_lines; lines++, j++) {
			if (lines->text > cell->text + position)
				break;
			y += e_font_height (font);
		}
		lines --;
		y -= e_font_descent (font);
		
		x += e_font_utf8_text_width (font, E_FONT_PLAIN,
					     lines->text,
					     position - (lines->text - cell->text));
		if ((CellEdit *) cell == cell->text_view->edit){
			x -= ((CellEdit *)cell)->xofs_edit;
			y -= ((CellEdit *)cell)->yofs_edit;
		}
		if (xp)
			*xp = x;
		if (yp)
			*yp = y;
		unref_lines (cell);
	}
}

static gint
_get_position_from_xy (CurrentCell *cell, gint x, gint y)
{
	int i, j;
	int xpos, ypos;
	struct line *lines;
	int return_val;
	gchar *p;

	ECellTextView *text_view = cell->text_view;
	ECellTextLineBreaks *linebreaks;
	EFont *font;
	
	font = text_view->font;

	split_into_lines (cell);
	
	linebreaks = cell->breaks;

	lines = linebreaks->lines;

	if ((CellEdit *) cell == cell->text_view->edit){
		x += ((CellEdit *)cell)->xofs_edit;
		y += ((CellEdit *)cell)->yofs_edit;
	}
	
	ypos = get_line_ypos (cell, linebreaks->lines);
	j = 0;
	while (y > ypos) {
		ypos += e_font_height (font);
		j ++;
	}
	j--;
	if (j >= linebreaks->num_lines)
		j = linebreaks->num_lines - 1;
	if (j < 0)
		j = 0;
	i = 0;

	lines += j;
	xpos = get_line_xpos (cell, lines);

	for (p = lines->text; p < lines->text + lines->length; p = unicode_next_utf8 (p)) {
		gint charwidth;

		charwidth = e_font_utf8_char_width (font, E_FONT_PLAIN, p);

		xpos += charwidth / 2;
		if (xpos > x) {
			break;
		}
		xpos += (charwidth + 1) / 2;
	}
	
	return_val = p - cell->text;

	unref_lines (cell);

	return return_val;
}

#define SCROLL_WAIT_TIME 30000

static gboolean
_blink_scroll_timeout (gpointer data)
{
	CurrentCell *cell = CURRENT_CELL(data);
	ECellTextView *text_view = (ECellTextView *) data;
	ECellText *ect = E_CELL_TEXT (((ECellView *)text_view)->ecell);
	CellEdit *edit = text_view->edit;
	ECellTextLineBreaks *linebreaks = cell->breaks;

	gulong current_time;
	gboolean scroll = FALSE;
	gboolean redraw = FALSE;
	
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
	if (scroll && edit->button_down) {
		/* FIXME: Copy this for y. */
		if (edit->lastx - ect->x > cell->width &&
		    edit->xofs_edit < linebreaks->max_width - cell->width) {
			edit->xofs_edit += 4;
			if (edit->xofs_edit > linebreaks->max_width - cell->width + 1)
				edit->xofs_edit = linebreaks->max_width - cell->width + 1;
			redraw = TRUE;
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
			e_tep_event.motion.position = _get_position_from_xy (cell, edit->lastx, edit->lasty);
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
		ect_queue_redraw (text_view, edit->cell.view_col, edit->cell.row);
	}
	return TRUE;
}

static int
_get_position (ECellTextView *text_view, ETextEventProcessorCommand *command)
{
	int length;
	int x, y;
	CellEdit *edit = text_view->edit;
	CurrentCell *cell = CURRENT_CELL(edit);
	EFont *font;
	gchar *p;
	int unival;
	
	font = text_view->font;
	
	switch (command->position) {
		
	case E_TEP_VALUE:
		return command->value;

	case E_TEP_SELECTION:
		return edit->selection_end;

	case E_TEP_START_OF_BUFFER:
		return 0;

		/* fixme: this probably confuses TEP */

	case E_TEP_END_OF_BUFFER:
		return strlen (cell->text);

	case E_TEP_START_OF_LINE:

		if (edit->selection_end < 1) return 0;

		p = unicode_previous_utf8 (cell->text, cell->text + edit->selection_end);

		if (p == cell->text) return 0;

		p = unicode_previous_utf8 (cell->text, p);

		while (p && p > cell->text) {
			if (*p == '\n') return p - cell->text + 1;
			p = unicode_previous_utf8 (cell->text, p);
		}

		return 0;

	case E_TEP_END_OF_LINE:

		length = strlen (cell->text);
		if (edit->selection_end >= length) return length;

		p = unicode_next_utf8 (cell->text + edit->selection_end);

		while (*p) {
			if (*p == '\n') return p - cell->text;
			p = unicode_next_utf8 (p);
		}

		return p - cell->text;

	case E_TEP_FORWARD_CHARACTER:

		length = strlen (cell->text);
		if (edit->selection_end >= length) return length;

		p = unicode_next_utf8 (cell->text + edit->selection_end);

		return p - cell->text;

	case E_TEP_BACKWARD_CHARACTER:

		if (edit->selection_end < 1) return 0;

		p = unicode_previous_utf8 (cell->text, cell->text + edit->selection_end);

		if (p == NULL) return 0;

		return p - cell->text;

	case E_TEP_FORWARD_WORD:

		length = strlen (cell->text);
		if (edit->selection_end >= length) return length;

		p = unicode_next_utf8 (cell->text + edit->selection_end);

		while (*p) {
			unicode_get_utf8 (p, &unival);
			if (unicode_isspace (unival)) return p - cell->text;
			p = unicode_next_utf8 (p);
		}

		return p - cell->text;

	case E_TEP_BACKWARD_WORD:

		if (edit->selection_end < 1) return 0;

		p = unicode_previous_utf8 (cell->text, cell->text + edit->selection_end);

		if (p == cell->text) return 0;

		p = unicode_previous_utf8 (cell->text, p);

		while (p && p > cell->text) {
			unicode_get_utf8 (p, &unival);
			if (unicode_isspace (unival)) {
				return (unicode_next_utf8 (p) - cell->text);
			}
			p = unicode_previous_utf8 (cell->text, p);
		}

		return 0;

	case E_TEP_FORWARD_LINE:
		_get_xy_from_position (cell, edit->selection_end, &x, &y);
		y += e_font_height (font);
		return _get_position_from_xy (cell, x, y);
	case E_TEP_BACKWARD_LINE:
		_get_xy_from_position (cell, edit->selection_end, &x, &y);
		y -= e_font_height (font);
		return _get_position_from_xy (cell, x, y);

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
	CurrentCell *cell = CURRENT_CELL(edit);
	gint length;
	gchar *sp, *ep;

	if (edit->selection_end == edit->selection_start) return;

	if (edit->selection_end < edit->selection_start) {
		edit->selection_end ^= edit->selection_start;
		edit->selection_start ^= edit->selection_end;
		edit->selection_end ^= edit->selection_start;
	}

	sp = cell->text + edit->selection_start;
	ep = cell->text + edit->selection_end;
	length = strlen (ep) + 1;

	memmove (sp, ep, length);

	edit->selection_end = edit->selection_start;
}

/* fixme: */
/* NB! We expect value to be length IN BYTES */

static void
_insert (ECellTextView *text_view, char *string, int value)
{
	CellEdit *edit = text_view->edit;
	CurrentCell *cell = CURRENT_CELL(edit);
	char *temp;

	if (value <= 0) return;

	temp = g_new (gchar, strlen (cell->text) + value + 1);

	strncpy (temp, cell->text, edit->selection_start);
	strncpy (temp + edit->selection_start, string, value);
	strcpy (temp + edit->selection_start + value, cell->text + edit->selection_end);

	g_free (cell->text);

	cell->text = temp;

	edit->selection_start += value;
	edit->selection_end = edit->selection_start;
}

static void
e_cell_text_view_command (ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data)
{
	CellEdit *edit = (CellEdit *) data;
	CurrentCell *cell = CURRENT_CELL(edit);
	ECellTextView *text_view = cell->text_view;

	gboolean change = FALSE;
	gboolean redraw = FALSE;

	int sel_start, sel_end;
	EFont *font;
	
	font = text_view->font;

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
							   cell->text + sel_start,
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
		if (edit->selection_end != edit->selection_start) {
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
							   cell->text + sel_start,
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
		e_table_item_leave_edit (text_view->cell_view.e_table_item_view);
		break;
	case E_TEP_SET_SELECT_BY_WORD:
		edit->select_by_word = command->value;
		break;
	case E_TEP_GRAB:
	case E_TEP_UNGRAB:
#if 0
	case E_TEP_GRAB:
		gnome_canvas_item_grab (GNOME_CANVAS_ITEM(text), 
					GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
					text_view->i_cursor,
					command->time);
		break;
		gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM(text), command->time);
		break;
#endif
	case E_TEP_NOP:
		break;
	}

	if (!edit->button_down) {
		int x;
		int i;
		struct line *lines;
		ECellTextLineBreaks *linebreaks;
		
		split_into_lines (cell);
		
		linebreaks = cell->breaks;
	
		for (lines = linebreaks->lines, i = 0; i < linebreaks->num_lines ; i++, lines ++) {
			if ((lines->text - cell->text) > edit->selection_end) {
				break;
			}
		}
		lines --;
		x = e_font_utf8_text_width (font, E_FONT_PLAIN,
					    lines->text,
					    edit->selection_end - (lines->text - cell->text));
		

		if (x < edit->xofs_edit) {
			edit->xofs_edit = x;
			redraw = TRUE;
		}

		if (2 + x - cell->width > edit->xofs_edit) {
			edit->xofs_edit = 2 + x - cell->width;
			redraw = TRUE;
		}
		unref_lines (cell);
	}

	if (redraw){
		ect_queue_redraw (text_view, edit->cell.view_col, edit->cell.row);
	}
#if 0
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(text));
#endif
}

static void _invisible_destroy (GtkInvisible *invisible,
				CellEdit *edit)
{
	edit->invisible = NULL;
}

static GtkWidget *e_cell_text_view_get_invisible (CellEdit *edit)
{	
	GtkWidget *invisible;
	if (edit->invisible) {
		invisible = edit->invisible;
	} else {
		invisible = gtk_invisible_new ();
		edit->invisible = invisible;
		
		gtk_selection_add_target (invisible,
					  GDK_SELECTION_PRIMARY,
					  GDK_SELECTION_TYPE_STRING,
					  E_SELECTION_PRIMARY);
		gtk_selection_add_target (invisible,
					  clipboard_atom,
					  GDK_SELECTION_TYPE_STRING,
					  E_SELECTION_CLIPBOARD);
		
		gtk_signal_connect (GTK_OBJECT(invisible), "selection_get",
				    GTK_SIGNAL_FUNC (_selection_get), 
				    edit);
		gtk_signal_connect (GTK_OBJECT(invisible), "selection_clear_event",
				    GTK_SIGNAL_FUNC (_selection_clear_event),
				    edit);
		gtk_signal_connect (GTK_OBJECT(invisible), "selection_received",
				    GTK_SIGNAL_FUNC (_selection_received),
				    edit);
		
		gtk_signal_connect (GTK_OBJECT(invisible), "destroy",
				    GTK_SIGNAL_FUNC (_invisible_destroy),
				    edit);
	}
	return invisible;
}

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
		gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING,
					8, edit->primary_selection, edit->primary_length);
		break;
	case E_SELECTION_CLIPBOARD:
		gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING,
					8, edit->clipboard_selection, edit->clipboard_length);
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
	if (selection_data->length < 0 || selection_data->type != GDK_SELECTION_TYPE_STRING) {
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

static void e_cell_text_view_supply_selection (CellEdit *edit, guint time, GdkAtom selection, char *data, gint length)
{
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
}

static void
e_cell_text_view_get_selection (CellEdit *edit, GdkAtom selection, guint32 time)
{
	GtkWidget *invisible;
	invisible = e_cell_text_view_get_invisible (edit);
	gtk_selection_convert (invisible,
			      selection,
			      GDK_SELECTION_TYPE_STRING,
			      time);
}

static void
_get_tep (CellEdit *edit)
{
	if (!edit->tep) {
		edit->tep = e_text_event_processor_emacs_like_new ();
		gtk_object_ref (GTK_OBJECT (edit->tep));
		gtk_object_sink (GTK_OBJECT (edit->tep));
		gtk_signal_connect (GTK_OBJECT(edit->tep),
				   "command",
				   GTK_SIGNAL_FUNC(e_cell_text_view_command),
				   (gpointer) edit);
	}
}

static int
number_of_lines (char *text)
{
	int num_lines = 0;
	gchar *p;

	if (!text) return 0;

	for (p = text; *p; p = unicode_next_utf8 (p)) {
		if (*p == '\n') num_lines++;
	}
	
	num_lines++;
	return num_lines;
}

/* Splits the text of the text item into lines */
static void
split_into_lines (CurrentCell *cell)
{
	char *p;
	struct line *lines;
	gint len;

	char *text = cell->text;
	ECellTextLineBreaks *linebreaks = cell->breaks;

	if (! cell->breaks) {
		cell->breaks = g_new (ECellTextLineBreaks, 1);
		cell->breaks->ref_count = 1;
	} else {
		cell->breaks->ref_count ++;
		return;
	}
	linebreaks = cell->breaks;

	/* Check if already split. */

	linebreaks->lines = NULL;
	linebreaks->num_lines = 0;
	
	if (!text)
		return;
	
	/* First, count the number of lines */

	linebreaks->num_lines = number_of_lines(cell->text);
	
	/* Allocate array of lines and calculate split positions */
	
	linebreaks->lines = lines = g_new0 (struct line, linebreaks->num_lines);

	len = 0;
	for (p = text; *p; p = unicode_next_utf8 (p)) {
		if (len == 0) lines->text = p;
		if (*p == '\n') {
			lines->length = p - lines->text;
			lines++;
			len = 0;
		} else
			len++;
	}
	
	if (len == 0)
		lines->text = p;
	lines->length = p - lines->text;

	calc_line_widths (cell);
}

/* Free lines structure. */
static void
unref_lines (CurrentCell *cell)
{
	if (cell->breaks){
		cell->breaks->ref_count --;
		if (cell->breaks->ref_count <= 0){
			g_free (cell->breaks->lines);
			g_free (cell->breaks);
			cell->breaks = NULL;
		}
	}
}

static void
calc_ellipsis (ECellTextView *text_view)
{
	ECellText *ect = E_CELL_TEXT (((ECellView *)text_view)->ecell);
	EFont *font;
	
	font = text_view->font;
	if (font)
		text_view->ellipsis_width = 
			e_font_utf8_text_width (font, E_FONT_PLAIN,
					ect->ellipsis ? ect->ellipsis : "...",
					ect->ellipsis ? strlen (ect->ellipsis) : 3);
}

/* Calculates the line widths (in pixels) of the text's splitted lines */
static void
calc_line_widths (CurrentCell *cell)
{
	ECellTextView *text_view = cell->text_view;
	ECellText *ect = E_CELL_TEXT (((ECellView *)text_view)->ecell);
	ECellTextLineBreaks *linebreaks = cell->breaks;
	struct line *lines;
	int i;
	int j;
	EFont *font;
	
	font = text_view->font;
	
	lines = linebreaks->lines;
	linebreaks->max_width = 0;

	if (!lines) return;

	for (i = 0; i < linebreaks->num_lines; i++) {
		if (lines->length != 0) {
			if (font) {
				lines->width = e_font_utf8_text_width (font, E_FONT_PLAIN,
								       lines->text, lines->length);
				lines->ellipsis_length = 0;
			} else {
				lines->width = 0;
			}
			
			if (ect->use_ellipsis &&
			    (!(text_view->edit &&
			       cell->row == text_view->edit->cell.row &&
			       cell->view_col == text_view->edit->cell.view_col)) &&
			    lines->width > cell->width) {
				if (font) {
					lines->ellipsis_length = 0;
					for (j = 0; j < lines->length; j++){
						if (e_font_utf8_text_width (font, E_FONT_PLAIN, lines->text, j) +
						    text_view->ellipsis_width <= cell->width)
							lines->ellipsis_length = j;
						else
							break;
					}
				}
				else
					lines->ellipsis_length = 0;
				lines->width = e_font_utf8_text_width (font, E_FONT_PLAIN, lines->text, lines->ellipsis_length) +
					text_view->ellipsis_width;
			}
			else
				lines->ellipsis_length = lines->length;

			if (lines->width > linebreaks->max_width)
				linebreaks->max_width = lines->width;
		} else {
			lines->width = 0;
			lines->ellipsis_length = 0;
		}

		lines++;
	}
}

static void
build_current_cell (CurrentCell *cell, ECellTextView *text_view, int model_col, int view_col, int row)
{
	ECellView *ecell_view = (ECellView *) text_view;
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);

	cell->text_view = text_view;
	cell->model_col = model_col;
	cell->view_col = view_col;
	cell->row = row;
	cell->breaks = NULL;

	if (ect->filter) {
		cell->text = (*ect->filter)(e_table_model_value_at (ecell_view->e_table_model, model_col, row));
	} else {
		cell->text = g_strdup (e_table_model_value_at (ecell_view->e_table_model, model_col, row));
	}
	cell->width = e_table_header_get_column (
		((ETableItem *)ecell_view->e_table_item_view)->header,
		view_col)->width - 8;
}

static void
unbuild_current_cell (CurrentCell *cell)
{
	g_free(cell->text);
	cell->text = NULL;
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

