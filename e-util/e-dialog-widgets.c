/* Evolution internal utilities - Glade dialog widget utilities
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <math.h>
#include <time.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkspinbutton.h>
#include <libgnomeui/gnome-dateedit.h>
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
dialog_destroy_cb (GtkObject *dialog, gpointer data)
{
	DialogHooks *hooks;

	hooks = data;

	g_slist_free (hooks->whooks);
	hooks->whooks = NULL;

	g_free (hooks);
	gtk_object_set_data (dialog, "dialog-hooks", NULL);
}

/* Ensures that the dialog has the necessary attached data to store the widget
 * hook information.
 */
static DialogHooks *
get_dialog_hooks (GtkWidget *dialog)
{
	DialogHooks *hooks;

	hooks = gtk_object_get_data (GTK_OBJECT (dialog), "dialog-hooks");
	if (!hooks) {
		hooks = g_new0 (DialogHooks, 1);
		gtk_object_set_data (GTK_OBJECT (dialog), "dialog-hooks", hooks);
		gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
				    GTK_SIGNAL_FUNC (dialog_destroy_cb), hooks);
	}

	return hooks;
}

/* Converts an mapped value to the appropriate index in an item group.  The
 * values for the items are provided as a -1-terminated array.
 */
static int
value_to_index (const int *value_map, int value)
{
	int i;

	for (i = 0; value_map[i] != -1; i++)
		if (value_map[i] == value)
			return i;

	return -1;
}

/* Converts an index in an item group to the appropriate mapped value.  See the
 * function above.
 */
static int
index_to_value (const int *value_map, int index)
{
	int i;

	/* We do this the hard way, i.e. not as a simple array reference, to
	 * check for correctness.
	 */

	for (i = 0; value_map[i] != -1; i++)
		if (i == index)
			return value_map[i];

	return -1;
}

/* Callback for the "toggled" signal of toggle buttons */
static void
toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	GnomePropertyBox *pbox;

	pbox = GNOME_PROPERTY_BOX (data);

	/* For radio buttons, we only notify the property box if the button is
	 * active, because we'll get one call for each of the changed buttons in
	 * the radio group.
	 */
	if (!GTK_IS_RADIO_BUTTON (toggle) || toggle->active)
		gnome_property_box_changed (pbox);
}

/* Hooks a radio button group */
static void
hook_radio (GtkWidget *dialog, GtkRadioButton *radio, gpointer value_var, gpointer info)
{
	GSList *group;
	int *value;
	int i;
	const int *value_map;
	GSList *l;

	group = gtk_radio_button_group (radio);

	/* Set the value */

	value = (int *) value_var;
	value_map = (const int *) info;

	i = value_to_index (value_map, *value);
	if (i != -1) {
		l = g_slist_nth (group, i);
		if (!l)
			g_message ("hook_radio(): could not find index %d in radio group!", i);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (l->data), TRUE);
	} else
		g_message ("hook_radio(): could not find value %d in value map!", *value);

	/* Hook to changed */

	if (GNOME_IS_PROPERTY_BOX (dialog))
		for (l = group; l; l = l->next)
			gtk_signal_connect (GTK_OBJECT (l->data), "toggled",
					    GTK_SIGNAL_FUNC (toggled_cb), dialog);
}

/* Gets the value of a radio button group */
static void
get_radio_value (GtkRadioButton *radio, gpointer value_var, gpointer info)
{
	GSList *group;
	GSList *l;
	int i;
	int *value;
	const int *value_map;
	int v;

	group = gtk_radio_button_group (radio);

	for (i = 0, l = group; l; l = l->next) {
		radio = GTK_RADIO_BUTTON (l->data);

		if (GTK_TOGGLE_BUTTON (radio)->active)
			break;
	}

	if (!l)
		g_assert_not_reached ();

	value = (int *) value_var;
	value_map = (const int *) info;

	v = index_to_value (value_map, i);
	if (v == -1) {
		g_message ("get_radio_value(): could not find index %d in value map!", i);
		return;
	}

	*value = v;
}

