/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Iain Holmes <iain@ximian.com>
 *
 *  Copyright 2002 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gal/util/e-util.h>
#include <gal/e-table/e-table-item.h>

#include "e-cell-tri.h"

#include "check-none.xpm"
#include "check-empty.xpm"
#include "check-filled.xpm"

#define PARENT_TYPE e_cell_toggle_get_type ()

static GdkPixbuf *checks[3];

static void
set_value (ECellView *view,
	   int model_col,
	   int view_col,
	   int row,
	   int value)
{
	ECell *ecell = view->ecell;
	ECellToggle *toggle = E_CELL_TOGGLE (ecell);

	if (value >= toggle->n_states) {
		g_print ("Value 2: %d\n", value);
		value = 1;
	}

	e_table_model_set_value_at (view->e_table_model,
				    model_col, row, GINT_TO_POINTER (value));
}

static gint
event (ECellView *ecell_view,
       GdkEvent *event,
       int model_col,
       int view_col,
       int row,
       ECellFlags flags,
       ECellActions *actions)
{
	void *_value = e_table_model_value_at (ecell_view->e_table_model, model_col, row);
	const int value = GPOINTER_TO_INT (_value);

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event->key.keyval != GDK_space) {
			return FALSE;
		}
		/* Fall through */
	case GDK_BUTTON_PRESS:
		if (e_table_model_is_cell_editable (ecell_view->e_table_model, model_col, row) == FALSE) {
			return FALSE;
		}
		if (value == 0) {
			return FALSE;
		}

		set_value (ecell_view, model_col, view_col, row, value + 1);
		return TRUE;

	default:
		return FALSE;
	}

	return TRUE;
}

static void
e_cell_tri_class_init (GtkObjectClass *object_class)
{
	ECellClass *e_cell_class = E_CELL_CLASS (object_class);

	e_cell_class->event = event;
	
	checks[0] = gdk_pixbuf_new_from_xpm_data (check_none_xpm);
	checks[1] = gdk_pixbuf_new_from_xpm_data (check_empty_xpm);
	checks[2] = gdk_pixbuf_new_from_xpm_data (check_filled_xpm);
	
}

E_MAKE_TYPE (e_cell_tri, "ECellTri", ECellTri, e_cell_tri_class_init, NULL, PARENT_TYPE);

ECell *
e_cell_tri_new (void)
{
	ECellTri *ect = gtk_type_new (e_cell_tri_get_type ());

	e_cell_toggle_construct (E_CELL_TOGGLE (ect), 2, 3, checks);

	return (ECell *) ect;
}

