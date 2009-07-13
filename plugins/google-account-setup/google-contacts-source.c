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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright 2008, Joergen Scheibengruber <joergen.scheibengruber@googlemail.com>
 *
 */

#include <config.h>
#include <string.h>

#include <glib/gi18n-lib.h>
#include <glib.h>

#include <gtk/gtk.h>

#include <e-util/e-config.h>
#include <e-util/e-plugin.h>
#include <addressbook/gui/widgets/eab-config.h>

#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-account-list.h>

#include "google-contacts-source.h"

void
ensure_google_contacts_source_group (void)
{
	ESourceList  *source_list;

	source_list = e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");

	if (source_list == NULL) {
		return;
	}

	e_source_list_ensure_group (source_list, _("Google"), "google://", FALSE);
	g_object_unref (source_list);
}

void
remove_google_contacts_source_group (void)
{
    ESourceList  *source_list;
    ESourceGroup *group;

    source_list = e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");

    if (source_list == NULL) {
        return;
    }

    group = e_source_list_peek_group_by_base_uri (source_list, "google://");

    if (group) {
        GSList *sources;

        sources = e_source_group_peek_sources (group);

        if (NULL == sources) {
            e_source_list_remove_group (source_list, group);
            e_source_list_sync (source_list, NULL);
        }
    }
    g_object_unref (source_list);
}

static void
on_username_entry_changed (GtkEntry *entry, gpointer user_data)
{
    ESource *source = user_data;
    const gchar *text;
    gchar *username;

    text = gtk_entry_get_text (entry);

    if (strstr (text, "@")) {
        username = g_strdup (text);
    } else {
        username = g_strdup_printf ("%s@gmail.com", text);
    }

    e_source_set_relative_uri (source, username);
    e_source_set_property (source, "username", username);
    e_source_set_property (source, "auth", "plain/password");
    g_free (username);
}

static void
on_ssl_cb_toggled (GtkToggleButton *tb, gpointer user_data)
{
    ESource *source = user_data;

    if (gtk_toggle_button_get_active (tb)) {
        e_source_set_property (source, "use-ssl", "true");
    } else {
        e_source_set_property (source, "use-ssl", "false");
    }
}

typedef enum {
    MINUTES,
    HOURS,
    DAYS,
    WEEKS
} IntervalType;

static void
seconds_to_interval (guint seconds, IntervalType *type, gint *time)
{
    gint minutes = seconds / 60;

    *type = MINUTES;
    *time = minutes;
    if (minutes  && !(minutes % 10080)) {
        *type = WEEKS;
        *time = minutes / 10080;
    } else if (minutes && !(minutes % 1440)) {
        *type = DAYS;
        *time = minutes / 1440;
    } else if (minutes && !(minutes % 60)) {
        *type = HOURS;
        *time = minutes / 60;
    }
}

static guint
interval_to_seconds (IntervalType type, gint time)
{
    switch (type) {
    case MINUTES:
        return time * 60;
    case HOURS:
        return time * 60 * 60;
    case DAYS:
        return time * 60 * 60 * 24;
    case WEEKS:
        return time * 60 * 60 * 24 * 7;
    default:
        g_warning ("Time unit out of range");
        break;
    }
    return 0;
}

static void
on_interval_sb_value_changed (GtkSpinButton *sb, gpointer user_data)
{
    ESource *source = user_data;
    gdouble time;
    guint seconds;
    gchar *value_string;
    GtkWidget *interval_combo;
    IntervalType type;

    interval_combo = g_object_get_data (G_OBJECT (sb), "interval-combo");
    type = gtk_combo_box_get_active (GTK_COMBO_BOX (interval_combo));

    time = gtk_spin_button_get_value (sb);

    seconds = interval_to_seconds (type, time);

    value_string = g_strdup_printf ("%u", seconds);
    e_source_set_property (source, "refresh-interval", value_string);
    g_free (value_string);
}

static void
on_interval_combo_changed (GtkComboBox *combo, gpointer user_data)
{
    ESource *source = user_data;
    gdouble time;
    guint seconds;
    gchar *value_string;
    GtkWidget *sb;
    IntervalType type;

    sb = g_object_get_data (G_OBJECT (combo), "interval-sb");
    type = gtk_combo_box_get_active (combo);

    time = gtk_spin_button_get_value (GTK_SPIN_BUTTON (sb));

    seconds = interval_to_seconds (type, time);

    value_string = g_strdup_printf ("%u", seconds);
    e_source_set_property (source, "refresh-interval", value_string);
    g_free (value_string);
}

struct ui_data {
	GtkWidget *widget;
};

static void
destroy_ui_data(gpointer data)
{
	struct ui_data *ui = data;

	if (ui && ui->widget)
		gtk_widget_destroy (ui->widget);

	g_free (ui);
}