/* Callback for the "activate" signal of menu items */
static void
activate_cb (GtkMenuItem *item, gpointer data)
{
	GnomePropertyBox *pbox;

	pbox = GNOME_PROPERTY_BOX (data);
	gnome_property_box_changed (pbox);
}

/* Hooks an option menu */
static void
hook_option_menu (GtkWidget *dialog, GtkOptionMenu *omenu, gpointer value_var, gpointer info)
{
	int *value;
	int i;
	const int *value_map;

	/* Set the value */

	value = (int *) value_var;
	value_map = (const int *) info;

	i = value_to_index (value_map, *value);
	if (i != -1)
		gtk_option_menu_set_history (omenu, i);
	else
		g_message ("hook_option_menu(): could not find value %d in value map!", *value);

	/* Hook to changed */

	if (GNOME_IS_PROPERTY_BOX (dialog)) {
		GtkMenu *menu;
		GList *l;

		menu = GTK_MENU (gtk_option_menu_get_menu (omenu));

		for (l = GTK_MENU_SHELL (menu)->children; l; l = l->next)
			gtk_signal_connect (GTK_OBJECT (l->data), "activate",
					    GTK_SIGNAL_FUNC (activate_cb), dialog);
	}
}

/* Gets the value of an option menu */
static void
get_option_menu_value (GtkOptionMenu *omenu, gpointer value_var, gpointer info)
{
	GtkMenu *menu;
	GtkWidget *active;
	GList *children;
	GList *l;
	int i;
	int *value;
	const int *value_map;
	int v;

	menu = GTK_MENU (gtk_option_menu_get_menu (omenu));

	active = gtk_menu_get_active (menu);
	g_assert (active != NULL);

	children = GTK_MENU_SHELL (menu)->children;

	for (i = 0, l = children; l; l = l->next) {
		if (GTK_WIDGET (l->data) == active)
			break;
	}

	if (!l)
		g_assert_not_reached ();

	value = (int *) value_var;
	value_map = (const int *) info;

	v = index_to_value (value_map, i);
	if (v == -1) {
		g_message ("get_option_menu_value(): could not find index %d in value map!", i);
		return;
	}

	*value = v;
}

/* Hooks a toggle button */
static void
hook_toggle (GtkWidget *dialog, GtkToggleButton *toggle, gpointer value_var, gpointer info)
{
	gboolean *value;

	/* Set the value */

	value = (gboolean *) value_var;
	gtk_toggle_button_set_active (toggle, *value);

	/* Hook to changed */

	if (GNOME_IS_PROPERTY_BOX (dialog))
		gtk_signal_connect (GTK_OBJECT (toggle), "toggled",
				    GTK_SIGNAL_FUNC (toggled_cb), dialog);
}

/* Gets the value of a toggle button */
static void
get_toggle_value (GtkToggleButton *toggle, gpointer value_var, gpointer info)
{
	gboolean *value;

	value = (gboolean *) value;
	*value = toggle->active ? TRUE : FALSE;
}

/* Callback for the "value_changed" signal of the adjustment of a spin button */
static void
value_changed_cb (GtkAdjustment *adj, gpointer data)
{
	GnomePropertyBox *pbox;

	pbox = GNOME_PROPERTY_BOX (data);
	gnome_property_box_changed (pbox);
}

/* Hooks a spin button */
static void
hook_spin_button (GtkWidget *dialog, GtkSpinButton *spin, gpointer value_var, gpointer info)
{
	double *value;
	GtkAdjustment *adj;

	/* Set the value */

	value = (double *) value_var;
	adj = gtk_spin_button_get_adjustment (spin);

	adj->value = *value;
	gtk_signal_emit_by_name (GTK_OBJECT (adj), "value_changed");

	/* Hook to changed */

	if (GNOME_IS_PROPERTY_BOX (dialog))
		gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
				    GTK_SIGNAL_FUNC (value_changed_cb), dialog);
}

