/*
 * e-cell-text.c: Text cell renderer
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 1999 Helix Code, Inc
 */
#include <config.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomeui/gnome-canvas.h>
#include <stdio.h>
#include "e-cell-text.h"
#include "e-util.h"
#include "e-table-item.h"

#define PARENT_TYPE e_cell_get_type()

#define TEXT_PAD 2

typedef struct {
	char         *old_text;
	GtkWidget    *entry_top;
	GtkEntry     *entry;

	/*
	 * Where the editing is taking place
	 */
	int           model_col, view_col, row;
} CellEdit;

typedef struct {
	ECellView    cell_view;
	GdkGC       *gc;
	GdkFont     *font;
	GnomeCanvas *canvas;
	ETableItem  *eti;
	
	/*
	 * During edition.
	 */
	CellEdit    *edit;
} ECellTextView;

static ECellClass *parent_class;

static void
ect_queue_redraw (ECellTextView *text_view, int view_col, int view_row)
{
	e_table_item_redraw_range (text_view->eti, view_col, view_row, view_col, view_row);
}

/*
 * Accept the currently edited text
 */
static void
ect_accept_edits (ECellTextView *text_view)
{
	const char *text = gtk_entry_get_text (text_view->edit->entry);
	CellEdit *edit = text_view->edit;
	
	e_table_model_set_value_at (text_view->cell_view.table_model, edit->model_col, edit->row, text);
}

/*
 * Shuts down the editing process
 */
static void
ect_stop_editing (ECellTextView *text_view)
{
	CellEdit *edit = text_view->edit;
	
	g_free (edit->old_text);
	edit->old_text = NULL;
	gtk_widget_destroy (edit->entry_top);
	edit->entry_top = NULL;
	edit->entry = NULL;

	g_free (edit);
	
	text_view->edit = NULL;

	e_table_item_leave_edit (text_view->eti);
}

/*
 * Cancels the edits
 */
static void
ect_cancel_edit (ECellTextView *text_view)
{
	ect_queue_redraw (text_view, text_view->edit->view_col, text_view->edit->row);
	ect_stop_editing (text_view);
}

/*
 * ECell::realize method
 */
static ECellView *
ect_realize (ECell *ecell, ETableModel *table_model, void *view)
{
	ECellText *ect = E_CELL_TEXT (ecell);
	ECellTextView *text_view = g_new0 (ECellTextView, 1);
	ETableItem *eti = E_TABLE_ITEM (view);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;
	
	text_view->cell_view.ecell = ecell;
	text_view->cell_view.table_model = table_model;
	
	text_view->gc = gdk_gc_new (GTK_WIDGET (canvas)->window);
	if (ect->font_name){
		GdkFont *f;

		f = gdk_fontset_load (ect->font_name);
		text_view->font = f;
	}
	if (!text_view->font){
		text_view->font = GTK_WIDGET (canvas)->style->font;
		
		gdk_font_ref (text_view->font);
	}

	text_view->eti = eti;
	text_view->canvas = canvas;
	
	return (ECellView *)text_view;
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

	gdk_font_unref (text_view->font);
	text_view->font = NULL;

	g_free (text_view);
}

/*
 * ECell::draw method
 */
static void
ect_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int model_col, int view_col, int row, gboolean selected,
	  int x1, int y1, int x2, int y2)
{
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	GtkWidget *w = GTK_WIDGET (text_view->canvas);
	GdkRectangle rect;
	const char *str = e_table_model_value_at (ecell_view->table_model, model_col, row);
	GdkFont *font = text_view->font;
	const int height = font->ascent + font->descent;
	int xoff;
	gboolean edit_display = FALSE;

	/*
	 * Figure if this cell is being edited
	 */
	if (text_view->edit){
		CellEdit *edit = text_view->edit;
		
		if ((edit->view_col == view_col) && (edit->row == row))
			edit_display = TRUE;
	}

	/*
	 * Be a nice citizen: clip to the region we are supposed to draw on
	 */
	rect.x = x1;
	rect.y = y1;
	rect.width = x2 - x1;
	rect.height = y2 - y1;
	gdk_gc_set_clip_rectangle (text_view->gc, &rect);

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
}

/*
 * Selects the entire string
 */
static void
ect_edit_select_all (ECellTextView *text_view)
{
	g_assert (text_view->edit);
	
	gtk_editable_select_region (GTK_EDITABLE (text_view->edit->entry), 0, -1);
}

/*
 * ECell::event method
 */
static gint
ect_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	
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
			e_table_item_enter_edit (text_view->eti, view_col, row);
		break;

	case GDK_BUTTON_RELEASE:
		/*
		 * Adjust for the border we use
		 */
		event->button.x++;
		printf ("Button released at %g %g\n", event->button.x, event->button.y);
		return TRUE;

	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_Escape){
			ect_cancel_edit (text_view);
			return TRUE;
		}
		
		if (!text_view->edit){
			e_table_item_enter_edit (text_view->eti, view_col, row);
			ect_edit_select_all (text_view);
		}

		gtk_widget_event (GTK_WIDGET (text_view->edit->entry), event);
		ect_queue_redraw (text_view, view_col, row);
		break;
		
	case GDK_KEY_RELEASE:
		break;
		
	default:
		return FALSE;
	}
	return TRUE;
}

/*
 * ECell::height method
 */
static int
ect_height (ECellView *ecell_view, int model_col, int view_col, int row) 
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	
	return (text_view->font->ascent + text_view->font->descent) + TEXT_PAD;
}

/*
 * Callback: invoked when the user pressed "enter" on the GtkEntry
 */
static void
ect_entry_activate (GtkEntry *entry, ECellTextView *text_view)
{
	e_table_item_leave_edit (text_view->eti);
}

/*
 * ECellView::enter_edit method
 */
static void *
ect_enter_edit (ECellView *ecell_view, int model_col, int view_col, int row)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	const char *str = e_table_model_value_at (ecell_view->table_model, model_col, row);
	CellEdit *edit;

	edit = g_new (CellEdit, 1);
	text_view->edit = edit;

	edit->model_col = model_col;
	edit->view_col = view_col;
	edit->row = row;
	
	edit->entry = (GtkEntry *) gtk_entry_new ();
	gtk_entry_set_text (edit->entry, str);
	edit->old_text = g_strdup (str);
	gtk_signal_connect (GTK_OBJECT (edit->entry), "activate",
			    GTK_SIGNAL_FUNC (ect_entry_activate), text_view);

	/*
	 * The hack: create this window off-screen
	 */
	edit->entry_top = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_container_add (GTK_CONTAINER (edit->entry_top), GTK_WIDGET (edit->entry));
	gtk_widget_set_uposition (edit->entry_top, 20000, 20000);
	gtk_widget_show_all (edit->entry_top);

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

	if (text_view->edit){
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
	
	ecc->realize    = ect_realize;
	ecc->unrealize  = ect_unrealize;
	ecc->draw       = ect_draw;
	ecc->event      = ect_event;
	ecc->height     = ect_height;
	ecc->enter_edit = ect_enter_edit;
	ecc->leave_edit = ect_leave_edit;

	parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE(e_cell_text, "ECellText", ECellText, e_cell_text_class_init, NULL, PARENT_TYPE);

ECell *
e_cell_text_new (ETableModel *etm, const char *fontname, GtkJustification justify)
{
	ECellText *ect = gtk_type_new (e_cell_text_get_type ());

	ect->font_name = g_strdup (fontname);
	ect->justify = justify;
      
	return (ECell *) ect;
}
