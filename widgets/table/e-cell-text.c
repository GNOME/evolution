/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-cell-text.c - Text cell renderer
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Miguel de Icaza <miguel@helixcode.com>
 *          Chris Lahey <clahey@helixcode.com>
 *         
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
#include "e-cell-text.h"
#include "e-util/e-util.h"
#include "e-table-item.h"
#include "e-text-event-processor-emacs-like.h"

#include <gdk/gdkx.h> /* for BlackPixel */
#include <ctype.h>
#include <math.h>


/* This defines a line of text */
struct line {
	char *text;	/* Line's text, it is a pointer into the text->text string */
	int length;	/* Line's length in characters */
	int width;	/* Line's width in pixels */
	int ellipsis_length;  /* Length before adding ellipsis */
};


/* Object argument IDs */
enum {
	ARG_0,
	ARG_TEXT,
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
	ARG_ELLIPSIS
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
	GdkFont     *font;
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
	gchar         *text;
	gchar         *starting_text; /* the text before the edits */
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
	int selection_start;            /* Start of selection */
	int selection_end;              /* End of selection */
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
	gint primary_length;            /* Primary selection text length */
	gchar *clipboard_selection;     /* Clipboard selection text */
	gint clipboard_length;          /* Clipboard selection text length*/

	guint pointer_in : 1;
	guint default_cursor_shown : 1;
};

static void e_cell_text_view_command (ETextEventProcessor *tep, ETextEventProcessorCommand *command, gpointer data);

