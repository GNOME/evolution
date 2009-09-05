/*
 * Evolution internal utilities - Glade dialog widget utilities
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <math.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>

#include "e-dialog-widgets.h"

/* Converts an mapped value to the appropriate index in an item group.  The
 * values for the items are provided as a -1-terminated array.
 */
static gint
value_to_index (const gint *value_map, gint value)
{
	gint i;

	for (i = 0; value_map[i] != -1; i++)
		if (value_map[i] == value)
			return i;

	return -1;
}

/* Converts an index in an item group to the appropriate mapped value.  See the
 * function above.
 */
static gint
index_to_value (const gint *value_map, gint index)
{
	gint i;

	/* We do this the hard way, i.e. not as a simple array reference, to
	 * check for correctness.
	 */

	for (i = 0; value_map[i] != -1; i++)
		if (i == index)
			return value_map[i];

	return -1;
}

/**
 * e_dialog_editable_set:
 * @widget: A #GtkEditable widget.
 * @value: String value.
 *
 * Sets the string value inside a #GtkEditable-derived widget.
 **/
void
e_dialog_editable_set (GtkWidget *widget, const gchar *value)
{
	gint pos = 0;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_EDITABLE (widget));

	gtk_editable_delete_text (GTK_EDITABLE (widget), 0, -1);

	if (value)
		gtk_editable_insert_text (GTK_EDITABLE (widget), value, strlen (value), &pos);
}

/**
 * e_dialog_editable_get:
 * @widget: A #GtkEditable widget.
 *
 * Queries the string value inside a #GtkEditable-derived widget.
 *
 * Return value: String value.  You should free it when you are done with it.
 * This function can return NULL if the string could not be converted from
 * GTK+'s encoding into UTF8.
 **/
gchar *
e_dialog_editable_get (GtkWidget *widget)
{
	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (GTK_IS_EDITABLE (widget), NULL);

	return gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);
}

/**
 * e_dialog_combo_box_set:
 * @widget: A #GtkComboBox.
 * @value: Enumerated value.
 * @value_map: Map from enumeration values to array indices.
 *
 * Sets the selected item in a #GtkComboBox.  Please read the description of
 * e_dialog_radio_set() to see how @value_map maps enumeration values to item
 * indices.
 **/
void
e_dialog_combo_box_set (GtkWidget *widget, gint value, const gint *value_map)
{
	gint i;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_COMBO_BOX (widget));
	g_return_if_fail (value_map != NULL);

	i = value_to_index (value_map, value);

	if (i != -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
	else
		g_message ("e_dialog_combo_box_set(): could not find value %d in value map!",
			   value);
}

/**
 * e_dialog_combo_box_get:
 * @widget: A #GtkComboBox.
 * @value_map: Map from enumeration values to array indices.
 *
 * Queries the selected item in a #GtkComboBox.  Please read the description
 * of e_dialog_radio_set() to see how @value_map maps enumeration values to item
 * indices.
 *
 * Return value: Enumeration value which corresponds to the selected item in the
 * combo box.
 **/
gint
e_dialog_combo_box_get (GtkWidget *widget, const gint *value_map)
{
	gint i;

	g_return_val_if_fail (widget != NULL, -1);
	g_return_val_if_fail (GTK_IS_COMBO_BOX (widget), -1);
	g_return_val_if_fail (value_map != NULL, -1);

	i = index_to_value (value_map, gtk_combo_box_get_active (GTK_COMBO_BOX (widget)));
	if (i == -1) {
		g_message ("e_dialog_combo_box_get(): could not find index %d in value map!", i);
		return -1;
	}
	return i;
}
