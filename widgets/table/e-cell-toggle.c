/*
 * e-cell-toggle.c: Multi-state image toggle cell object.
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 1999, 2000 Helix Code, Inc
 */
#include <config.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkkeysyms.h>
#include <libgnomeui/gnome-canvas.h>
#include "e-cell-toggle.h"
#include "e-util/e-util.h"
#include "e-table-item.h"

#define PARENT_TYPE e_cell_get_type ()

typedef struct {
	ECellView    cell_view;
	GdkGC       *gc;
	GnomeCanvas *canvas;
} ECellToggleView;

static ECellClass *parent_class;

static void
etog_queue_redraw (ECellToggleView *text_view, int view_col, int view_row)
{
	e_table_item_redraw_range (
		text_view->cell_view.e_table_item_view,
		view_col, view_row, view_col, view_row);
}

/*
 * ECell::realize method
 */
static ECellView *
etog_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	ECellToggleView *toggle_view = g_new0 (ECellToggleView, 1);
	ETableItem *eti = E_TABLE_ITEM (e_table_item_view);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;
	
	toggle_view->cell_view.ecell = ecell;
	toggle_view->cell_view.e_table_model = table_model;
	toggle_view->cell_view.e_table_item_view = e_table_item_view;
	toggle_view->canvas = canvas;
	
	return (ECellView *) toggle_view;
}

static void
etog_kill_view (ECellView *ecell_view)
{
	g_free (ecell_view);
}	

static void
etog_realize (ECellView *ecell_view)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;

	toggle_view->gc = gdk_gc_new (GTK_WIDGET (toggle_view->canvas)->window);
}

/*
 * ECell::unrealize method
 */
static void
etog_unrealize (ECellView *ecv)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecv;

	gdk_gc_unref (toggle_view->gc);
	toggle_view->gc = NULL;
}

/*
 * ECell::draw method
 */
static void
etog_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int model_col, int view_col, int row, gboolean selected,
	  int x1, int y1, int x2, int y2)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	GdkPixbuf *image;
	int x, y, width, height;
	const int value = GPOINTER_TO_INT (
		 e_table_model_value_at (ecell_view->e_table_model, model_col, row));

	if (value >= toggle->n_states){
		g_warning ("Value from the table model is %d, the states we support are [0..%d)\n",
			   value, toggle->n_states);
		return;
	}

	/*
	 * Paint the background
	 */
	gdk_draw_rectangle (drawable, GTK_WIDGET (toggle_view->canvas)->style->white_gc, TRUE, x1, y1, x2 - x1, y2 - y1);
			    
	image = toggle->images [value];

	if ((x2 - x1) < gdk_pixbuf_get_width (image)){
		x = x1;
		width = x2 - x1;
	} else {
		x = x1 + ((x2 - x1) - gdk_pixbuf_get_width (image)) / 2;
		width = gdk_pixbuf_get_width (image);
	}

	if ((y2 - y1) < gdk_pixbuf_get_height (image)){
		y = y1;
		height = y2 - y1;
	} else {
		y = y1 + ((y2 - y1) - gdk_pixbuf_get_height (image)) / 2;
		height = gdk_pixbuf_get_height (image);
	}

#if 0 /* do alpha */
	if (gdk_pixbuf_get_has_alpha (image)) {
		flat = gdk_pixbuf_composite_color_simple (
			image,
			gdk_pixbuf_get_width (image),
			gdk_pixbuf_get_height (image),
			GDK_INTERP_NEAREST,
			255,
			32,
			0xffffff, 0xffffff);	
		
		gdk_pixbuf_render_to_drawable (flat, drawable,
					       toggle_view->gc,
					       0, 0,
					       x, y,
					       width, height,
					       GDK_RGB_DITHER_NORMAL,
					       0, 0);
		gdk_pixbuf_unref (flat);
	} else {
		gdk_pixbuf_render_to_drawable (image, drawable,
					       toggle_view->gc,
					       0, 0,
					       x, y,
					       width, height,
					       GDK_RGB_DITHER_NORMAL,
					       0, 0);
	}
#else 
	gdk_pixbuf_render_to_drawable_alpha (image, drawable,
					     0, 0,
					     x, y,
					     width, height,
					     GDK_PIXBUF_ALPHA_BILEVEL,
					     128,
					     GDK_RGB_DITHER_NORMAL,
					     x, y);
#endif
}

static void
etog_set_value (ECellToggleView *toggle_view, int model_col, int view_col, int row, int value)
{
	ECell *ecell = toggle_view->cell_view.ecell;
	ECellToggle *toggle = E_CELL_TOGGLE (ecell);

	if (value >= toggle->n_states)
		value = 0;

	e_table_model_set_value_at (toggle_view->cell_view.e_table_model,
				    model_col, row, GINT_TO_POINTER (value));
	etog_queue_redraw (toggle_view, view_col, row);
}

/*
 * ECell::event method
 */
static gint
etog_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	void *_value = e_table_model_value_at (ecell_view->e_table_model, model_col, row);
	const int value = GPOINTER_TO_INT (_value);
	
	switch (event->type){
	case GDK_BUTTON_RELEASE:
		if (!e_table_model_is_cell_editable(ecell_view->e_table_model, model_col, row))
			return FALSE;
		
		etog_set_value (toggle_view, model_col, view_col, row, value + 1);
		return TRUE;

	case GDK_KEY_PRESS:
		if (!e_table_model_is_cell_editable(ecell_view->e_table_model, model_col, row))
			return FALSE;
		
		if (event->key.keyval == GDK_space){
			etog_set_value (toggle_view, model_col, view_col, row, value + 1);
			return TRUE;
		}
		return FALSE;
		
	default:
		return FALSE;
	}
	return TRUE;
}

/*
 * ECell::height method
 */
static int
etog_height (ECellView *ecell_view, int model_col, int view_col, int row)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);

	return toggle->height;
}

static void
etog_destroy (GtkObject *object)
{
	ECellToggle *etog = E_CELL_TOGGLE (object);
	int i;
	
	for (i = 0; i < etog->n_states; i++)
		gdk_pixbuf_unref (etog->images [i]);

	g_free (etog->images);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
e_cell_toggle_class_init (GtkObjectClass *object_class)
{
	ECellClass *ecc = (ECellClass *) object_class;

	object_class->destroy = etog_destroy;

	ecc->new_view   = etog_new_view;
	ecc->kill_view  = etog_kill_view;
	ecc->realize    = etog_realize;
	ecc->unrealize  = etog_unrealize;
	ecc->draw       = etog_draw;
	ecc->event      = etog_event;
	ecc->height     = etog_height;

	parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE(e_cell_toggle, "ECellToggle", ECellToggle, e_cell_toggle_class_init, NULL, PARENT_TYPE);

void
e_cell_toggle_construct (ECellToggle *etog, int border, int n_states, GdkPixbuf **images)
{
	int max_height =  0;
	int i;
	
	etog->border = border;
	etog->n_states = n_states;

	etog->images = g_new (GdkPixbuf *, n_states);

	for (i = 0; i < n_states; i++){
		etog->images [i] = images [i];
		gdk_pixbuf_ref (images [i]);

		if (gdk_pixbuf_get_height (images [i]) > max_height)
		  max_height = gdk_pixbuf_get_height (images [i]);
	}

	etog->height = max_height;
}

ECell *
e_cell_toggle_new (int border, int n_states, GdkPixbuf **images)
{
	ECellToggle *etog = gtk_type_new (e_cell_toggle_get_type ());

	e_cell_toggle_construct (etog, border, n_states, images);

	return (ECell *) etog;
}


