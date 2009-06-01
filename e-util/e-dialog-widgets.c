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



/* A widget, a pointer to the variable it will modify, and extra information */
typedef struct {
	GtkWidget *widget;
	gpointer value_var;
	gpointer info;
} WidgetHook;

/* Hook information for a complete dialog */
typedef struct {
	GSList *whooks;
} DialogHooks;



/* Destroy handler for the dialog; frees the dialog hooks */
static void
dialog_destroy_cb (DialogHooks *hooks, GObject *dialog)
{
	g_slist_free (hooks->whooks);
	hooks->whooks = NULL;

	g_free (hooks);
	g_object_set_data (dialog, "dialog-hooks", NULL);
}

/* Ensures that the dialog has the necessary attached data to store the widget
 * hook information.
 */
static DialogHooks *
get_dialog_hooks (GtkWidget *dialog)
{
	DialogHooks *hooks;

	hooks = g_object_get_data ((GObject *) dialog, "dialog-hooks");
	if (!hooks) {
		hooks = g_new0 (DialogHooks, 1);
		g_object_set_data ((GObject *) dialog, "dialog-hooks", hooks);
		g_object_weak_ref ((GObject *) dialog, (GWeakNotify) dialog_destroy_cb, hooks);
	}

	return hooks;
}

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

/* Hooks a radio button group */
static void
hook_radio (GtkWidget *dialog, GtkRadioButton *radio, gpointer value_var, gpointer info)
{
	const gint *value_map;
	gint *value;

	/* Set the value */

	value = (gint *) value_var;
	value_map = (const gint *) info;

	e_dialog_radio_set (GTK_WIDGET (radio), *value, value_map);
}

/* Gets the value of a radio button group */
static void
get_radio_value (GtkRadioButton *radio, gpointer value_var, gpointer info)
{
	gint *value;
	const gint *value_map;

	value = (gint *) value_var;
	value_map = (const gint *) info;

	*value = e_dialog_radio_get (GTK_WIDGET (radio), value_map);
}

/* Hooks a toggle button */
static void
hook_toggle (GtkWidget *dialog, GtkToggleButton *toggle, gpointer value_var, gpointer info)
{
	gboolean *value;

	/* Set the value */

	value = (gboolean *) value_var;
	e_dialog_toggle_set (GTK_WIDGET (toggle), *value);
}

/* Gets the value of a toggle button */
static void
get_toggle_value (GtkToggleButton *toggle, gpointer value_var, gpointer info)
{
	gboolean *value;

	value = (gboolean *) value_var;
	*value = e_dialog_toggle_get (GTK_WIDGET (toggle));
}

/* Hooks a spin button */
static void
hook_spin_button (GtkWidget *dialog, GtkSpinButton *spin, gpointer value_var, gpointer info)
{
	double *value;
	GtkAdjustment *adj;

	/* Set the value */

	value = (double *) value_var;
	e_dialog_spin_set (GTK_WIDGET (spin), *value);

	/* Hook to changed */

	adj = gtk_spin_button_get_adjustment (spin);
}

/* Gets the value of a spin button */
static void
get_spin_button_value (GtkSpinButton *spin, gpointer value_var, gpointer info)
{
	double *value;

	value = (double *) value_var;
	*value = e_dialog_spin_get_double (GTK_WIDGET (spin));
}

/* Hooks a GtkEditable widget */
static void
hook_editable (GtkWidget *dialog, GtkEditable *editable, gpointer value_var, gpointer info)
{
	gchar **value;

	/* Set the value */

	value = (gchar **) value_var;

	e_dialog_editable_set (GTK_WIDGET (editable), *value);
}

/* Gets the value of a GtkEditable widget */
static void
get_editable_value (GtkEditable *editable, gpointer value_var, gpointer data)
{
	gchar **value;

	value = (gchar **) value_var;
	if (*value)
		g_free (*value);

	*value = e_dialog_editable_get (GTK_WIDGET (editable));
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
 * e_dialog_radio_set:
 * @widget: A #GtkRadioButton in a radio button group.
 * @value: Enumerated value.
 * @value_map: Map from enumeration values to array indices.
 *
 * Sets the selected item in a radio group.  The specified @widget can be any of
 * the #GtkRadioButtons in the group.  Each radio button should correspond to an
 * enumeration value; the specified @value will be mapped to an integer from
 * zero to the number of items in the group minus 1 by using a mapping table
 * specified in @value_map.  The last element in this table should be -1.  Thus
 * a table to map three possible interpolation values to integers could be
 * specified as { NEAREST_NEIGHBOR, BILINEAR, HYPERBOLIC, -1 }.
 **/
void
e_dialog_radio_set (GtkWidget *widget, gint value, const gint *value_map)
{
	GSList *group, *l;
	gint i;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_RADIO_BUTTON (widget));
	g_return_if_fail (value_map != NULL);

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	i = value_to_index (value_map, value);
	if (i != -1) {
		/* Groups are built by prepending items, so the list ends up in reverse
		 * order; we need to flip the index around.
		 */
		i = g_slist_length (group) - i - 1;

		l = g_slist_nth (group, i);
		if (!l)
			g_message ("e_dialog_radio_set(): could not find index %d in radio group!", i);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (l->data), TRUE);
	} else
		g_message ("e_dialog_radio_set(): could not find value %d in value map!", value);
}

