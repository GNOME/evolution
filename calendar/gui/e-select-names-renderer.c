/*
 * e-select-names-renderer.c
 *
 * Author:  Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 2003 Ximian Inc.
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
 */

#include <config.h>
#include <gtk/gtkcellrenderertext.h>

#include "e-calendar-marshal.h"

#include "e-select-names-editable.h"
#include "e-select-names-renderer.h"


struct _ESelectNamesRendererPriv {
	ESelectNamesEditable *editable;
	gchar *path;
	gchar *address;
};

enum {
	PROP_0,
	PROP_ADDRESS
};

enum {
	CELL_EDITED,
	LAST_SIGNAL
};

static gint signals [LAST_SIGNAL];

G_DEFINE_TYPE (ESelectNamesRenderer, e_select_names_renderer, GTK_TYPE_CELL_RENDERER_TEXT)

static void
e_select_names_renderer_editing_done (GtkCellEditable *editable, ESelectNamesRenderer *cell)
{
	gchar *new_address, *new_name;
	BonoboControlFrame *cf;

	/* We don't need to listen for the de-activation any more */
	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (editable));
	g_signal_handlers_disconnect_matched (G_OBJECT (cf), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, cell);
	
	new_address = e_select_names_editable_get_address (E_SELECT_NAMES_EDITABLE (editable));
	new_name = e_select_names_editable_get_name (E_SELECT_NAMES_EDITABLE (editable));
                                                             
	g_signal_emit (cell, signals [CELL_EDITED], 0, cell->priv->path, new_address, new_name);
	g_free (new_address);
	g_free (new_name);
	g_free (cell->priv->path);
	cell->priv->path = NULL;
}

static void
e_select_names_renderer_activated (BonoboControlFrame *cf, gboolean activated, ESelectNamesRenderer *cell)
{
	if (!activated)
		e_select_names_renderer_editing_done (GTK_CELL_EDITABLE (cell->priv->editable), cell);
}

static GtkCellEditable *
e_select_names_renderer_start_editing (GtkCellRenderer *cell, GdkEvent *event, GtkWidget *widget, const gchar *path,
		    GdkRectangle *bg_area, GdkRectangle *cell_area, GtkCellRendererState flags)
{
	ESelectNamesRenderer *sn_cell = E_SELECT_NAMES_RENDERER (cell);
	GtkCellRendererText *text_cell = GTK_CELL_RENDERER_TEXT (cell);
	ESelectNamesEditable *editable;
	BonoboControlFrame *cf;
	
	if (!text_cell->editable)
		return NULL;

	editable = E_SELECT_NAMES_EDITABLE (e_select_names_editable_new ());
	e_select_names_editable_set_address (editable, sn_cell->priv->address);
	gtk_widget_show (GTK_WIDGET (editable));

	g_signal_connect (editable, "editing_done", G_CALLBACK (e_select_names_renderer_editing_done), sn_cell);

	/* Listen for de-activation/loss of focus */
	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (editable));
	bonobo_control_frame_set_autoactivate (cf, TRUE);

	g_signal_connect (cf, "activated", G_CALLBACK (e_select_names_renderer_activated), sn_cell);

	sn_cell->priv->editable = g_object_ref (editable);
	sn_cell->priv->path = g_strdup (path);

	return GTK_CELL_EDITABLE (editable);
}

static void
e_select_names_renderer_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ESelectNamesRenderer *esnr = E_SELECT_NAMES_RENDERER (object);

	switch (prop_id) {
	case PROP_ADDRESS:
		g_value_set_string (value, esnr->priv->address);
		break;
	default:
		break;
	}
}

static void
e_select_names_renderer_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ESelectNamesRenderer *esnr = E_SELECT_NAMES_RENDERER (object);

	switch (prop_id) {
	case PROP_ADDRESS:
		g_free (esnr->priv->address);
		esnr->priv->address = g_strdup (g_value_get_string (value));
		break;
	default:
		break;
	}
}

static void
e_select_names_renderer_finalize (GObject *obj)
{
	ESelectNamesRenderer *cell = (ESelectNamesRenderer *) obj;

	if (cell->priv->editable)
		g_object_unref (cell->priv->editable);
	cell->priv->editable = NULL;

	g_free (cell->priv->path);
	g_free (cell->priv->address);
	g_free (cell->priv);

	if (G_OBJECT_CLASS (e_select_names_renderer_parent_class)->finalize)
		G_OBJECT_CLASS (e_select_names_renderer_parent_class)->finalize (obj);
}

static void
e_select_names_renderer_init (ESelectNamesRenderer *cell)
{
	cell->priv = g_new0 (ESelectNamesRendererPriv, 1);
}

static void
e_select_names_renderer_class_init (ESelectNamesRendererClass *class)
{
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);
	GObjectClass *obj_class = G_OBJECT_CLASS (class);
	
	obj_class->finalize = e_select_names_renderer_finalize;
	obj_class->get_property = e_select_names_renderer_get_property;
	obj_class->set_property = e_select_names_renderer_set_property;

	cell_class->start_editing = e_select_names_renderer_start_editing;

	g_object_class_install_property (obj_class, PROP_ADDRESS,
					 g_param_spec_string ("address", "Address", "Email address.", NULL, G_PARAM_READWRITE));

	signals [CELL_EDITED] = g_signal_new ("cell_edited",
					      G_OBJECT_CLASS_TYPE (obj_class),
					      G_SIGNAL_RUN_LAST,
					      G_STRUCT_OFFSET (ESelectNamesRendererClass, cell_edited),
					      NULL, NULL,
					      e_calendar_marshal_VOID__STRING_STRING_STRING,
					      G_TYPE_NONE, 3,
					      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

GtkCellRenderer *
e_select_names_renderer_new (void)
{
	return GTK_CELL_RENDERER (g_object_new (E_TYPE_SELECT_NAMES_RENDERER, NULL));
}

