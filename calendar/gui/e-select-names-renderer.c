/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include "e-util/e-util.h"

#include "e-select-names-editable.h"
#include "e-select-names-renderer.h"

struct _ESelectNamesRendererPriv {
	ESelectNamesEditable *editable;
	gchar *path;

	gchar *name;
	gchar *email;
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_EMAIL
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
	GList *addresses = NULL, *names = NULL, *a, *n;

	/* We don't need to listen for the focus out event any more */
	g_signal_handlers_disconnect_matched (editable, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, cell);

	if (GTK_ENTRY (editable)->editing_canceled) {
		gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (cell), TRUE);
		goto cleanup;
	}

	addresses = e_select_names_editable_get_emails (E_SELECT_NAMES_EDITABLE (editable));
	names = e_select_names_editable_get_names (E_SELECT_NAMES_EDITABLE (editable));

	/* remove empty addresses */
	for (a = addresses, n = names; a && n; ) {
		gchar *addr = a->data, *nm = n->data;

		if ((!addr || !*addr) && (!nm || !*nm)) {
			g_free (addr);
			g_free (nm);
			addresses = g_list_remove_link (addresses, a);
			names = g_list_remove_link (names, n);
			a = addresses;
			n = names;
		} else {
			a = a->next;
			n = n->next;
		}
	}

	g_signal_emit (cell, signals [CELL_EDITED], 0, cell->priv->path, addresses, names);

	g_list_foreach (addresses, (GFunc)g_free, NULL);
	g_list_foreach (names, (GFunc)g_free, NULL);
	g_list_free (addresses);
	g_list_free (names);

 cleanup:
	g_free (cell->priv->path);
	cell->priv->path = NULL;
	cell->priv->editable = NULL;
}

static GtkCellEditable *
e_select_names_renderer_start_editing (GtkCellRenderer *cell, GdkEvent *event, GtkWidget *widget, const gchar *path,
		    GdkRectangle *bg_area, GdkRectangle *cell_area, GtkCellRendererState flags)
{
	ESelectNamesRenderer *sn_cell = E_SELECT_NAMES_RENDERER (cell);
	GtkCellRendererText *text_cell = GTK_CELL_RENDERER_TEXT (cell);
	ESelectNamesEditable *editable;

	if (!text_cell->editable)
		return NULL;

	editable = E_SELECT_NAMES_EDITABLE (e_select_names_editable_new ());
	gtk_entry_set_has_frame (GTK_ENTRY (editable), FALSE);
	gtk_entry_set_alignment (GTK_ENTRY (editable), cell->xalign);
	if (sn_cell->priv->email && *sn_cell->priv->email)
		e_select_names_editable_set_address (editable, sn_cell->priv->name, sn_cell->priv->email);
	gtk_widget_show (GTK_WIDGET (editable));

	g_signal_connect (editable, "editing_done", G_CALLBACK (e_select_names_renderer_editing_done), sn_cell);

	/* Removed focus-out-event. focus out event already listen by base class.
           We don't need to listen for the focus out event any more */

	sn_cell->priv->editable = g_object_ref (editable);
	sn_cell->priv->path = g_strdup (path);

	return GTK_CELL_EDITABLE (editable);
}

static void
e_select_names_renderer_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ESelectNamesRenderer *esnr = E_SELECT_NAMES_RENDERER (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, esnr->priv->name);
		break;
	case PROP_EMAIL:
		g_value_set_string (value, esnr->priv->email);
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
	case PROP_NAME:
		g_free (esnr->priv->name);
		esnr->priv->name = g_strdup (g_value_get_string (value));
		break;
	case PROP_EMAIL:
		g_free (esnr->priv->email);
		esnr->priv->email = g_strdup (g_value_get_string (value));
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
	g_free (cell->priv->name);
	g_free (cell->priv->email);
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

	g_object_class_install_property (obj_class, PROP_NAME,
					 g_param_spec_string ("name", "Name", "Email name.", NULL, G_PARAM_READWRITE));

	g_object_class_install_property (obj_class, PROP_EMAIL,
					 g_param_spec_string ("email", "Email", "Email address.", NULL, G_PARAM_READWRITE));

	signals [CELL_EDITED] = g_signal_new ("cell_edited",
					      G_OBJECT_CLASS_TYPE (obj_class),
					      G_SIGNAL_RUN_LAST,
					      G_STRUCT_OFFSET (ESelectNamesRendererClass, cell_edited),
					      NULL, NULL,
					      e_marshal_VOID__STRING_POINTER_POINTER,
					      G_TYPE_NONE, 3,
					      G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER);
}

GtkCellRenderer *
e_select_names_renderer_new (void)
{
	return GTK_CELL_RENDERER (g_object_new (E_TYPE_SELECT_NAMES_RENDERER, NULL));
}