static void e_cell_text_view_get_selection (CellEdit *edit, GdkAtom selection, guint32 time);
static void e_cell_text_view_supply_selection (CellEdit *edit, guint time, GdkAtom selection, guchar *data, gint length);

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
static void calc_ellipsis (ECellTextView *text_view);

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

	if (strcmp (cell->starting_text, cell->text))
		e_table_model_set_value_at (text_view->cell_view.e_table_model,
					    cell->model_col, cell->row, cell->text);
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
	g_free (edit->cell.starting_text);
	edit->cell.starting_text = NULL;
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
		GdkFont *f;

		f = gdk_fontset_load (ect->font_name);
		text_view->font = f;
	}
	if (!text_view->font){
		text_view->font = GTK_WIDGET (canvas)->style->font;
		
		gdk_font_ref (text_view->font);
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

	gdk_gc_unref (text_view->gc);
	text_view->gc = NULL;

	if (text_view->edit){
		ect_cancel_edit (text_view);
	}

	if (text_view->font)
		gdk_font_unref (text_view->font);
	
	if (text_view->stipple)
		gdk_bitmap_unref (text_view->stipple);

	gdk_cursor_destroy (text_view->i_cursor);

	if (parent_class->unrealize)
		(* parent_class->unrealize) (ecv);
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
	GdkFont *font = text_view->font;
	const int height = font->ascent + font->descent;
	CellEdit *edit = text_view->edit;
	gboolean edit_display = FALSE;
	ECellTextLineBreaks *linebreaks;
	GdkColor *background, *foreground;


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
		background = &canvas->style->bg [GTK_STATE_SELECTED];
		foreground = &canvas->style->text [GTK_STATE_SELECTED];
	} else {
		background = &canvas->style->base [GTK_STATE_NORMAL];
		foreground = &canvas->style->text [GTK_STATE_NORMAL];
	}
	gdk_gc_set_foreground (text_view->gc, background);
	gdk_draw_rectangle (drawable, text_view->gc, TRUE,
			    rect.x, rect.y, rect.width, rect.height);
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
		ypos += font->ascent;
		ypos -= edit->yofs_edit;

		for (i = 0; i < linebreaks->num_lines; i++) {
			xpos = get_line_xpos (cell, lines);
			xpos -= edit->xofs_edit;
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
				sel_rect.x = xpos + x1 + gdk_text_width (font,
									lines->text,
									sel_start - start_char);
				sel_rect.y = ypos + y1 - font->ascent;
				sel_rect.width = gdk_text_width (font,
								 lines->text + sel_start - start_char,
								 sel_end - sel_start);
				sel_rect.height = height;
				gtk_paint_flat_box (canvas->style,
						   drawable,
						   edit->has_selection ?
						   GTK_STATE_SELECTED :
						   GTK_STATE_ACTIVE,
						   GTK_SHADOW_NONE,
						   clip_rect,
						   canvas,
						   "text",
						   sel_rect.x,
						   sel_rect.y,
						   sel_rect.width,
						   sel_rect.height);
				gdk_draw_text (drawable,
					       font,
					       text_view->gc,
					       xpos + x1,
					       ypos + y1,
					       lines->text,
					       sel_start - start_char);
				gdk_draw_text (drawable,
					       font,
					       fg_gc,
					       xpos + x1 + gdk_text_width (font,
									  lines->text,
									  sel_start - start_char),
					       ypos + y1,
					       lines->text + sel_start - start_char,
					       sel_end - sel_start);
				gdk_draw_text (drawable,
					       font,
					       text_view->gc,
					       xpos + x1 + gdk_text_width (font,
									  lines->text,
									  sel_end - start_char),
					       ypos + y1,
					       lines->text + sel_end - start_char,
					       end_char - sel_end);
			} else {
				gdk_draw_text (drawable,
					       font,
					       text_view->gc,
					       xpos + x1,
					       ypos + y1,
					       lines->text,
					       lines->length);
			}
			if (edit->selection_start == edit->selection_end &&
			    edit->selection_start >= start_char &&
			    edit->selection_start <= end_char &&
			    edit->show_cursor) {
				gdk_draw_rectangle (drawable,
						    text_view->gc,
						    TRUE,
						    xpos + x1 + gdk_text_width (font, 
									       lines->text,
									       sel_start - start_char),
						    ypos + y1 - font->ascent,
						    1,
						    height);
			}
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
		ypos += font->ascent;
		
		
		for (i = 0; i < linebreaks->num_lines; i++) {
			xpos = get_line_xpos (&cell, lines);
			if (ect->use_ellipsis && lines->ellipsis_length < lines->length) {
				gdk_draw_text (drawable,
					       font,
					       text_view->gc,
					       xpos + x1,
					       ypos + y1,
					       lines->text,
					       lines->ellipsis_length);
				gdk_draw_text (drawable,
					       font,
					       text_view->gc,
					       xpos + x1 + 
					       lines->width - text_view->ellipsis_width,
					       ypos + y1,
					       ect->ellipsis ? ect->ellipsis : "...",
					       ect->ellipsis ? strlen (ect->ellipsis) : 3);
			} else {
				gdk_draw_text (drawable,
					       font,
					       text_view->gc,
					       xpos + x1,
					       ypos + y1,
					       lines->text,
					       lines->length);
			}
		}
		
		ypos += height;
		lines++;
		unref_lines (&cell);
		g_free (cell.starting_text);
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
			return TRUE;
		}
		
		if ((!edit_display) && e_table_model_is_cell_editable (ecell_view->e_table_model, view_col, row)) {
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
				e_tep_event.key.length = key.length;
				e_tep_event.key.string = key.string;
				_get_tep (edit);
				return e_text_event_processor_handle_event (edit->tep,
									    &e_tep_event);
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
		    && e_table_model_is_cell_editable (ecell_view->e_table_model, view_col, row)
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

	return (text_view->font->ascent + text_view->font->descent) * number_of_lines(e_table_model_value_at (ecell_view->e_table_model, model_col, row)) + TEXT_PAD;
}

/*
 * ECellView::enter_edit method
 */
static void *
ect_enter_edit (ECellView *ecell_view, int model_col, int view_col, int row)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	const char *str = e_table_model_value_at (ecell_view->e_table_model, model_col, row);
	CellEdit *edit;

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
	
	edit->old_text = g_strdup (str);
	edit->cell.text = g_strdup (str);

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
	} else {
		/*
		 * We did invoke this leave edit internally
		 */
	}
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

	parent_class = gtk_type_class (PARENT_TYPE);

	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
}

E_MAKE_TYPE(e_cell_text, "ECellText", ECellText, e_cell_text_class_init, NULL, PARENT_TYPE);

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

	y = text_view->yofs + ect->y;
	y += (line - lines) * (text_view->font->ascent + text_view->font->descent);

	return y;
}

