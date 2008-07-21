/*
 * Copyright 2008, Joergen Scheibengruber <joergen.scheibengruber@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
    ESourceGroup *group;

    source_list = e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");

    if (source_list == NULL) {
        return;
    }

    group = e_source_list_peek_group_by_name (source_list, _("Google"));

    if (group == NULL) {
        gboolean res;

        group = e_source_group_new (_("Google"), "google://");
        res = e_source_list_add_group (source_list, group, -1);

        if (res == FALSE) {
            g_warning ("Could not add Google source group!");
        } else {
            e_source_list_sync (source_list, NULL);
        }

        g_object_unref (group);
    }
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

    group = e_source_list_peek_group_by_name (source_list, _("Google"));

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
    const char *text;
    char *username;

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
on_update_cb_toggled (GtkToggleButton *tb, gpointer user_data)
{
    ESource *source = user_data;
    GtkWidget *sb = g_object_get_data (G_OBJECT (tb), "sb");

    gtk_widget_set_sensitive (sb, gtk_toggle_button_get_active (tb));
    if (gtk_toggle_button_get_active (tb)) {
        gdouble value;
        char *value_string;

        value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (sb));
        value_string = g_strdup_printf ("%d", (int)(value * 60.0));
        e_source_set_property (source, "refresh-interval", value_string);
        g_free (value_string);
    } else {
        e_source_set_property (source, "refresh-interval", "-1");
    }
}

static void
on_interval_sb_value_changed (GtkSpinButton *sb, gpointer user_data)
{
    ESource *source = user_data;
    gdouble value;
    char *value_string;

    value = gtk_spin_button_get_value (sb);
    value_string = g_strdup_printf ("%d", (int)(value * 60.0));
    e_source_set_property (source, "refresh-interval", value_string);
    g_free (value_string);
}

GtkWidget *
plugin_google_contacts (EPlugin                    *epl,
                        EConfigHookItemFactoryData *data)
{
    EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
    ESource      *source;
    ESourceGroup *group;
    const char   *base_uri;
    const char   *username;
    const char   *refresh_interval_str;
    int           refresh_interval;
    GtkWidget    *parent;
    GtkWidget    *vbox;

    GtkWidget    *section;
    GtkWidget    *vbox2;

    GtkWidget    *hbox;
    GtkWidget    *spacer;
    GtkWidget    *label;
    GtkWidget    *username_entry;

    GtkWidget    *update_cb;
    GtkWidget    *interval_sb;


    source = t->source;
    group = e_source_peek_group (source);

    base_uri = e_source_group_peek_base_uri (group);

    g_object_set_data_full (G_OBJECT (epl), "widget", NULL,
                            (GDestroyNotify)gtk_widget_destroy);

    if (strcmp (base_uri, "google://")) {
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

    label = gtk_label_new (_("Username:"));
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

    refresh_interval_str = e_source_get_property (source, "refresh-interval");
    if (refresh_interval_str &&
        (1 == sscanf (refresh_interval_str, "%d", &refresh_interval))) {
    } else {
        refresh_interval = -1;
    }
    update_cb = gtk_check_button_new_with_label (_("Update every"));
    gtk_box_pack_start (GTK_BOX (hbox), update_cb, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (update_cb),
                                  refresh_interval > 0);

    interval_sb = gtk_spin_button_new_with_range (1, 60, 1);
    gtk_widget_set_sensitive (interval_sb,
                              refresh_interval > 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (interval_sb),
                               refresh_interval > 0 ? refresh_interval / 60 : 30);
    gtk_box_pack_start (GTK_BOX (hbox), interval_sb, FALSE, FALSE, 0);

    label = gtk_label_new (_("minute(s)"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    gtk_widget_show_all (vbox2);

    g_object_set_data (G_OBJECT (update_cb), "sb", interval_sb);
    g_object_set_data_full (G_OBJECT (epl), "widget", vbox2,
                            (GDestroyNotify)gtk_widget_destroy);

    g_signal_connect (G_OBJECT (username_entry), "changed",
                      G_CALLBACK (on_username_entry_changed),
                      source);
    g_signal_connect (G_OBJECT (update_cb), "toggled",
                      G_CALLBACK (on_update_cb_toggled),
                      source);
    g_signal_connect (G_OBJECT (interval_sb), "value-changed",
                      G_CALLBACK (on_interval_sb_value_changed),
                      source);

    return NULL;
}


