/*
 * e-cell-checkbox.c: Checkbox cell renderer
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
#include "e-cell-checkbox.h"
#include <e-util/e-util.h>
#include "e-table-item.h"

#include "check-empty.xpm"
#include "check-filled.xpm"

#define PARENT_TYPE e_cell_toggle_get_type()

static GdkPixbuf *checks [2];

static void
e_cell_checkbox_class_init (GtkObjectClass *object_class)
{
	checks [0] = gdk_pixbuf_new_from_xpm_data (check_empty_xpm);
	checks [1] = gdk_pixbuf_new_from_xpm_data (check_filled_xpm);
}

E_MAKE_TYPE(e_cell_checkbox, "ECellCheckbox", ECellCheckbox, e_cell_checkbox_class_init, NULL, PARENT_TYPE);

ECell *
e_cell_checkbox_new (void)
{
	ECellCheckbox *eccb = gtk_type_new (e_cell_checkbox_get_type ());

	e_cell_toggle_construct (E_CELL_TOGGLE (eccb), 2, 2, checks);
      
	return (ECell *) eccb;
}
