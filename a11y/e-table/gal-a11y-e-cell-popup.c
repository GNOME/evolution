/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: gal-a11y-e-cell-popup.h
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Yang Wu <yang.wu@sun.com> Sun Microsystem Inc., 2003
 *
 */

#include <config.h>
#include <gal/e-table/e-cell-popup.h>
#include "gal-a11y-e-cell-popup.h"
#include "gal-a11y-e-cell-registry.h"
#include "gal-a11y-util.h"
#include <atk/atkobject.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <glib/gi18n.h>

static AtkObjectClass *parent_class = NULL;
#define PARENT_TYPE (gal_a11y_e_cell_get_type ())

static void gal_a11y_e_cell_popup_class_init (GalA11yECellPopupClass *klass);
static void popup_cell_action (GalA11yECell *cell);

/**
 * gal_a11y_e_cell_popup_get_type:
 * @void: 
 * 
 * Registers the &GalA11yECellPopup class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GalA11yECellPopup class.
 **/
GType
gal_a11y_e_cell_popup_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yECellPopupClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gal_a11y_e_cell_popup_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yECellPopup),
			0,
			(GInstanceInitFunc) NULL,
			NULL /* value_cell_popup */
		};

		type = g_type_register_static (PARENT_TYPE, "GalA11yECellPopup", &info, 0);
		gal_a11y_e_cell_type_add_action_interface (type);
	}

	return type;
}

static void
gal_a11y_e_cell_popup_class_init (GalA11yECellPopupClass *klass)
{
	parent_class = g_type_class_ref (PARENT_TYPE);
}

AtkObject *
gal_a11y_e_cell_popup_new (ETableItem *item,
			  ECellView  *cell_view,
			  AtkObject  *parent,
			  int         model_col,
			  int         view_col,
			  int         row)
{
	AtkObject *a11y;
	GalA11yECell *cell;
 	ECellPopup *popupcell;
	ECellView* child_view = NULL;

	popupcell=  E_CELL_POPUP(cell_view->ecell);

	if (popupcell && popupcell->popup_cell_view)
	        child_view = popupcell->popup_cell_view->child_view;
	
 	if (child_view && child_view->ecell) {
		a11y = gal_a11y_e_cell_registry_get_object (NULL,
							    item,
							    child_view,
							    parent,
							    model_col,
							    view_col,
							    row);
	} else {
		a11y = g_object_new (GAL_A11Y_TYPE_E_CELL_POPUP, NULL);
		gal_a11y_e_cell_construct (a11y,
					   item,
					   cell_view,
					   parent,
					   model_col,
					   view_col,
					   row);
	       	}
	g_return_val_if_fail (a11y != NULL, NULL);
	cell = GAL_A11Y_E_CELL(a11y);
	gal_a11y_e_cell_add_action (cell, 
				    _("popup"),	       /* action name*/
				    _("popup a child"), /* action description */
				    "<Alt>Down",              /* action keybinding */
				    popup_cell_action);

	a11y->role  = ATK_ROLE_TABLE_CELL;
	return a11y;
}

static void
popup_cell_action (GalA11yECell *cell)
{
	gint finished;
	GdkEvent event;

	event.key.type = GDK_KEY_PRESS; 
	event.key.window = GTK_LAYOUT(GNOME_CANVAS_ITEM(cell->item)->canvas)->bin_window;;
	event.key.send_event = TRUE;
	event.key.time = GDK_CURRENT_TIME;
	event.key.state = GDK_MOD1_MASK;
	event.key.keyval = GDK_Down;
  
	g_signal_emit_by_name (cell->item, "event", &event, &finished);
}