/**
 * e_dialog_radio_get:
 * @widget: A #GtkRadioButton in a radio button group.
 * @value_map: Map from enumeration values to array indices.
 *
 * Queries the selected item in a #GtkRadioButton group.  Please read the
 * description of e_dialog_radio_set() to see how @value_map maps enumeration
 * values to button indices.
 *
 * Return value: Enumeration value which corresponds to the selected item in the
 * radio group.
 **/
gint
e_dialog_radio_get (GtkWidget *widget, const gint *value_map)
{
	GSList *group, *l;
	gint i, v;

	g_return_val_if_fail (widget != NULL, -1);
	g_return_val_if_fail (GTK_IS_RADIO_BUTTON (widget), -1);
	g_return_val_if_fail (value_map != NULL, -1);

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	for (i = 0, l = group; l; l = l->next, i++) {
		widget = GTK_WIDGET (l->data);

		if (GTK_TOGGLE_BUTTON (widget)->active)
			break;
	}

	g_return_val_if_fail (l != NULL, -1);

	/* Groups are built by prepending items, so the list ends up in reverse
	 * order; we need to flip the index around.
	 */
	i = g_slist_length (group) - i - 1;

	v = index_to_value (value_map, i);
	if (v == -1) {
		g_message ("e_dialog_radio_get(): could not find index %d in value map!", i);
		return -1;
	}

	return v;
}

/**
 * e_dialog_toggle_set:
 * @widget: A #GtkToggleButton.
 * @value: Toggle value.
 *
 * Sets the value of a #GtkToggleButton-derived widget.  This should not be used
 * for radio buttons; it is more convenient to use use e_dialog_radio_set()
 * instead.
 **/
void
e_dialog_toggle_set (GtkWidget *widget, gboolean value)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
}

/**
 * e_dialog_toggle_get:
 * @widget: A #GtkToggleButton.
 *
 * Queries the value of a #GtkToggleButton-derived widget.  This should not be
 * used for radio buttons; it is more convenient to use e_dialog_radio_get()
 * instead.
 *
 * Return value: Toggle value.
 **/
gboolean
e_dialog_toggle_get (GtkWidget *widget)
{
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (widget), FALSE);

	return GTK_TOGGLE_BUTTON (widget)->active;
}

/**
 * e_dialog_spin_set:
 * @widget: A #GtkSpinButton.
 * @value: Numeric value.
 *
 * Sets the value of a #GtkSpinButton widget.
 **/
void
e_dialog_spin_set (GtkWidget *widget, double value)
{
	GtkAdjustment *adj;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));

	adj->value = value;
	g_signal_emit_by_name (adj, "value_changed", 0);
}

/**
 * e_dialog_spin_get_double:
 * @widget: A #GtkSpinButton.
 *
 * Queries the floating-point value of a #GtkSpinButton widget.
 *
 * Return value: Numeric value.
 **/
gdouble
e_dialog_spin_get_double (GtkWidget *widget)
{
	GtkAdjustment *adj;

	g_return_val_if_fail (widget != NULL, 0.0);
	g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), 0.0);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
	return adj->value;
}

/**
 * e_dialog_spin_get_int:
 * @widget: A #GtkSpinButton.
 *
 * Queries the integer value of a #GtkSpinButton widget.
 *
 * Return value: Numeric value.
 **/
gint
e_dialog_spin_get_int (GtkWidget *widget)
{
	double value;

	g_return_val_if_fail (widget != NULL, -1);
	g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), -1);

	value = e_dialog_spin_get_double (widget);
	return (gint) floor (value);
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

/**
 * e_dialog_widget_hook_value:
 * @dialog: Dialog box in which the @widget lives in.
 * @widget: A widget that will control a variable.
 * @value_var: Pointer to the variable that the @widget will control.
 * @info: NULL for most widgets, or an integer value map array (see
 * e_dialog_radio_set() for details).
 *
 * Hooks a widget from a dialog box to the variable it will modify.  Supported
 * widgets are:  #GtkEditable (gchar *), #GtkRadioButton (int/value_map pair; see
 * e_dialog_radio_set() for more information), #GtkTogglebutton (gboolean),
 * #GtkSpinButton (double), #GtkOptionMenu (int/value_map pair), and
 * #GnomeDateEdit (time_t).
 *
 * A pointer to the appropriate variable to modify should be passed in @value_var.
 * For values that take a value_map array as well, it should be passed in @info.
 *
 * The widgets within a dialog that are hooked with this function will set their
 * respective variables only when e_dialog_get_values() is called.  The typical
 * use is to call that function in the handler for the "OK" button of a dialog
 * box.
 *
 * Return value: TRUE if the type of the specified @widget is supported, FALSE
 * otherwise.
 **/
