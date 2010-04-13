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
 * Copyright (C) 1999-2010 Novell, Inc. (www.novell.com)
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <libedataserver/e-source.h>
#include <string.h>

#include "e-plugin-util.h"

/* name of a property on a widget with corresponding property name for an ESource */
#define EPU_SP_NAME "e-source-property-name"

#define EPU_CHECK_TRUE "epu-check-true-value"
#define EPU_CHECK_FALSE "epu-check-false-value"

static gboolean
epu_is_uri_proto (const gchar *uri, const gchar *protocol)
{
	gboolean res;

	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (protocol != NULL, FALSE);

	res = uri && g_ascii_strncasecmp (uri, protocol, strlen (protocol)) == 0;

	if (res)
		res = strchr (protocol, ':') != NULL || uri[strlen (protocol)] == ':';

	return res;
}

/**
 * e_plugin_util_is_source_proto:
 * @source: #ESource object
 * @protocol: protocol to check on, like "http", "https", ...
 *
 * Returns whether given source's uri is of the given protocol.
 *
 * Returns: whether given source's uri is of the given protocol.
 **/
gboolean
e_plugin_util_is_source_proto (ESource *source, const gchar *protocol)
{
	gchar *uri;
	gboolean res;

	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (protocol != NULL, FALSE);

	uri = e_source_get_uri (source);
	res = epu_is_uri_proto (uri, protocol);
	g_free (uri);

	return res;
}

/**
 * e_plugin_util_is_group_proto:
 * @group: #ESourceGroup object
 * @protocol: protocol to check on, like "http", "https", ...
 *
 * Returns whether given groups' base uri is of the given protocol.
 *
 * Returns: whether given groups' base uri is of the given protocol.
 **/
gboolean
e_plugin_util_is_group_proto (ESourceGroup *group, const gchar *protocol)
{
	g_return_val_if_fail (E_IS_SOURCE_GROUP (group), FALSE);
	g_return_val_if_fail (protocol != NULL, FALSE);

	return epu_is_uri_proto (e_source_group_peek_base_uri (group), protocol);
}

/**
 * e_plugin_util_replace_at_sign:
 * @str: string to work with
 *
 * Replaces all '@' with '%40' in @str.
 *
 * Returns: a newly-allocated string
 **/
gchar *
e_plugin_util_replace_at_sign (const gchar *str)
{
	gchar *res, *at;

	if (!str)
		return NULL;

	res = g_strdup (str);
	while (at = strchr (res, '@'), at) {
		gchar *tmp = g_malloc0 (sizeof (gchar) * (1 + strlen (res) + 2));

		strncpy (tmp, res, at - res);
		strcat (tmp, "%40");
		strcat (tmp, at + 1);

		g_free (res);
		res = tmp;
	}

	return res;
}

/**
 * e_plugin_util_uri_no_proto:
 * @uri: #SoupURI object
 *
 * Returns uri encoded as string, without protocol part.
 * Returned pointer should be freed with g_free.
 *
 * Returns: uri encoded as string, without protocol part.
 **/
gchar *
e_plugin_util_uri_no_proto (SoupURI *uri)
{
	gchar *full_uri, *uri_noproto;
	const gchar *tmp;

	g_return_val_if_fail (uri != NULL, NULL);

	full_uri = soup_uri_to_string (uri, FALSE);
	g_return_val_if_fail (full_uri != NULL, NULL);

	tmp = strstr (full_uri, "://");
	if (tmp && tmp < strchr (full_uri, '/')) {
		uri_noproto = g_strdup (tmp + 3);
	} else {
		uri_noproto = full_uri;
		full_uri = NULL;
	}

	g_free (full_uri);

	return uri_noproto;
}

static void
epu_update_source_property (ESource *source, GObject *object, const gchar *value)
{
	const gchar *property_name;

	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (G_IS_OBJECT (object));

	property_name = g_object_get_data (object, EPU_SP_NAME);
	g_return_if_fail (property_name != NULL);

	e_source_set_property (source, property_name, value);
}

static void
epu_entry_changed_cb (GObject *entry, ESource *source)
{
	g_return_if_fail (GTK_IS_ENTRY (entry));

	epu_update_source_property (source, entry, gtk_entry_get_text (GTK_ENTRY (entry)));
}

static void
epu_check_toggled_cb (GObject *button, ESource *source)
{
	const gchar *true_value, *false_value;

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

	true_value = g_object_get_data (button, EPU_CHECK_TRUE);
	false_value = g_object_get_data (button, EPU_CHECK_FALSE);

	epu_update_source_property (source, button, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) ? true_value : false_value);
}