/* Gets the value of a spin button */
static void
get_spin_button_value (GtkSpinButton *spin, gpointer value_var, gpointer info)
{
	double *value;
	GtkAdjustment *adj;

	value = (double *) value_var;

	adj = gtk_spin_button_get_adjustment (spin);
	*value = adj->value;
}

/* Callback for the "changed" signal of a GtkEditable widget */
static void
changed_cb (GtkEditable *editable, gpointer data)
{
	GnomePropertyBox *pbox;

	pbox = GNOME_PROPERTY_BOX (data);
	gnome_property_box_changed (pbox);
}

/* Hooks a GtkEditable widget */
static void
hook_editable (GtkWidget *dialog, GtkEditable *editable, gpointer value_var, gpointer info)
{
	char **value;
	gint pos;

	/* Set the value */

	value = (char **) value_var;

	pos = 0;
	gtk_editable_delete_text (editable, 0, -1);

	/* Take NULL as empty string */
	if (*value)
		gtk_editable_insert_text (editable, *value, strlen (*value), &pos);

	/* Hook to changed */

	if (GNOME_IS_PROPERTY_BOX (dialog))
		gtk_signal_connect (GTK_OBJECT (editable), "changed",
				    GTK_SIGNAL_FUNC (changed_cb), dialog);
}

/* Gets the value of a GtkEditable widget */
static void
get_editable_value (GtkEditable *editable, gpointer value_var, gpointer data)
{
	char **value;

	value = (char **) value_var;
	if (*value)
		g_free (*value);

	*value = gtk_editable_get_chars (editable, 0, -1);
}

void
e_dialog_editable_set (GtkWidget *widget, char *value)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_EDITABLE (widget));

	gtk_editable_delete_text (GTK_EDITABLE (widget), 0, -1);

	if (value) {
		gint pos;

		pos = 0;
		gtk_editable_insert_text (GTK_EDITABLE (widget), value, strlen (value), &pos);
	}
}

char *
e_dialog_editable_get (GtkWidget *widget)
{
	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (GTK_IS_EDITABLE (widget), NULL);

	return gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);
}

void
e_dialog_radio_set (GtkWidget *widget, int value, const int *value_map)
{
	GSList *group;
	int i;
	GSList *l;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_RADIO_BUTTON (widget));
	g_return_if_fail (value_map != NULL);

	group = gtk_radio_button_group (GTK_RADIO_BUTTON (widget));

	i = value_to_index (value_map, value);
	if (i != -1) {
		l = g_slist_nth (group, i);
		if (!l)
			g_message ("e_dialog_radio_set(): could not find index %d in radio group!",
				   i);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (l->data), TRUE);
	} else
		g_message ("e_dialog_radio_set(): could not find value %d in value map!",
			   value);
}

int
e_dialog_radio_get (GtkWidget *widget, const int *value_map)
{
	GSList *group;
	GSList *l;
	int i;
	int v;

	g_return_val_if_fail (widget != NULL, -1);
	g_return_val_if_fail (GTK_IS_RADIO_BUTTON (widget), -1);
	g_return_val_if_fail (value_map != NULL, -1);

	group = gtk_radio_button_group (GTK_RADIO_BUTTON (widget));

	for (i = 0, l = group; l; l = l->next) {
		widget = GTK_WIDGET (l->data);

		if (GTK_TOGGLE_BUTTON (widget)->active)
			break;
	}

	if (!l)
		g_assert_not_reached ();

	v = index_to_value (value_map, i);
	if (v == -1) {
		g_message ("e_dialog_radio_get(): could not find index %d in value map!", i);
		return -1;
	}

	return v;
}

void
e_dialog_toggle_set (GtkWidget *widget, gboolean value)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
}

