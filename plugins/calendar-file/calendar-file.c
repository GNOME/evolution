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
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-url.h>
#include <glib/gi18n.h>
#include <string.h>

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
set_refresh_time (ESource *source, GtkWidget *spin, GtkWidget *combobox)
{
	gint time;
	gint item_num = 0;
	const gchar *refresh_str = e_source_get_property (source, "refresh");
	time = refresh_str ? atoi (refresh_str) : 30;

	if (time  && !(time % 10080)) {
		/* weeks */
		item_num = 3;
		time /= 10080;
	} else if (time && !(time % 1440)) {
		/* days */
		item_num = 2;
		time /= 1440;
	} else if (time && !(time % 60)) {
		/* hours */
		item_num = 1;
		time /= 60;
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), item_num);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), time);
}

static gchar *
get_refresh_minutes (GtkWidget *spin, GtkWidget *combobox)
{
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
		/* weeks - is this *really* necessary? */
		setting *= 10080;
		break;
	default:
		g_warning ("Time unit out of range");
		break;
	}

	return g_strdup_printf ("%d", setting);
}

static void
spin_changed (GtkSpinButton *spin, ESource *source)
{
	gchar *refresh_str;
	GtkWidget *combobox;

	combobox = g_object_get_data (G_OBJECT (spin), "combobox");

	refresh_str = get_refresh_minutes ((GtkWidget *) spin, combobox);
	e_source_set_property (source, "refresh", refresh_str);
	g_free (refresh_str);
}

static void
combobox_changed (GtkComboBox *combobox, ESource *source)
{
	gchar *refresh_str;
	GtkWidget *spin;

	spin = g_object_get_data (G_OBJECT (combobox), "spin");

	refresh_str = get_refresh_minutes (spin, (GtkWidget *) combobox);
	e_source_set_property (source, "refresh", refresh_str);
	g_free (refresh_str);
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
	GtkWidget *spin, *combobox;
	gint active = gtk_combo_box_get_active (refresh_type);
	gchar buff[2] = {0};

	spin = g_object_get_data (G_OBJECT (refresh_type), "spin");
	combobox = g_object_get_data (G_OBJECT (refresh_type), "combobox");

	if (active < 0 || active > 2)
		active = 0;

	if (active == 2) {
		gtk_widget_show (spin);
		gtk_widget_show (combobox);
	} else {
		gtk_widget_hide (spin);
		gtk_widget_hide (combobox);
	}

	buff [0] = '0' + active;
	e_source_set_property (source, "refresh-type", buff);
}

static void
force_readonly_toggled (GtkToggleButton *check, ESource *source)
{
	e_source_set_property (source, "custom-file-readonly", gtk_toggle_button_get_active (check) ? "1" : NULL);
}

GtkWidget *e_calendar_file_customs (EPlugin *epl, EConfigHookItemFactoryData *data);

GtkWidget *
e_calendar_file_customs (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	gchar *uri_text;
	const gchar *relative_uri, *value;
	GtkWidget *w1, *w2, *w3, *box1, *box2, *mainbox, *maincheck;

        uri_text = e_source_get_uri (source);
	if (strncmp (uri_text, "file", 4)) {
		g_free (uri_text);

		return NULL;
	}

	relative_uri = e_source_peek_relative_uri (source);

	if (relative_uri && g_str_equal (relative_uri, "system")) {
		g_free (uri_text);
		return NULL;
	}

	e_source_set_relative_uri (source, e_source_peek_uid (source));

	mainbox = gtk_vbox_new (FALSE, 2);
	gtk_table_attach (GTK_TABLE (data->parent), mainbox, 1, 2, GTK_TABLE (data->parent)->nrows, GTK_TABLE (data->parent)->nrows + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

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
	gtk_combo_box_set_active ((GtkComboBox *)w2, (value && *value && !value[1] && value [0] >= '0' && value [0] <= '2') ? value [0] - '0' : 0);

	w1 = w2;
	w2 = gtk_spin_button_new_with_range (1, 100, 1);
	gtk_box_pack_start (GTK_BOX (box2), w2, FALSE, TRUE, 0);

	w3 = gtk_combo_box_new_text ();
	gtk_combo_box_append_text (GTK_COMBO_BOX (w3), _("minutes"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (w3), _("hours"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (w3), _("days"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (w3), _("weeks"));
	set_refresh_time (source, w2, w3);
	gtk_box_pack_start (GTK_BOX (box2), w3, FALSE, TRUE, 0);

	g_object_set_data (G_OBJECT (w1), "spin", w2);
	g_object_set_data (G_OBJECT (w1), "combobox", w3);
	g_object_set_data (G_OBJECT (w2), "combobox", w3);

	g_signal_connect (G_OBJECT (w1), "changed", G_CALLBACK (refresh_type_changed), source);
	g_signal_connect (G_OBJECT (w2), "value-changed", G_CALLBACK (spin_changed), source);
	g_signal_connect (G_OBJECT (w3), "changed", G_CALLBACK (combobox_changed), source);

	w2 = gtk_check_button_new_with_mnemonic (_("Force read _only"));
	value = e_source_get_property (source, "custom-file-readonly");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w2), value && g_str_equal (value, "1"));
	g_signal_connect (G_OBJECT (w2), "toggled", G_CALLBACK (force_readonly_toggled), source);
	gtk_box_pack_start ((GtkBox *)box1, w2, TRUE, TRUE, 2);

	gtk_widget_show_all (mainbox);

	/* w1 is a refresh-type combobox, and it hides widgets,
	   thus should be called after show_all call */
	refresh_type_changed (GTK_COMBO_BOX (w1), source);
	g_free (uri_text);

	return mainbox;
}