GtkWidget *
plugin_google_contacts (EPlugin                    *epl,
                        EConfigHookItemFactoryData *data)
{
    EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
    ESource      *source;
    ESourceGroup *group;
    const gchar   *base_uri;
    const gchar   *username;
    const gchar   *refresh_interval_str;
    guint         refresh_interval;
    const gchar   *use_ssl_str;
    gboolean      use_ssl;
    GtkWidget    *parent;
    GtkWidget    *vbox;

    GtkWidget    *section;
    GtkWidget    *vbox2;

    GtkWidget    *hbox;
    GtkWidget    *spacer;
    GtkWidget    *label;
    GtkWidget    *username_entry;

    GtkWidget    *interval_sb;
    GtkWidget    *interval_combo;
    IntervalType type;
    gint        time;

    GtkWidget    *ssl_cb;
    struct ui_data *ui;

    source = t->source;
    group = e_source_peek_group (source);

    base_uri = e_source_group_peek_base_uri (group);

    g_object_set_data (G_OBJECT (epl), "gwidget", NULL);

    if (g_ascii_strncasecmp ("google://", base_uri, 9) != 0) {
        return NULL;
    }

    /* Build up the UI */
    parent = data->parent;
    vbox = gtk_widget_get_ancestor (gtk_widget_get_parent (parent), GTK_TYPE_VBOX);

    vbox2 = gtk_vbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

    section = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (section), _("<b>Server</b>"));
    gtk_misc_set_alignment (GTK_MISC (section), 0.0, 0.0);
    gtk_box_pack_start (GTK_BOX (vbox2), section, FALSE, FALSE, 0);

    hbox = gtk_hbox_new (FALSE, 10);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox, TRUE, TRUE, 0);

    spacer = gtk_label_new ("   ");
    gtk_box_pack_start (GTK_BOX (hbox), spacer, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic (_("User_name:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    username_entry = gtk_entry_new ();
    username = e_source_get_property (source, "username");
    if (username) {
        gtk_entry_set_text (GTK_ENTRY (username_entry), username);
    }
    gtk_box_pack_start (GTK_BOX (hbox), username_entry, TRUE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, 10);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox, TRUE, TRUE, 0);

    spacer = gtk_label_new ("   ");
    gtk_box_pack_start (GTK_BOX (hbox), spacer, FALSE, FALSE, 0);

    use_ssl_str = e_source_get_property (source, "use-ssl");
    if (use_ssl_str && ('1' == use_ssl_str[0] ||
        0 == g_ascii_strcasecmp (use_ssl_str, "true"))) {
        use_ssl = 1;
    } else {
        use_ssl = 0;
    }
    ssl_cb = gtk_check_button_new_with_mnemonic (_("Use _SSL"));
    gtk_box_pack_start (GTK_BOX (hbox), ssl_cb, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ssl_cb),
                                  use_ssl);

    hbox = gtk_hbox_new (FALSE, 10);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox, TRUE, TRUE, 0);

    spacer = gtk_label_new ("   ");
    gtk_box_pack_start (GTK_BOX (hbox), spacer, FALSE, FALSE, 0);

    refresh_interval_str = e_source_get_property (source, "refresh-interval");
    if (refresh_interval_str &&
        (1 == sscanf (refresh_interval_str, "%u", &refresh_interval))) {
    } else {
        refresh_interval = -1;
    }
    seconds_to_interval (refresh_interval, &type, &time);

    label = gtk_label_new_with_mnemonic (_("Re_fresh:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    interval_sb = gtk_spin_button_new_with_range (1, 100, 1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (interval_sb), time);
    gtk_box_pack_start (GTK_BOX (hbox), interval_sb, FALSE, FALSE, 0);

    interval_combo = gtk_combo_box_new_text ();
    gtk_combo_box_append_text (GTK_COMBO_BOX (interval_combo), _("minutes"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (interval_combo), _("hours"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (interval_combo), _("days"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (interval_combo), _("weeks"));
    gtk_combo_box_set_active (GTK_COMBO_BOX (interval_combo), type);
    gtk_box_pack_start (GTK_BOX (hbox), interval_combo, FALSE, FALSE, 0);

    gtk_widget_show_all (vbox2);

    g_object_set_data (G_OBJECT (interval_sb), "interval-combo", interval_combo);
    g_object_set_data (G_OBJECT (interval_combo), "interval-sb", interval_sb);

    ui = g_malloc0 (sizeof (struct ui_data));
    ui->widget = vbox2;
    g_object_set_data_full(G_OBJECT(epl), "gwidget", ui, destroy_ui_data);
    g_signal_connect (ui->widget, "destroy", G_CALLBACK (gtk_widget_destroyed), &ui->widget);

    g_signal_connect (G_OBJECT (username_entry), "changed",
                      G_CALLBACK (on_username_entry_changed),
                      source);
    g_signal_connect (G_OBJECT (interval_combo), "changed",
                      G_CALLBACK (on_interval_combo_changed),
                      source);
    g_signal_connect (G_OBJECT (ssl_cb), "toggled",
                      G_CALLBACK (on_ssl_cb_toggled),
                      source);
    g_signal_connect (G_OBJECT (interval_sb), "value-changed",
                      G_CALLBACK (on_interval_sb_value_changed),
                      source);

    return NULL;
}