gboolean
e_dialog_toggle_get (GtkWidget *widget)
{
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (widget), FALSE);

	return GTK_TOGGLE_BUTTON (widget)->active;
}

void
e_dialog_spin_set (GtkWidget *widget, double value)
{
	GtkAdjustment *adj;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));

	adj->value = value;
	gtk_signal_emit_by_name (GTK_OBJECT (adj), "value_changed");
}

double
e_dialog_spin_get_double (GtkWidget *widget)
{
	GtkAdjustment *adj;

	g_return_val_if_fail (widget != NULL, 0.0);
	g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), 0.0);

	adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
	return adj->value;
}

int
e_dialog_spin_get_int (GtkWidget *widget)
{
	double value;

	g_return_val_if_fail (widget != NULL, -1);
	g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), -1);

	value = e_dialog_spin_get_double (widget);
	return (int) floor (value);
}

void
e_dialog_option_menu_set (GtkWidget *widget, int value, const int *value_map)
{
	int i;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_OPTION_MENU (widget));
	g_return_if_fail (value_map != NULL);

	i = value_to_index (value_map, value);

	if (i != -1)
		gtk_option_menu_set_history (GTK_OPTION_MENU (widget), i);
	else
		g_message ("e_dialog_option_menu_set(): could not find value %d in value map!",
			   value);
}

int
e_dialog_option_menu_get (GtkWidget *widget, const int *value_map)
{
	GtkMenu *menu;
	GtkWidget *active;
	GList *children;
	GList *l;
	int i;
	int v;

	g_return_val_if_fail (widget != NULL, -1);
	g_return_val_if_fail (GTK_IS_OPTION_MENU (widget), -1);
	g_return_val_if_fail (value_map != NULL, -1);

	menu = GTK_MENU (gtk_option_menu_get_menu (GTK_OPTION_MENU (widget)));

	active = gtk_menu_get_active (menu);
	g_assert (active != NULL);

	children = GTK_MENU_SHELL (menu)->children;

	for (i = 0, l = children; l; l = l->next) {
		if (GTK_WIDGET (l->data) == active)
			break;
	}

	if (!l)
		g_assert_not_reached ();

	v = index_to_value (value_map, i);
	if (v == -1) {
		g_message ("e_dialog_option_menu_get(): could not find index %d in value map!", i);
		return -1;
	}

	return v;
}

void
e_dialog_dateedit_set (GtkWidget *widget, time_t t)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_DATE_EDIT (widget));

	gnome_date_edit_set_time (GNOME_DATE_EDIT (widget), t);
}

time_t
e_dialog_dateedit_get (GtkWidget *widget)
{
	g_return_val_if_fail (widget != NULL, -1);
	g_return_val_if_fail (GNOME_IS_DATE_EDIT (widget), -1);

	return gnome_date_edit_get_date (GNOME_DATE_EDIT (widget));
}

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
	else if (GTK_IS_OPTION_MENU (widget))
		hook_option_menu (dialog, GTK_OPTION_MENU (widget), value_var, info);
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
		else if (GTK_IS_OPTION_MENU (wh->widget))
			get_option_menu_value (GTK_OPTION_MENU (wh->widget), wh->value_var, wh->info);
		else if (GTK_IS_TOGGLE_BUTTON (wh->widget))
			get_toggle_value (GTK_TOGGLE_BUTTON (wh->widget), wh->value_var, wh->info);
		else if (GTK_IS_SPIN_BUTTON (wh->widget))
			get_spin_button_value (GTK_SPIN_BUTTON (wh->widget), wh->value_var, wh->info);
		else if (GTK_IS_EDITABLE (wh->widget))
			get_editable_value (GTK_EDITABLE (wh->widget), wh->value_var, wh->info);
		else
			g_assert_not_reached ();
	}
}

gboolean
e_dialog_xml_widget_hook_value (GladeXML *xml, GtkWidget *dialog, const char *widget_name,
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
