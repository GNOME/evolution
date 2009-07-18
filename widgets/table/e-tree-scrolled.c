/*
 *
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libgnomecanvas/gnome-canvas.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"

#include "e-tree-scrolled.h"

#define COLUMN_HEADER_HEIGHT 16

G_DEFINE_TYPE (ETreeScrolled, e_tree_scrolled, GTK_TYPE_SCROLLED_WINDOW)

enum {
	PROP_0,
	PROP_TREE
};

static void
e_tree_scrolled_init (ETreeScrolled *ets)
{
	GtkScrolledWindow *scrolled_window;

	scrolled_window = GTK_SCROLLED_WINDOW (ets);

	GTK_WIDGET_SET_FLAGS (ets, GTK_CAN_FOCUS);

	ets->tree = g_object_new (E_TREE_TYPE, NULL);

	gtk_scrolled_window_set_policy      (scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (scrolled_window, GTK_SHADOW_IN);
}

static void
e_tree_scrolled_real_construct (ETreeScrolled *ets)
{
	gtk_container_add(GTK_CONTAINER(ets), GTK_WIDGET(ets->tree));

	gtk_widget_show(GTK_WIDGET(ets->tree));
}

gboolean
e_tree_scrolled_construct (ETreeScrolled *ets,
                           ETreeModel *etm,
                           ETableExtras *ete,
                           const gchar *spec,
                           const gchar *state)
{
	g_return_val_if_fail(ets != NULL, FALSE);
	g_return_val_if_fail(E_IS_TREE_SCROLLED(ets), FALSE);
	g_return_val_if_fail(etm != NULL, FALSE);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), FALSE);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), FALSE);
	g_return_val_if_fail(spec != NULL, FALSE);

	if (!e_tree_construct (ets->tree, etm, ete, spec, state))
		return FALSE;

	e_tree_scrolled_real_construct(ets);

	return TRUE;
}

GtkWidget      *e_tree_scrolled_new                       (ETreeModel       *etm,
							    ETableExtras      *ete,
							    const gchar        *spec,
							    const gchar        *state)
{
	ETreeScrolled *ets;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec != NULL, NULL);

	ets = E_TREE_SCROLLED (gtk_widget_new (e_tree_scrolled_get_type (),
						"hadjustment", NULL,
						"vadjustment", NULL,
						NULL));

	if (!e_tree_scrolled_construct (ets, etm, ete, spec, state)) {
		g_object_unref (ets);
		return NULL;
	}

	return GTK_WIDGET (ets);
}

gboolean
e_tree_scrolled_construct_from_spec_file  (ETreeScrolled *ets,
                                           ETreeModel *etm,
                                           ETableExtras *ete,
                                           const gchar *spec_fn,
                                           const gchar *state_fn)
{
	g_return_val_if_fail(ets != NULL, FALSE);
	g_return_val_if_fail(E_IS_TREE_SCROLLED(ets), FALSE);
	g_return_val_if_fail(etm != NULL, FALSE);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), FALSE);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), FALSE);
	g_return_val_if_fail(spec_fn != NULL, FALSE);

	if (!e_tree_construct_from_spec_file (ets->tree, etm, ete, spec_fn, state_fn))
		return FALSE;

	e_tree_scrolled_real_construct(ets);

	return TRUE;
}

GtkWidget      *e_tree_scrolled_new_from_spec_file        (ETreeModel       *etm,
							    ETableExtras      *ete,
							    const gchar        *spec_fn,
							    const gchar        *state_fn)
{
	ETreeScrolled *ets;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TREE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_fn != NULL, NULL);

	ets = E_TREE_SCROLLED (gtk_widget_new (e_tree_scrolled_get_type (),
                                                "hadjustment", NULL,
                                                "vadjustment", NULL,
                                                NULL));
	if (!e_tree_scrolled_construct_from_spec_file (ets, etm, ete, spec_fn, state_fn)) {
		g_object_unref (ets);
		return NULL;
	}

	return GTK_WIDGET (ets);
}

ETree *
e_tree_scrolled_get_tree                 (ETreeScrolled *ets)
{
	return ets->tree;
}

static void
ets_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ETreeScrolled *ets = E_TREE_SCROLLED (object);

	switch (prop_id) {
	case PROP_TREE:
		g_value_set_object (value, ets->tree);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* Grab_focus handler for the scrolled ETree */
static void
ets_grab_focus (GtkWidget *widget)
{
	ETreeScrolled *ets;

	ets = E_TREE_SCROLLED (widget);

	gtk_widget_grab_focus (GTK_WIDGET (ets->tree));
}

/* Focus handler for the scrolled ETree */
static gint
ets_focus (GtkWidget *container, GtkDirectionType direction)
{
	ETreeScrolled *ets;

	ets = E_TREE_SCROLLED (container);

	return gtk_widget_child_focus (GTK_WIDGET (ets->tree), direction);
}

static void
e_tree_scrolled_class_init (ETreeScrolledClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	object_class->get_property = ets_get_property;

	widget_class->grab_focus = ets_grab_focus;

	widget_class->focus = ets_focus;

	g_object_class_install_property (object_class, PROP_TREE,
					 g_param_spec_object ("tree",
							      _( "Tree" ),
							      _( "Tree" ),
							      E_TREE_TYPE,
							      G_PARAM_READABLE));
}

