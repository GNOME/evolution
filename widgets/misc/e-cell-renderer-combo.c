/*
 * e-cell-renderer-combo.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkbutton.h>

#include "e-combo-cell-editable.h"
#include "e-cell-renderer-combo.h"

enum {
	PROP_0,
	PROP_LIST
};

struct _ECellRendererComboPriv {
	EComboCellEditable *editable;
	gchar *path;
	GList *list;
};

G_DEFINE_TYPE (ECellRendererCombo, e_cell_renderer_combo, GTK_TYPE_CELL_RENDERER_TEXT)

static void
ecrc_editing_done (GtkCellEditable *editable, ECellRendererCombo *cell)
{
	const gchar *new_text;

	if (e_combo_cell_editable_cancelled (E_COMBO_CELL_EDITABLE (editable)))
		return;

	new_text = e_combo_cell_editable_get_text (E_COMBO_CELL_EDITABLE (editable));
                                                             
	g_signal_emit_by_name (cell, "edited", cell->priv->path, new_text);
	g_free (cell->priv->path);
	cell->priv->path = NULL;
}

static GtkCellEditable *
ecrc_start_editing (GtkCellRenderer *cell, GdkEvent *event, GtkWidget *widget, const gchar *path,
		    GdkRectangle *bg_area, GdkRectangle *cell_area, GtkCellRendererState flags)
{
	ECellRendererCombo *combo_cell = E_CELL_RENDERER_COMBO (cell);
	GtkCellRendererText *text_cell = GTK_CELL_RENDERER_TEXT (cell);
	EComboCellEditable *editable;

	if (!text_cell->editable)
		return NULL;

	editable = E_COMBO_CELL_EDITABLE (e_combo_cell_editable_new ());
	e_combo_cell_editable_set_text (editable, text_cell->text);
	e_combo_cell_editable_set_list (editable, combo_cell->priv->list);
	gtk_widget_show (GTK_WIDGET (editable));

	g_signal_connect (editable, "editing-done", G_CALLBACK (ecrc_editing_done), combo_cell);

	combo_cell->priv->editable = g_object_ref (editable);
	combo_cell->priv->path = g_strdup (path);

	return GTK_CELL_EDITABLE (editable);
}

static void
ecrc_get_size (GtkCellRenderer *cell, GtkWidget *widget, GdkRectangle *cell_area, 
	       gint *x_offset, gint *y_offset, gint *width, gint *height)
{
	GtkWidget *btn;
	GtkRequisition req;

	if (GTK_CELL_RENDERER_CLASS (e_cell_renderer_combo_parent_class)->get_size)
		GTK_CELL_RENDERER_CLASS (e_cell_renderer_combo_parent_class)->get_size (cell, widget, cell_area, x_offset, y_offset, width, height);

	btn = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (btn), gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE));
	gtk_widget_size_request (btn, &req);
	*width += req.width;
	gtk_widget_destroy (btn);
}

static void
ecrc_get_prop (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ECellRendererCombo *ecrc = E_CELL_RENDERER_COMBO (object);

	switch (prop_id) {
	case PROP_LIST:
		g_value_set_pointer (value, ecrc->priv->list);
		break;
	default:
		break;
	}
}

static void
ecrc_set_prop (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ECellRendererCombo *ecrc = E_CELL_RENDERER_COMBO (object);

	switch (prop_id) {
	case PROP_LIST:
		ecrc->priv->list = g_value_get_pointer (value);
		break;
	default:
		break;
	}
}

static void
ecrc_finalize (GObject *obj)
{
	ECellRendererCombo *cell = (ECellRendererCombo *) obj;

	if (cell->priv->editable)
		g_object_unref (cell->priv->editable);
	cell->priv->editable = NULL;

	if (cell->priv->path)
		g_free (cell->priv->path);
	cell->priv->path = NULL;

	g_free (cell->priv);

	if (G_OBJECT_CLASS (e_cell_renderer_combo_parent_class)->finalize)
		G_OBJECT_CLASS (e_cell_renderer_combo_parent_class)->finalize (obj);
}

static void
e_cell_renderer_combo_init (ECellRendererCombo *cell)
{
	cell->priv = g_new0 (ECellRendererComboPriv, 1);
}

static void
e_cell_renderer_combo_class_init (ECellRendererComboClass *class)
{
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);
	GObjectClass *obj_class = G_OBJECT_CLASS (class);
	
	obj_class->get_property = ecrc_get_prop;
	obj_class->set_property = ecrc_set_prop;
	obj_class->finalize = ecrc_finalize;

	cell_class->start_editing = ecrc_start_editing;
	cell_class->get_size = ecrc_get_size;

	g_object_class_install_property (obj_class, PROP_LIST,
					 g_param_spec_pointer ("list", "List", "List of items to popup.", G_PARAM_READWRITE));
}

GtkCellRenderer *
e_cell_renderer_combo_new (void)
{
	return GTK_CELL_RENDERER (g_object_new (E_TYPE_CELL_RENDERER_COMBO, NULL));
}

