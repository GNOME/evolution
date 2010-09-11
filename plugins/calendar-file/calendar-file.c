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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <gtk/gtk.h>
#include <e-util/e-config.h>
#include <e-util/e-plugin-util.h>
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-source.h>
#include <glib/gi18n.h>
#include <string.h>

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

static void
location_changed (GtkFileChooserButton *widget, ESource *source)
{
	gchar *filename;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (source != NULL);

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	e_source_set_property (source, "custom-file", (filename && *filename) ? filename : NULL);
	g_free (filename);
}

static void
maincheck_toggled (GtkToggleButton *check, ESource *source)
{
	GtkWidget *w;
	gboolean enabled = gtk_toggle_button_get_active (check);

	w = g_object_get_data (G_OBJECT (check), "child");
	gtk_widget_set_sensitive (w, enabled);

	/* used with source = NULL to enable widgets only */
	if (!source)
		return;

	if (enabled) {
		gchar *file;

		w = g_object_get_data (G_OBJECT (check), "file-chooser");
		file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (w));
		e_source_set_property (source, "custom-file", (file && *file) ? file : NULL);
		g_free (file);
	} else {
		e_source_set_property (source, "custom-file", NULL);
	}
}

static void
refresh_type_changed (GtkComboBox *refresh_type, ESource *source)
{
	GtkWidget *refresh_hbox;
	gint active = gtk_combo_box_get_active (refresh_type);
	gchar buff[2] = {0};

	refresh_hbox = g_object_get_data (G_OBJECT (refresh_type), "refresh-hbox");

	if (active < 0 || active > 2)
		active = 0;

	if (active == 2) {
		gtk_widget_show (refresh_hbox);
	} else {
		gtk_widget_hide (refresh_hbox);
	}

	buff[0] = '0' + active;
	e_source_set_property (source, "refresh-type", buff);
}

GtkWidget *e_calendar_file_customs (EPlugin *epl, EConfigHookItemFactoryData *data);

GtkWidget *
e_calendar_file_customs (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	const gchar *relative_uri, *value;
	guint n_rows;
	GtkWidget *w1, *w2, *box1, *box2, *mainbox, *maincheck;

	if (!e_plugin_util_is_source_proto (source, "local"))
		return NULL;

	relative_uri = e_source_peek_relative_uri (source);
	if (relative_uri && g_str_equal (relative_uri, "system"))
		return NULL;

	e_source_set_relative_uri (source, e_source_peek_uid (source));

	mainbox = gtk_vbox_new (FALSE, 2);
	g_object_get (data->parent, "n-rows", &n_rows, NULL);
	gtk_table_attach (
		GTK_TABLE (data->parent), mainbox,
		1, 2, n_rows, n_rows + 1,
		GTK_EXPAND | GTK_FILL, 0, 0, 0);

	maincheck = gtk_check_button_new_with_mnemonic (_("_Customize options"));
	gtk_box_pack_start ((GtkBox *)mainbox, maincheck, TRUE, TRUE, 2);

	box1 = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start ((GtkBox *)mainbox, box1, TRUE, TRUE, 2);

	g_object_set_data ((GObject*)maincheck, "child", box1);

	/* left-most space, the first one */
	w1 = gtk_label_new ("");
	gtk_box_pack_start ((GtkBox *)box1, w1, FALSE, TRUE, 8);

	box2 = gtk_vbox_new (FALSE, 2);
	gtk_box_pack_start ((GtkBox *)box1, box2, TRUE, TRUE, 2);

	box1 = box2;
	box2 = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start ((GtkBox *)box1, box2, TRUE, TRUE, 2);

	w1 = gtk_label_new_with_mnemonic (_("File _name:"));
	gtk_misc_set_alignment (GTK_MISC (w1), 0.0, 0.5);
	gtk_box_pack_start ((GtkBox *)box2, w1, FALSE, TRUE, 2);

	w2 = gtk_file_chooser_button_new (_("Choose calendar file"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (w2), TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (w1), w2);
	gtk_box_pack_start ((GtkBox *)box2, w2, TRUE, TRUE, 2);

	g_object_set_data (G_OBJECT (maincheck), "file-chooser", w2);

	value = e_source_get_property (source, "custom-file");
	if (value && *value) {
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (w2), value);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (maincheck), TRUE);
	} else {
		gchar *file = NULL;
		const gchar *file_name = NULL;

		switch (t->source_type) {
		case E_CAL_SOURCE_TYPE_EVENT:
			file_name = "calendar.ics";
			break;
		case E_CAL_SOURCE_TYPE_TODO:
			file_name = "tasks.ics";
			break;
		case E_CAL_SOURCE_TYPE_JOURNAL:
			file_name = "journal.ics";
			break;
		case E_CAL_SOURCE_TYPE_LAST:
			break;
		}

		file = g_build_filename (g_get_home_dir (), file_name, NULL);
		if (file && *file)
			gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (w2), file);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (maincheck), FALSE);
		g_free (file);
	}
	maincheck_toggled (GTK_TOGGLE_BUTTON (maincheck), NULL);

	g_signal_connect (G_OBJECT (w2), "file-set", G_CALLBACK (location_changed), source);
	g_signal_connect (G_OBJECT (maincheck), "toggled", G_CALLBACK (maincheck_toggled), source);

	box2 = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start ((GtkBox *)box1, box2, FALSE, TRUE, 2);

	w1 = gtk_label_new_with_mnemonic (_("Re_fresh:"));
	gtk_misc_set_alignment (GTK_MISC (w1), 0.0, 0.5);
	gtk_box_pack_start ((GtkBox *)box2, w1, FALSE, TRUE, 2);

	w2 = gtk_combo_box_new_text ();
	gtk_combo_box_append_text ((GtkComboBox *)w2, _("On open"));
	gtk_combo_box_append_text ((GtkComboBox *)w2, _("On file change"));
	gtk_combo_box_append_text ((GtkComboBox *)w2, _("Periodically"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (w1), w2);
	gtk_box_pack_start ((GtkBox *)box2, w2, FALSE, TRUE, 2);

	value = e_source_get_property (source, "refresh-type");
	gtk_combo_box_set_active ((GtkComboBox *)w2, (value && *value && !value[1] && value[0] >= '0' && value[0] <= '2') ? value[0] - '0' : 0);

	w1 = w2;
	w2 = e_plugin_util_add_refresh (NULL, NULL, source, "refresh");
	gtk_box_pack_start (GTK_BOX (box2), w2, FALSE, TRUE, 0);

	g_object_set_data (G_OBJECT (w1), "refresh-hbox", w2);

	g_signal_connect (G_OBJECT (w1), "changed", G_CALLBACK (refresh_type_changed), source);

	w2 = e_plugin_util_add_check (NULL, _("Force read _only"), source, "custom-file-readonly", "1", NULL);
	gtk_box_pack_start ((GtkBox *)box1, w2, TRUE, TRUE, 2);

	gtk_widget_show_all (mainbox);

	/* w1 is a refresh-type combobox, and it hides widgets,
	   thus should be called after show_all call */
	refresh_type_changed (GTK_COMBO_BOX (w1), source);

	return mainbox;
}
