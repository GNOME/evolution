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
#include "e-cell-text.h"
#include "e-util.h"

#define PARENT_TYPE e_cell_get_type()

#define TEXT_PAD 2

typedef struct {
	ECellView    cell_view;
	GdkGC       *gc;
	GdkFont     *font;
	GnomeCanvas *canvas;
} ECellTextView;

static ECellClass *parent_class;

static ECellView *
ect_realize (ECell *ecell, GnomeCanvas *canvas)
{
	ECellText *ect = E_CELL_TEXT (ecell);
	ECellTextView *ectv = g_new0 (ECellTextView, 1);

	ectv->cell_view.ecell = ecell;
	ectv->gc = gdk_gc_new (GTK_WIDGET (canvas)->window);
	if (ect->font_name){
		GdkFont *f;

		f = gdk_fontset_load (ect->font_name);
		ectv->font = f;
	}
	if (!ectv->font){
		ectv->font = GTK_WIDGET (canvas)->style->font;
		
		gdk_font_ref (ectv->font);
	}
	
	ectv->canvas = canvas;

	return (ECellView *)ectv;
}

static void
ect_unrealize (ECellView *ecv)
{
	ECellTextView *ectv = (ECellTextView *) ecv;

	gdk_gc_unref (ectv->gc);
	ectv->gc = NULL;

	gdk_font_unref (ectv->font);
	ectv->font = NULL;

	g_free (ectv);
}

static void
ect_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int col, int row, gboolean selected,
	  int x1, int y1, int x2, int y2)
{
	ECellText *ect = E_CELL_TEXT (ecell_view->ecell);
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	GdkRectangle rect;
	const char *str = e_table_model_value_at (ecell_view->ecell->table_model, col, row);
	int xoff, w;
		
	rect.x = x1;
	rect.y = y1;
	rect.width = x2 - x1;
	rect.height = y2 - y1;
	
	gdk_gc_set_clip_rectangle (text_view->gc, &rect);

	switch (ect->justify){
	case GTK_JUSTIFY_LEFT:
		xoff = 1;
		break;
		
	case GTK_JUSTIFY_RIGHT:
		w = 1 + gdk_text_width (text_view->font, str, strlen (str));
		xoff = (x2 - x1) - w;
		break;
		
	case GTK_JUSTIFY_CENTER:
		xoff = ((x2 - x1) - gdk_text_width (text_view->font, str, strlen (str))) / 2;
		break;
	default:
		xoff = 0;
		g_warning ("Can not handle GTK_JUSTIFY_FILL");
		break;
	}

	/* Draw now */
	{
		GtkWidget *w = GTK_WIDGET (text_view->canvas);
		GdkColor *background, *foreground;
		const int height = text_view->font->ascent + text_view->font->descent;
		
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
		gdk_draw_string (drawable, text_view->font, text_view->gc,
				 x1 + xoff, y2 - text_view->font->descent - ((y2-y1-height)/2), str);
	}
}

static void
e_cell_text_start_editing (ECellText *ect, int col, int row)
{
	printf ("Starting to edit %d %d\n", col, row);
}

static gint
ect_event (ECellView *ecell_view, GdkEvent *event, int col, int row)
{
	ECell *ecell = ecell_view->ecell;
	ECellText *ect = E_CELL_TEXT (ecell);
	
	switch (event->type){
	case GDK_BUTTON_PRESS:
		return TRUE;

	default:
		break;
	}
	return FALSE;
}

static int
ect_height (ECellView *ecell_view, int col, int row)
{
	ECellTextView *text_view = (ECellTextView *) ecell_view;
	
	return (text_view->font->ascent + text_view->font->descent) + TEXT_PAD;
}

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
	
	ecc->realize = ect_realize;
	ecc->unrealize = ect_unrealize;
	ecc->draw = ect_draw;
	ecc->event = ect_event;
	ecc->height = ect_height;
	
	parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE(e_cell_text, "ECellText", ECellText, e_cell_text_class_init, NULL, PARENT_TYPE);

ECell *
e_cell_text_new (ETableModel *etm, const char *fontname, GtkJustification justify)
{
	ECellText *ect = gtk_type_new (e_cell_text_get_type ());

	ect->font_name = g_strdup (fontname);
	ect->justify = justify;
	E_CELL (ect)->table_model = etm;
      
	return (ECell *) ect;
}
