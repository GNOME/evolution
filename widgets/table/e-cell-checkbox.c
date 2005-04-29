/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell-checkbox.c: Checkbox cell renderer
 * Copyright 1999, 2000, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>

#include "gal/util/e-util.h"

#include "e-table-item.h"
#include "e-cell-checkbox.h"

#include "check-empty.xpm"
#include "check-filled.xpm"

#define PARENT_TYPE e_cell_toggle_get_type ()

static GdkPixbuf *checks [2];

static void
e_cell_checkbox_class_init (GtkObjectClass *object_class)
{
	checks [0] = gdk_pixbuf_new_from_xpm_data (check_empty_xpm);
	checks [1] = gdk_pixbuf_new_from_xpm_data (check_filled_xpm);
}

E_MAKE_TYPE(e_cell_checkbox, "ECellCheckbox", ECellCheckbox, e_cell_checkbox_class_init, NULL, PARENT_TYPE)

/**
 * e_cell_checkbox_new:
 *
 * Creates a new ECell renderer that can be used to render check
 * boxes.  the data provided from the model is cast to an integer.
 * zero is used for the off display, and non-zero for checked status.
 * 
 * Returns: an ECell object that can be used to render checkboxes.
 */
ECell *
e_cell_checkbox_new (void)
{
	ECellCheckbox *eccb = g_object_new (E_CELL_CHECKBOX_TYPE, NULL);

	e_cell_toggle_construct (E_CELL_TOGGLE (eccb), 2, 2, checks);
      
	return (ECell *) eccb;
}