static void
_get_xy_from_position (CurrentCell *cell, gint position, gint *xp, gint *yp)
{
	if (xp || yp) {
		struct line *lines;
		int x, y;
		int j;
		ECellTextView *text_view = cell->text_view;
		GdkFont *font = text_view->font;
		ECellTextLineBreaks *linebreaks;
		
		split_into_lines (cell);
		
		linebreaks = cell->breaks;
		lines = linebreaks->lines;

		x = get_line_xpos (cell, lines);
		y = get_line_ypos (cell, lines);
		for (j = 0, lines = linebreaks->lines; j < linebreaks->num_lines; lines++, j++) {
			if (lines->text > cell->text + position)
				break;
			y += font->ascent + font->descent;
		}
		lines --;
		y -= font->descent;
		
		x += gdk_text_width (font,
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

	ECellTextView *text_view = cell->text_view;
	GdkFont *font = text_view->font;
	ECellTextLineBreaks *linebreaks;

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
		ypos += font->ascent + font->descent;
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
	for (i = 0; i < lines->length; i++) {
		int charwidth = gdk_text_width (font,
					       lines->text + i,
					       1);
		xpos += charwidth / 2;
		if (xpos > x) {
			break;
		}
		xpos += (charwidth + 1) / 2;
	}
	
	return_val = lines->text + i - cell->text;

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
	int i;
	int length;
	int x, y;
	CellEdit *edit = text_view->edit;
	CurrentCell *cell = CURRENT_CELL(edit);
	
	switch (command->position) {
		
	case E_TEP_VALUE:
		return command->value;

	case E_TEP_SELECTION:
		return edit->selection_end;

	case E_TEP_START_OF_BUFFER:
		return 0;
	case E_TEP_END_OF_BUFFER:
		return strlen (cell->text);

	case E_TEP_START_OF_LINE:
		for (i = edit->selection_end - 2; i > 0; i--)
			if (cell->text[i] == '\n') {
				i++;
				break;
			}
		return i;
	case E_TEP_END_OF_LINE:
		length = strlen (cell->text);
		for (i = edit->selection_end + 1; i < length; i++)
			if (cell->text[i] == '\n') {
				break;
			}
		if (i > length)
			i = length;
		return i;

	case E_TEP_FORWARD_CHARACTER:
		length = strlen (cell->text);
		i = edit->selection_end + 1;
		if (i > length)
			i = length;
		return i;
	case E_TEP_BACKWARD_CHARACTER:
		i = edit->selection_end - 1;
		if (i < 0)
			i = 0;
		return i;

	case E_TEP_FORWARD_WORD:
		length = strlen (cell->text);
		for (i = edit->selection_end + 1; i < length; i++)
			if (isspace (cell->text[i])) {
				break;
			}
		if (i > length)
			i = length;
		return i;
	case E_TEP_BACKWARD_WORD:
		for (i = edit->selection_end - 2; i > 0; i--)
			if (isspace (cell->text[i])) {
				i++;
				break;
			}
		if (i < 0)
			i = 0;
		return i;

	case E_TEP_FORWARD_LINE:
		_get_xy_from_position (cell, edit->selection_end, &x, &y);
		y += text_view->font->ascent + text_view->font->descent;
		return _get_position_from_xy (cell, x, y);
	case E_TEP_BACKWARD_LINE:
		_get_xy_from_position (cell, edit->selection_end, &x, &y);
		y -= text_view->font->ascent + text_view->font->descent;
		return _get_position_from_xy (cell, x, y);

	case E_TEP_FORWARD_PARAGRAPH:
	case E_TEP_BACKWARD_PARAGRAPH:
		
	case E_TEP_FORWARD_PAGE:
	case E_TEP_BACKWARD_PAGE:
		return edit->selection_end;
	default:
		return edit->selection_end;
		}
}

static void
_delete_selection (ECellTextView *text_view)
{
	CellEdit *edit = text_view->edit;
	CurrentCell *cell = CURRENT_CELL(edit);
	gint length = strlen (cell->text);
	if (edit->selection_end == edit->selection_start)
		return;
	if (edit->selection_end < edit->selection_start) {
		edit->selection_end ^= edit->selection_start;
		edit->selection_start ^= edit->selection_end;
		edit->selection_end ^= edit->selection_start;
	}
	memmove (cell->text + edit->selection_start,
		 cell->text + edit->selection_end,
		 length - edit->selection_end + 1);
	length -= edit->selection_end - edit->selection_start;
	edit->selection_end = edit->selection_start;
}

static void
_insert (ECellTextView *text_view, char *string, int value)
{
	if (value > 0) {
		char *temp;
		CellEdit *edit = text_view->edit;
		CurrentCell *cell = CURRENT_CELL(edit);
		gint length = strlen (cell->text);
		temp = g_new (gchar, length + value + 1);
		strncpy (temp, cell->text, edit->selection_start);
		strncpy (temp + edit->selection_start, string, value);
		strcpy (temp + edit->selection_start + value, cell->text + edit->selection_start);
		g_free (cell->text);
		cell->text = temp;
		edit->selection_start += value;
		edit->selection_end = edit->selection_start;
	}
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
			e_cell_text_view_supply_selection (edit, command->time, GDK_SELECTION_PRIMARY, cell->text + sel_start, sel_end - sel_start);
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
			e_cell_text_view_supply_selection (edit, command->time, clipboard_atom, cell->text + sel_start, sel_end - sel_start);
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
			if (lines->text - cell->text > edit->selection_end) {
				break;
			}
		}
		lines --;
		x = gdk_text_width (text_view->font,
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

static void e_cell_text_view_supply_selection (CellEdit *edit, guint time, GdkAtom selection, guchar *data, gint length)
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
	char *p;
	if (!text)
		return 0;
	for (p = text; *p; p++)
		if (*p == '\n')
			num_lines++;
	
	num_lines++;
	return num_lines;
}

/* Splits the text of the text item into lines */
static void
split_into_lines (CurrentCell *cell)
{
	char *p;
	struct line *lines;
	int len;

	gchar *text = cell->text;
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
	
	for (p = text; *p; p++) {
		if (len == 0)
			lines->text = p;
		if (*p == '\n') {
			lines->length = len;
			lines++;
			len = 0;
		} else
			len++;
	}
	
	if (len == 0)
		lines->text = p;
	lines->length = len;
	
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
	if (text_view->font)
		text_view->ellipsis_width = 
			gdk_text_width (text_view->font,
					ect->ellipsis ? ect->ellipsis : "...",
					ect->ellipsis ? strlen (ect->ellipsis) : 3);
}

/* Calculates the line widths (in pixels) of the text's splitted lines */
static void
calc_line_widths (CurrentCell *cell)
{
	ECellTextView *text_view = cell->text_view;
	ECellText *ect = E_CELL_TEXT (((ECellView *)text_view)->ecell);
	GdkFont *font = text_view->font;
	ECellTextLineBreaks *linebreaks = cell->breaks;
	struct line *lines;
	int i;
	int j;
	
	lines = linebreaks->lines;
	linebreaks->max_width = 0;

	if (!lines)
		return;

	for (i = 0; i < linebreaks->num_lines; i++) {
		if (lines->length != 0) {
			if (font) {
				lines->width = gdk_text_width (font,
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
						if (gdk_text_width (font, lines->text, j) + text_view->ellipsis_width <= cell->width)
							lines->ellipsis_length = j;
						else
							break;
					}
				}
				else
					lines->ellipsis_length = 0;
				lines->width = gdk_text_width (font, lines->text, lines->ellipsis_length) +
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

	cell->text_view = text_view;
	cell->model_col = model_col;
	cell->view_col = view_col;
	cell->row = row;
	cell->breaks = NULL;
	cell->text = e_table_model_value_at (ecell_view->e_table_model, model_col, row);
	cell->starting_text = g_strdup(cell->text);
	cell->width = e_table_header_get_column (
		((ETableItem *)ecell_view->e_table_item_view)->header,
		view_col)->width - 8;
}