gboolean
e_dialog_widget_hook_value (GtkWidget *dialog, GtkWidget *widget,
			    gpointer value_var, gpointer info)
{
	DialogHooks *hooks;
	WidgetHook *wh;

	g_return_val_if_fail (dialog != NULL, FALSE);
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (value_var != NULL, FALSE);

	hooks = get_dialog_hooks (dialog);

	/* First check if it is a "group" widget, like a radio button or an
	 * option menu.  Then we check for normal ungrouped widgets.
	 */

	if (GTK_IS_RADIO_BUTTON (widget))
		hook_radio (dialog, GTK_RADIO_BUTTON (widget), value_var, info);
	else if (GTK_IS_TOGGLE_BUTTON (widget))
		hook_toggle (dialog, GTK_TOGGLE_BUTTON (widget), value_var, info);
	else if (GTK_IS_SPIN_BUTTON (widget))
		hook_spin_button (dialog, GTK_SPIN_BUTTON (widget), value_var, info);
	else if (GTK_IS_EDITABLE (widget))
		hook_editable (dialog, GTK_EDITABLE (widget), value_var, info);
	else
		return FALSE;

	wh = g_new (WidgetHook, 1);
	wh->widget = widget;
	wh->value_var = value_var;
	wh->info = info;

	hooks->whooks = g_slist_prepend (hooks->whooks, wh);

	return TRUE;
}

/**
 * e_dialog_get_values:
 * @dialog: A dialog box whose widgets have been hooked to the appropriate
 * variables with e_dialog_widget_hook_value().
 *
 * Makes every widget in a @dialog that was hooked with
 * e_dialog_widget_hook_value() apply its value to its corresponding variable.
 * The typical usage is to call this function in the handler for the "OK" button
 * of a dialog box.
 **/
void
e_dialog_get_values (GtkWidget *dialog)
{
	DialogHooks *hooks;
	GSList *l;

	g_return_if_fail (dialog != NULL);

	hooks = get_dialog_hooks (dialog);

	for (l = hooks->whooks; l; l = l->next) {
		WidgetHook *wh;

		wh = l->data;

		if (GTK_IS_RADIO_BUTTON (wh->widget))
			get_radio_value (GTK_RADIO_BUTTON (wh->widget), wh->value_var, wh->info);
		else if (GTK_IS_TOGGLE_BUTTON (wh->widget))
			get_toggle_value (GTK_TOGGLE_BUTTON (wh->widget), wh->value_var, wh->info);
		else if (GTK_IS_SPIN_BUTTON (wh->widget))
			get_spin_button_value (GTK_SPIN_BUTTON (wh->widget), wh->value_var, wh->info);
		else if (GTK_IS_EDITABLE (wh->widget))
			get_editable_value (GTK_EDITABLE (wh->widget), wh->value_var, wh->info);
		else
			g_return_if_reached ();
	}
}

/**
 * e_dialog_xml_widget_hook_value:
 * @xml: Glade XML description of a dialog box.
 * @dialog: Dialog box in which the widget lives in.
 * @widget_name: Name of the widget in the Glade XML data.
 * @value_var: Pointer to the variable that the widget will control.
 * @info: NULL for most widgets, or an integer value map array (see
 * e_dialog_radio_set() for details).
 *
 * Similar to e_dialog_widget_hook_value(), but uses the widget from a #GladeXML
 * data structure.
 *
 * Return value: TRUE if the type of the specified widget is supported, FALSE
 * otherwise.
 **/
gboolean
e_dialog_xml_widget_hook_value (GladeXML *xml, GtkWidget *dialog, const gchar *widget_name,
				gpointer value_var, gpointer info)
{
	GtkWidget *widget;

	g_return_val_if_fail (xml != NULL, FALSE);
	g_return_val_if_fail (GLADE_IS_XML (xml), FALSE);
	g_return_val_if_fail (dialog != NULL, FALSE);
	g_return_val_if_fail (widget_name != NULL, FALSE);
	g_return_val_if_fail (value_var != NULL, FALSE);

	widget = glade_xml_get_widget (xml, widget_name);
	if (!widget) {
		g_message ("e_dialog_xml_widget_hook_value(): could not find widget `%s' in "
			   "Glade data!", widget_name);
		return FALSE;
	}

	return e_dialog_widget_hook_value (dialog, widget, value_var, info);
}