/**
 * e_plugin_util_add_entry:
 * @parent: two-columns #GtkTable or #GtkContainer, where to add new entry
 * @label: label for the entry; can be NULL for no label
 * @source: #ESource object to which tight the entry change; can be NULL for no property binding
 * @source_property: source's property name to use for a value; can be NULL for no property binding
 *
 * Adds a #GtkEntry to the table at the last row or to the container, with a given label.
 * The entry will be always at the second column of the table.
 * Value of an entry will be prefilled with a property value of the given
 * source, and the source will be updated on any change of the entry automatically.
 * Entry is shown by default.
 *
 * Returns: pointer to newly added #GtkEntry
 **/
GtkWidget *
e_plugin_util_add_entry (GtkWidget *parent, const gchar *label, ESource *source, const gchar *source_property)
{
	GtkWidget *entry, *lbl = NULL;
	const gchar *value;
	gint row = -1;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (GTK_IS_TABLE (parent) || GTK_IS_CONTAINER (parent), NULL);

	if (source || source_property) {
		g_return_val_if_fail (E_IS_SOURCE (source), NULL);
		g_return_val_if_fail (source_property != NULL, NULL);
		g_return_val_if_fail (*source_property != 0, NULL);
	}

	if (GTK_IS_TABLE (parent))
		g_object_get (parent, "n-rows", &row, NULL);

	if (label) {
		lbl = gtk_label_new_with_mnemonic (label);
		gtk_widget_show (lbl);
		gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
		if (row != -1)
			gtk_table_attach (GTK_TABLE (parent), lbl, 0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
		else
			gtk_container_add (GTK_CONTAINER (parent), lbl);
	}

	if (source)
		value = e_source_get_property (source, source_property);
	else
		value = NULL;

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_entry_set_text (GTK_ENTRY (entry), value ? value : "");
	if (row != -1)
		gtk_table_attach (GTK_TABLE (parent), entry, 1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	else
		gtk_container_add (GTK_CONTAINER (parent), entry);

	if (lbl)
		gtk_label_set_mnemonic_widget (GTK_LABEL (lbl), entry);

	if (source) {
		g_object_set_data_full (G_OBJECT (entry), EPU_SP_NAME, g_strdup (source_property), g_free);
		g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (epu_entry_changed_cb), source);
	}

	return entry;
}

/**
 * e_plugin_util_add_check:
 * @parent: either two-columns #GtkTable or #GtkContainer where to add new check box; or NULL to just create it
 * @label: label for the check; cannot be NULL
 * @source: #ESource object to which tight the check change; can be NULL for no property binding
 * @source_property: source's property name to use for a value; can be NULL for no property binding
 * @true_value: what value use for a checked state in a source
 * @false_value: what value use for an unchecked state in a source
 *
 * Adds a #GtkCheckButton to the parent (if provided) at the last row, with a given label.
 * The check will be always at the second column of the table.
 * Value of a check will be prefilled with a property value of the given
 * source, and the source will be updated on any change of the check automatically.
 * Check is shown by default.
 *
 * Returns: pointer to newly added #GtkCheckButton
 **/
GtkWidget *
e_plugin_util_add_check (GtkWidget *parent, const gchar *label, ESource *source, const gchar *source_property, const gchar *true_value, const gchar *false_value)
{
	GtkWidget *check;
	const gchar *value;
	guint row;

	g_return_val_if_fail (parent == NULL || GTK_IS_TABLE (parent) || GTK_IS_CONTAINER (parent), NULL);
	g_return_val_if_fail (label != NULL, NULL);

	if (source || source_property) {
		g_return_val_if_fail (source != NULL, NULL);
		g_return_val_if_fail (E_IS_SOURCE (source), NULL);
		g_return_val_if_fail (source_property != NULL, NULL);
		g_return_val_if_fail (*source_property != 0, NULL);
	}

	if (source)
		value = e_source_get_property (source, source_property);
	else
		value = NULL;

	check = gtk_check_button_new_with_mnemonic (label);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
		value && (value == true_value ||
			  (true_value && g_str_equal (value, true_value)) ||
			  (!source && g_str_equal (value, "1"))));
	gtk_widget_show (check);

	if (parent && GTK_IS_TABLE (parent)) {
		g_object_get (parent, "n-rows", &row, NULL);

		gtk_table_attach (GTK_TABLE (parent), check, 1, 2, row , row + 1, GTK_FILL, 0, 0, 0);
	} else if (parent) {
		gtk_container_add (GTK_CONTAINER (parent), check);
	}

	if (source) {
		g_object_set_data_full (G_OBJECT (check), EPU_SP_NAME, g_strdup (source_property), g_free);
		g_object_set_data_full (G_OBJECT (check), EPU_CHECK_TRUE, g_strdup (true_value), g_free);
		g_object_set_data_full (G_OBJECT (check), EPU_CHECK_FALSE, g_strdup (false_value), g_free);
		g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (epu_check_toggled_cb), source);
	}

	return check;
}

