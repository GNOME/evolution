/*
 * Evolution internal utilities - Glade dialog widget utilities
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <math.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-dialog-widgets.h"

/* Converts a mapped value to the appropriate index in an item group.  The
 * values for the items are provided as a -1-terminated array.
 */
static gint
value_to_index (const gint *value_map,
                gint value)
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
index_to_value (const gint *value_map,
                gint index)
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
e_dialog_combo_box_set (GtkWidget *widget,
                        gint value,
                        const gint *value_map)
{
	gint i;

	g_return_if_fail (GTK_IS_COMBO_BOX (widget));
	g_return_if_fail (value_map != NULL);

	i = value_to_index (value_map, value);

	if (i != -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
	else
		g_message (
			"e_dialog_combo_box_set(): could not "
			"find value %d in value map!", value);
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
e_dialog_combo_box_get (GtkWidget *widget,
                        const gint *value_map)
{
	gint active;
	gint i;

	g_return_val_if_fail (GTK_IS_COMBO_BOX (widget), -1);
	g_return_val_if_fail (value_map != NULL, -1);

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	i = index_to_value (value_map, active);

	if (i == -1) {
		g_message (
			"e_dialog_combo_box_get(): could not "
			"find index %d in value map!", i);
		return -1;
	}

	return i;
}

/**
 * e_dialog_button_new_with_icon:
 * @icon_name: Icon's name to use; can be %NULL
 * @label: Button label to set, with mnemonics
 *
 * Creates a new #GtkButton with preset @label and image set
 * to @icon_name.
 *
 * Returns: (transfer-full): A new #GtkButton
 *
 * Since: 3.12
 **/
GtkWidget *
e_dialog_button_new_with_icon (const gchar *icon_name,
                               const gchar *label)
{
	GtkIconSize icon_size = GTK_ICON_SIZE_BUTTON;
	GtkWidget *button;

	if (label && *label) {
		button = gtk_button_new_with_mnemonic (label);
	} else {
		button = gtk_button_new ();
		icon_size = GTK_ICON_SIZE_MENU;
	}

	if (icon_name)
		gtk_button_set_image (
			GTK_BUTTON (button),
			gtk_image_new_from_icon_name (icon_name, icon_size));

	gtk_widget_show (button);

	return button;
}

static GtkWidget *
dialog_widgets_construct_time_units_combo (void)
{
	struct _units {
		const gchar *nick;
		const gchar *caption;
	} units[4] = {
		/* Translators: This is part of: "Do not synchronize locally mails older than [ xxx ] [ days ]" */
		{ "days", NC_("time-unit", "days") },
		/* Translators: This is part of: "Do not synchronize locally mails older than [ xxx ] [ weeks ]" */
		{ "weeks", NC_("time-unit", "weeks") },
		/* Translators: This is part of: "Do not synchronize locally mails older than [ xxx ] [ months ]" */
		{ "months", NC_("time-unit", "months") },
		/* Translators: This is part of: "Do not synchronize locally mails older than [ xxx ] [ years ]" */
		{ "years", NC_("time-unit", "years") }
	};
	gint ii;
	GtkCellRenderer *renderer;
	GtkListStore *store;
	GtkWidget *combo;

	/* 0: 'nick', 1: caption */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	for (ii = 0; ii < G_N_ELEMENTS (units); ii++) {
		GtkTreeIter iter;
		const gchar *caption;

		/* Localize the caption. */
		caption = g_dpgettext2 (GETTEXT_PACKAGE, "time-unit", units[ii].caption);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, units[ii].nick, 1, caption, -1);
	}

	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	gtk_combo_box_set_id_column (GTK_COMBO_BOX (combo), 0);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", 1, NULL);

	g_object_unref (store);

	return combo;
}

/**
 * e_dialog_offline_settings_new_limit_box:
 * @offline_settings: a #CamelOfflineSettings
 *
 * Creates a new horizontal #GtkBox, which contains widgets
 * to configure @offline_settings properties limit-by-age,
 * limit-unit and limit-value.
 *
 * Returns: (transfer full): a new #GtkBox
 *
 * Since: 3.24
 **/
GtkWidget *
e_dialog_offline_settings_new_limit_box (CamelOfflineSettings *offline_settings)
{
	GtkAdjustment *adjustment;
	GtkWidget *hbox, *spin, *combo;
	GtkWidget *prefix;

	g_return_val_if_fail (CAMEL_IS_OFFLINE_SETTINGS (offline_settings), NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_widget_show (hbox);

	/* Translators: This is part of: "Do not synchronize locally mails older than [ xxx ] [ days ]" */
	prefix = gtk_check_button_new_with_mnemonic (_("Do not synchronize locally mails older than"));
	gtk_box_pack_start (GTK_BOX (hbox), prefix, FALSE, TRUE, 0);
	gtk_widget_show (prefix);

	e_binding_bind_property (
		offline_settings, "limit-by-age",
		prefix, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	adjustment = gtk_adjustment_new (1.0, 1.0, 999.0, 1.0, 1.0, 0.0);

	spin = gtk_spin_button_new (adjustment, 1.0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);
	gtk_widget_show (spin);

	e_binding_bind_property (
		offline_settings, "limit-value",
		spin, "value",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		prefix, "active",
		spin, "sensitive",
		G_BINDING_SYNC_CREATE);

	combo = dialog_widgets_construct_time_units_combo ();
	gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
	gtk_widget_show (combo);

	e_binding_bind_property_full (
		offline_settings, "limit-unit",
		combo, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, NULL);

	e_binding_bind_property (
		prefix, "active",
		combo, "sensitive",
		G_BINDING_SYNC_CREATE);

	return hbox;
}

static CamelThreeState
edw_three_state_source_to_camel (EThreeState value)
{
	switch (value) {
	case E_THREE_STATE_OFF:
		return CAMEL_THREE_STATE_OFF;
	case E_THREE_STATE_ON:
		return CAMEL_THREE_STATE_ON;
	case E_THREE_STATE_INCONSISTENT:
		return CAMEL_THREE_STATE_INCONSISTENT;
	}

	return CAMEL_THREE_STATE_INCONSISTENT;
}

static EThreeState
edw_three_state_camel_to_source (CamelThreeState value)
{
	switch (value) {
	case CAMEL_THREE_STATE_OFF:
		return E_THREE_STATE_OFF;
	case CAMEL_THREE_STATE_ON:
		return E_THREE_STATE_ON;
	case CAMEL_THREE_STATE_INCONSISTENT:
		return E_THREE_STATE_INCONSISTENT;
	}

	return E_THREE_STATE_INCONSISTENT;
}

static gboolean
edw_three_state_to_sensitive_cb (GBinding *binding,
				 const GValue *from_value,
				 GValue *to_value,
				 gpointer user_data)
{
	CamelThreeState value;
	GObject *source = g_binding_dup_source (binding);

	if (CAMEL_IS_FOLDER (source)) {
		value = g_value_get_enum (from_value);
	} else {
		value = edw_three_state_source_to_camel (g_value_get_enum (from_value));
	}

	g_value_set_boolean (to_value, value == CAMEL_THREE_STATE_ON);
	g_clear_object (&source);

	return TRUE;
}

static gboolean
edw_mark_seen_timeout_to_double_cb (GBinding *binding,
				    const GValue *from_value,
				    GValue *to_value,
				    gpointer user_data)
{
	gint value;

	value = g_value_get_int (from_value);
	g_value_set_double (to_value, ((gdouble) value) / 1000.0);

	return TRUE;
}

static gboolean
edw_double_to_mark_seen_timeout_cb (GBinding *binding,
				    const GValue *from_value,
				    GValue *to_value,
				    gpointer user_data)
{
	gdouble value;

	value = g_value_get_double (from_value);
	g_value_set_int (to_value, value * 1000);

	return TRUE;
}

typedef struct _ThreeStateData {
	GObject *object;
	gulong handler_id;
} ThreeStateData;

static void
three_state_data_free (gpointer data,
		       GClosure *closure)
{
	ThreeStateData *tsd = data;

	if (tsd) {
		g_clear_object (&tsd->object);
		g_free (tsd);
	}
}

static void
edw_three_state_toggled_cb (GtkToggleButton *widget,
			    gpointer user_data)
{
	ThreeStateData *tsd = user_data;
	CamelThreeState set_to;

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));
	g_return_if_fail (tsd != NULL);

	g_signal_handler_block (widget, tsd->handler_id);

	if (gtk_toggle_button_get_inconsistent (widget) &&
	    gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_active (widget, FALSE);
		gtk_toggle_button_set_inconsistent (widget, FALSE);
		set_to = CAMEL_THREE_STATE_OFF;
	} else if (!gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_inconsistent (widget, TRUE);
		gtk_toggle_button_set_active (widget, FALSE);
		set_to = CAMEL_THREE_STATE_INCONSISTENT;
	} else {
		set_to = CAMEL_THREE_STATE_ON;
	}

	if (CAMEL_IS_FOLDER (tsd->object)) {
		g_object_set (G_OBJECT (tsd->object), "mark-seen", set_to, NULL);
	} else {
		g_object_set (G_OBJECT (tsd->object),
			"mark-seen", edw_three_state_camel_to_source (set_to),
			NULL);
	}

	g_signal_handler_unblock (widget, tsd->handler_id);
}

/**
 * e_dialog_new_mark_seen_box:
 * @object: either #CamelFolder or #ESourceMailAccount
 *
 * Returns: (transfer full): a new #GtkBox containing widgets to
 *   setup "[x] Mark messages as read after [spin button] seconds"
 *   option using @object properties "mark-seen" and "mark-seen-timeout".
 *
 * Since: 3.32
 **/
GtkWidget *
e_dialog_new_mark_seen_box (gpointer object)
{
	/* Translators: The %s is replaced with a spin button; always keep it in the string at the right position */
	const gchar *blurb = _("Mark messages as read after %s seconds");
	GtkWidget *hbox, *widget;
	CamelThreeState three_state = CAMEL_THREE_STATE_INCONSISTENT;
	ThreeStateData *tsd;
	gboolean set_inconsistent = FALSE, set_active = FALSE;
	gchar **strv;

	g_return_val_if_fail (CAMEL_IS_FOLDER (object) || E_IS_SOURCE_MAIL_ACCOUNT (object), NULL);

	if (CAMEL_IS_FOLDER (object)) {
		three_state = camel_folder_get_mark_seen (object);
	} else {
		three_state = edw_three_state_source_to_camel (e_source_mail_account_get_mark_seen (object));
	}

	switch (three_state) {
		case CAMEL_THREE_STATE_ON:
			set_inconsistent = FALSE;
			set_active = TRUE;
			break;
		case CAMEL_THREE_STATE_OFF:
			set_inconsistent = FALSE;
			set_active = FALSE;
			break;
		case CAMEL_THREE_STATE_INCONSISTENT:
			set_inconsistent = TRUE;
			set_active = FALSE;
			break;
	}

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_show (hbox);

	strv = g_strsplit (blurb, "%s", -1);
	g_warn_if_fail (strv && strv[0] && strv[1] && !strv[2]);

	widget = gtk_check_button_new_with_mnemonic (strv && strv[0] ? strv[0] : "Mark messages as read after ");

	g_object_set (G_OBJECT (widget),
		"inconsistent", set_inconsistent,
		"active", set_active,
		NULL);

	tsd = g_new0 (ThreeStateData, 1);
	tsd->object = g_object_ref (object);
	tsd->handler_id = g_signal_connect_data (widget, "toggled",
		G_CALLBACK (edw_three_state_toggled_cb),
		tsd, three_state_data_free, 0);

	gtk_widget_show (widget);

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	widget = gtk_spin_button_new_with_range (0.0, 10.0, 1.0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (widget), TRUE);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 1);
	e_binding_bind_property_full (object, "mark-seen",
		widget, "sensitive",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
		edw_three_state_to_sensitive_cb,
		NULL, NULL, NULL);
	e_binding_bind_property_full (object, "mark-seen-timeout",
		widget, "value",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
		edw_mark_seen_timeout_to_double_cb,
		edw_double_to_mark_seen_timeout_cb,
		NULL, NULL);
	gtk_widget_show (widget);

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	widget = gtk_label_new (strv && strv[0] && strv[1] ? strv[1] : " seconds");
	gtk_widget_show (widget);

	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	g_strfreev (strv);

	return hbox;
}
