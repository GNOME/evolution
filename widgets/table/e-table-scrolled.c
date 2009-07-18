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

#include "e-table.h"
#include "e-table-scrolled.h"

#define COLUMN_HEADER_HEIGHT 16

G_DEFINE_TYPE (ETableScrolled, e_table_scrolled, GTK_TYPE_SCROLLED_WINDOW)

enum {
	PROP_0,
	PROP_TABLE
};

static void
e_table_scrolled_init (ETableScrolled *ets)
{
	GtkScrolledWindow *scrolled_window;

	scrolled_window = GTK_SCROLLED_WINDOW (ets);

	GTK_WIDGET_SET_FLAGS (ets, GTK_CAN_FOCUS);

	ets->table = g_object_new (E_TABLE_TYPE, NULL);

	gtk_scrolled_window_set_policy      (scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (scrolled_window, GTK_SHADOW_IN);
}

static void
e_table_scrolled_real_construct (ETableScrolled *ets)
{
	gtk_container_add(GTK_CONTAINER(ets), GTK_WIDGET(ets->table));

	gtk_widget_show(GTK_WIDGET(ets->table));
}

ETableScrolled *e_table_scrolled_construct                 (ETableScrolled    *ets,
							    ETableModel       *etm,
							    ETableExtras      *ete,
							    const gchar        *spec,
							    const gchar        *state)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_SCROLLED(ets), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec != NULL, NULL);

	e_table_construct(ets->table, etm, ete, spec, state);

	e_table_scrolled_real_construct(ets);

	return ets;
}

GtkWidget      *e_table_scrolled_new                       (ETableModel       *etm,
							    ETableExtras      *ete,
							    const gchar        *spec,
							    const gchar        *state)
{
	ETableScrolled *ets;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec != NULL, NULL);

	ets = E_TABLE_SCROLLED (gtk_widget_new (e_table_scrolled_get_type (),
						"hadjustment", NULL,
						"vadjustment", NULL,
						NULL));

	ets = e_table_scrolled_construct (ets, etm, ete, spec, state);

	return GTK_WIDGET (ets);
}

ETableScrolled *e_table_scrolled_construct_from_spec_file  (ETableScrolled    *ets,
							    ETableModel       *etm,
							    ETableExtras      *ete,
							    const gchar        *spec_fn,
							    const gchar        *state_fn)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_SCROLLED(ets), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_fn != NULL, NULL);

	e_table_construct_from_spec_file(ets->table, etm, ete, spec_fn, state_fn);

	e_table_scrolled_real_construct(ets);

	return ets;
}

GtkWidget      *e_table_scrolled_new_from_spec_file        (ETableModel       *etm,
							    ETableExtras      *ete,
							    const gchar        *spec_fn,
							    const gchar        *state_fn)
{
	ETableScrolled *ets;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_fn != NULL, NULL);

	ets = E_TABLE_SCROLLED (gtk_widget_new (e_table_scrolled_get_type (),
						"hadjustment", NULL,
						"vadjustment", NULL,
						NULL));

	ets = e_table_scrolled_construct_from_spec_file (ets, etm, ete, spec_fn, state_fn);

	return GTK_WIDGET (ets);
}

ETable *
e_table_scrolled_get_table                 (ETableScrolled *ets)
{
	return ets->table;
}

static void
ets_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ETableScrolled *ets = E_TABLE_SCROLLED (object);

	switch (prop_id) {
	case PROP_TABLE:
		g_value_set_object (value, ets->table);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* Grab_focus handler for the scrolled ETable */
static void
ets_grab_focus (GtkWidget *widget)
{
	ETableScrolled *ets;

	ets = E_TABLE_SCROLLED (widget);

	gtk_widget_grab_focus (GTK_WIDGET (ets->table));
}

/* Focus handler for the scrolled ETable */
static gint
ets_focus (GtkWidget *container, GtkDirectionType direction)
{
	ETableScrolled *ets;

	ets = E_TABLE_SCROLLED (container);

	return gtk_widget_child_focus (GTK_WIDGET (ets->table), direction);
}

static void
e_table_scrolled_class_init (ETableScrolledClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	object_class->get_property = ets_get_property;

	widget_class->grab_focus = ets_grab_focus;

	widget_class->focus = ets_focus;

	g_object_class_install_property (object_class, PROP_TABLE,
					 g_param_spec_object ("table",
							      _( "Table" ),
							      _( "Table" ),
							      E_TABLE_TYPE,
							      G_PARAM_READABLE));
}