static void
epu_update_refresh_value (GtkWidget *spin, GtkWidget *combobox, ESource *source)
{
	gchar *value;
	gint setting = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	switch (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox))) {
	case 0:
		/* minutes */
		break;
	case 1:
		/* hours */
		setting *= 60;
		break;
	case 2:
		/* days */
		setting *= 1440;
		break;
	case 3:
		/* weeks */
		setting *= 10080;
		break;
	default:
		g_warning ("%s: Time unit out of range", G_STRFUNC);
		break;
	}

	value = g_strdup_printf ("%d", setting);
	epu_update_source_property (source, G_OBJECT (spin), value);
	g_free (value);
}

static void
epu_refresh_spin_changed_cb (GtkWidget *spin, ESource *source)
{
	g_return_if_fail (spin != NULL);
	g_return_if_fail (GTK_IS_SPIN_BUTTON (spin));

	epu_update_refresh_value (spin, g_object_get_data (G_OBJECT (spin), "refresh-combo"), source);
}

static void
epu_refresh_combo_changed_cb (GtkWidget *combobox, ESource *source)
{
	g_return_if_fail (combobox != NULL);
	g_return_if_fail (GTK_IS_COMBO_BOX (combobox));

	epu_update_refresh_value (g_object_get_data (G_OBJECT (combobox), "refresh-spin"), combobox, source);
}

/**
 * e_plugin_util_add_refresh:
 * @parent: two-columns #GtkTable where to add new "refresh" setup widgets or NULL to just create an hbox
 * @label: label for the widgets; can be NULL, but for parent == NULL is ignored
 * @source: #ESource object to which tight the refresh change; cannot be NULL
 * @source_property: source's property name to use for a value; cannot be NULL
 *
 * Adds widgets to setup Refresh interval. The stored value is in minutes.
 * Returns pointer to an HBox, which contains two widgets, spin and a combo box.
 * Both can be accessed by g_object_get_data with a name "refresh-spin" and "refresh-combo".
 *
 * Returns: a new refresh control widget
 **/
GtkWidget *
e_plugin_util_add_refresh (GtkWidget *parent, const gchar *label, ESource *source, const gchar *source_property)
{
	GtkWidget *lbl = NULL, *hbox, *spin, *combo;
	const gchar *value;
	gint row = -1, value_num, item_num = 0;

	g_return_val_if_fail (parent == NULL || GTK_IS_TABLE (parent), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (source_property != NULL, NULL);
	g_return_val_if_fail (*source_property != 0, NULL);

	if (parent)
		g_object_get (parent, "n-rows", &row, NULL);

	value = e_source_get_property (source, source_property);
	if (!value) {
		value = "30";
		e_source_set_property (source, source_property, value);
	}

	if (label && parent) {
		lbl = gtk_label_new_with_mnemonic (label);
		gtk_widget_show (lbl);
		gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
		gtk_table_attach (GTK_TABLE (parent), lbl, 0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
	}

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);

	spin = gtk_spin_button_new_with_range (0, 100, 1);
	gtk_widget_show (spin);
	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);

	combo = gtk_combo_box_new_text ();
	gtk_widget_show (combo);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("minutes"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("hours"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("days"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("weeks"));
	gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, TRUE, 0);

	value_num = value ? atoi (value) : 30;

	if (value_num && !(value_num % 10080)) {
		/* weeks */
		item_num = 3;
		value_num /= 10080;
	} else if (value_num && !(value_num % 1440)) {
		/* days */
		item_num = 2;
		value_num /= 1440;
	} else if (value_num && !(value_num % 60)) {
		/* hours */
		item_num = 1;
		value_num /= 60;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), item_num);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), value_num);

	if (lbl)
		gtk_label_set_mnemonic_widget (GTK_LABEL (lbl), spin);

	g_object_set_data_full (G_OBJECT (spin), EPU_SP_NAME, g_strdup (source_property), g_free);

	g_object_set_data (G_OBJECT (combo), "refresh-spin", spin);
	g_object_set_data (G_OBJECT (spin), "refresh-combo", combo);
	g_object_set_data (G_OBJECT (hbox), "refresh-spin", spin);
	g_object_set_data (G_OBJECT (hbox), "refresh-combo", combo);
	g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (epu_refresh_combo_changed_cb), source);
	g_signal_connect (G_OBJECT (spin), "value-changed", G_CALLBACK (epu_refresh_spin_changed_cb), source);

	if (parent)
		gtk_table_attach (GTK_TABLE (parent), hbox, 1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return hbox;
}
