/*
 * e-cell-toggle.c: Multi-state image toggle cell object.
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
#include "e-cell-toggle.h"
#include "e-util.h"
#include "e-table-item.h"

#define PARENT_TYPE e_cell_get_type()

typedef struct {
	ECellView    cell_view;
	GdkGC       *gc;
	GnomeCanvas *canvas;
	ETableItem  *eti;
} ECellToggleView;

static ECellClass *parent_class;

static void
etog_queue_redraw (ECellToggleView *text_view, int col, int row)
{
	e_table_item_redraw_range (text_view->eti, col, row, col, row);
}

/*
 * ECell::realize method
 */
static ECellView *
etog_realize (ECell *ecell, void *view)
{
	ECellToggle *eccb = E_CELL_TOGGLE (ecell);
	ECellToggleView *toggle_view = g_new0 (ECellToggleView, 1);
	ETableItem *eti = E_TABLE_ITEM (view);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;
	
	toggle_view->cell_view.ecell = ecell;
	toggle_view->eti = eti;
	toggle_view->canvas = canvas;

	return (ECellView *) toggle_view;
}

/*
 * ECell::unrealize method
 */
static void
etog_unrealize (ECellView *ecv)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecv;

	g_free (toggle_view);
}

/*
 * ECell::draw method
 */
static void
etog_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int col, int row, gboolean selected,
	  int x1, int y1, int x2, int y2)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	GdkPixbuf *image;
	ArtPixBuf *art;
	int x, y, width, height;
	const int value = GPOINTER_TO_INT (
		e_table_model_value_at (ecell_view->ecell->table_model, col, row));

	if (value >= toggle->n_states){
		g_warning ("Value from the table model is %d, the states we support are [0..%d)\n",
			   value, toggle->n_states);
		return;
	}

	image = toggle->images [value];
	art = image->art_pixbuf;

	if ((x2 - x1) < art->width){
		x = x1;
		width = x2 - x1;
	} else {
		x = x1 + ((x2 - x1) - art->width) / 2;
		width = art->width;
	}

	if ((y2 - y1) < art->height){
		y = y1;
		height = y2 - y1;
	} else {
		y = y1 + ((y2 - y1) - art->height) / 2;
		height = art->height;
	}

	width = y2 - y1; 
	gdk_pixbuf_render_to_drawable_alpha (
		image, drawable, 0, 0, x, y,
		width, height,
		GDK_PIXBUF_ALPHA_FULL, 0,
		GDK_RGB_DITHER_NORMAL,
		0, 0);
}

static void
etog_set_value (ECellToggleView *toggle_view, int col, int row, int value)
{
	ECell *ecell = toggle_view->cell_view.ecell;
	ECellToggle *toggle = E_CELL_TOGGLE (ecell);

	if (value >= toggle->n_states)
		value = 0;

	e_table_model_set_value_at (ecell->table_model, col, row, GINT_TO_POINTER (value));
	etog_queue_redraw (toggle_view, col, row);
}

/*
 * ECell::event method
 */
static gint
etog_event (ECellView *ecell_view, GdkEvent *event, int col, int row)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	void *_value = e_table_model_value_at (ecell_view->ecell->table_model, col, row);
	const int value = GPOINTER_TO_INT (_value);
	
	switch (event->type){
	case GDK_BUTTON_RELEASE:
		etog_set_value (toggle_view, col, row, value + 1);
		return TRUE;

	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_space){
			etog_set_value (toggle_view, col, row, value + 1);
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
etog_height (ECellView *ecell_view, int col, int row)
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
	
	ecc->realize    = etog_realize;
	ecc->unrealize  = etog_unrealize;
	ecc->draw       = etog_draw;
	ecc->event      = etog_event;
	ecc->height     = etog_height;

	parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE(e_cell_text, "ECellToggle", ECellToggle, e_cell_toggle_class_init, NULL, PARENT_TYPE);

void
e_cell_toggle_construct (ECellToggle *etog, ETableModel *etm, int border, int n_states, GdkPixbuf **images)
{
	int max_height =  0;
	int i;
	
	E_CELL (etog)->table_model = etm;

	etog->border = border;
	etog->n_states = n_states;

	etog->images = g_new (GdkPixbuf *, n_states);

	for (i = 0; i < n_states; i++){
		etog->images [i] = images [i];
		gdk_pixbuf_ref (images [i]);

		if (images [i]->art_pixbuf->height > max_height)
			max_height = images [i]->art_pixbuf->height;
	}

	etog->height = max_height;
}

ECell *
e_cell_toggle_new (ETableModel *etm, int border, int n_states, GdkPixbuf **images)
{
	ECellToggle *etog = gtk_type_new (e_cell_toggle_get_type ());

	e_cell_toggle_construct (etog, etm, border, n_states, images);

	return (ECell *) etog;
}
