/*
 *
 * Copyright (C) 2008 Matthias Braun <matze@braunis.de>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib/gi18n-lib.h>
#include <glib.h>

#include <gtk/gtk.h>

#include <e-util/e-config.h>
#include <e-util/e-plugin.h>
#include <e-util/e-plugin-util.h>
#include <addressbook/gui/widgets/eab-config.h>

#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-account-list.h>

#define BASE_URI "webdav://"

typedef struct {
	ESource         *source;
	GtkWidget       *box;
	GtkEntry        *url_entry;
	GtkEntry        *username_entry;
	GtkToggleButton *avoid_ifmatch_toggle;
} ui_data;

GtkWidget *
plugin_webdav_contacts(EPlugin *epl, EConfigHookItemFactoryData *data);

gint
e_plugin_lib_enable(EPlugin *ep, gint enable);

static void
ensure_webdav_contacts_source_group(void)
{
	ESourceList  *source_list;

	source_list = e_source_list_new_for_gconf_default("/apps/evolution/addressbook/sources");

	if (source_list == NULL) {
		return;
	}

	e_source_list_ensure_group (source_list, _("WebDAV"), BASE_URI, FALSE);
	g_object_unref (source_list);
}

static void
remove_webdav_contacts_source_group(void)
{
	ESourceList  *source_list;
	ESourceGroup *group;

	source_list = e_source_list_new_for_gconf_default("/apps/evolution/addressbook/sources");

	if (source_list == NULL) {
		return;
	}

	group = e_source_list_peek_group_by_base_uri (source_list, BASE_URI);

	if (group) {
		GSList *sources;

		sources = e_source_group_peek_sources(group);

		if (NULL == sources) {
			e_source_list_remove_group(source_list, group);
			e_source_list_sync(source_list, NULL);
		}
	}
	g_object_unref(source_list);
}

static void
set_ui_from_source(ui_data *data)
{
	ESource    *source  = data->source;
	const gchar *url     = e_source_get_uri(source);
	SoupURI     *suri    = soup_uri_new (url);
	gchar       *url_ui;
	const gchar *property;
	gboolean    use_ssl;
	gboolean    avoid_ifmatch;

	property = e_source_get_property(source, "use_ssl");
	if (property != NULL && strcmp(property, "1") == 0) {
		use_ssl = TRUE;
	} else {
		use_ssl = FALSE;
	}

	property = e_source_get_property(source, "avoid_ifmatch");
	if (property != NULL && strcmp(property, "1") == 0) {
		avoid_ifmatch = TRUE;
	} else {
		avoid_ifmatch = FALSE;
	}
	gtk_toggle_button_set_active(data->avoid_ifmatch_toggle, avoid_ifmatch);

	/* it's really a http or https protocol */
	if (suri)
		soup_uri_set_scheme (suri, use_ssl ? "https" : "http");

	/* remove user/username and set user field */
	if (suri && suri->user != NULL) {
		gtk_entry_set_text (data->username_entry, suri->user);
		soup_uri_set_user (suri, NULL);
	} else {
		gtk_entry_set_text(data->username_entry, "");
	}

	if (suri)
		url_ui = soup_uri_to_string (suri, FALSE);
	else
		url_ui = g_strdup ("");
	gtk_entry_set_text (data->url_entry, url_ui);
	g_free (url_ui);

	if (suri)
		soup_uri_free (suri);
}

static void
set_source_from_ui (ui_data *data)
{
	ESource    *source        = data->source;
	gboolean    avoid_ifmatch = gtk_toggle_button_get_active(data->avoid_ifmatch_toggle);
	const gchar *url           = gtk_entry_get_text(data->url_entry);
	SoupURI     *suri          = soup_uri_new (url);
	gchar       *url_noprotocol;
	gboolean    use_ssl;

	if (!suri)
		return;

	e_source_set_property(source, "avoid_ifmatch", avoid_ifmatch ? "1" : "0");

	/* put username into uri */
	soup_uri_set_user (suri, gtk_entry_get_text (data->username_entry));

	if (suri->user && *suri->user) {
		e_source_set_property(source, "auth", "plain/password");
		e_source_set_property(source, "username", suri->user);
	} else {
		e_source_set_property(source, "auth", NULL);
		e_source_set_property(source, "username", NULL);
	}

	/* set use_ssl based on protocol in URL */
	if (suri->scheme && g_str_equal (suri->scheme, "https")) {
		use_ssl = TRUE;
	} else {
		use_ssl = FALSE;
	}
	e_source_set_property(source, "use_ssl", use_ssl ? "1" : "0");

	url_noprotocol = e_plugin_util_uri_no_proto (suri);
	e_source_set_relative_uri (source, url_noprotocol);
	g_free (url_noprotocol);
	soup_uri_free (suri);
}

static void
destroy_ui_data (gpointer data)
{
	ui_data *ui = data;

	if (ui && ui->box)
		gtk_widget_destroy (ui->box);

	g_free (ui);
}

GtkWidget *
plugin_webdav_contacts(EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESource      *source;
	GtkWidget    *parent;
	GtkWidget    *vbox;

	GtkWidget    *section;
	GtkWidget    *vbox2;

	GtkBox       *hbox;
	GtkWidget    *spacer;
	GtkWidget    *label;

	ui_data      *uidata;
	gchar        *buff;

	source = t->source;

	g_object_set_data (G_OBJECT (epl), "wwidget", NULL);

	if (!e_plugin_util_is_group_proto (e_source_peek_group (source), BASE_URI)) {
		return NULL;
	}

	uidata         = g_malloc0(sizeof(uidata[0]));
	uidata->source = source;

	/* Build up the UI */
	parent = data->parent;
	vbox   = gtk_widget_get_ancestor(gtk_widget_get_parent(parent), GTK_TYPE_VBOX);

	vbox2 = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);

	section = gtk_label_new(NULL);
	buff = g_strconcat ("<b>", _("Server"), "</b>", NULL);
	gtk_label_set_markup(GTK_LABEL(section), buff);
	g_free (buff);
	gtk_misc_set_alignment(GTK_MISC(section), 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(vbox2), section, FALSE, FALSE, 0);

	hbox = GTK_BOX(gtk_hbox_new(FALSE, 10));
	gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(hbox), TRUE, TRUE, 0);

	spacer = gtk_label_new("   ");
	gtk_box_pack_start(hbox, spacer, FALSE, FALSE, 0);

	label = gtk_label_new(_("URL:"));
	gtk_box_pack_start(hbox, label, FALSE, FALSE, 0);

	uidata->url_entry = GTK_ENTRY(gtk_entry_new());
	gtk_box_pack_start(hbox, GTK_WIDGET(uidata->url_entry), TRUE, TRUE, 0);

	hbox = GTK_BOX(gtk_hbox_new(FALSE, 10));
	gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(hbox), TRUE, TRUE, 0);

	spacer = gtk_label_new("   ");
	gtk_box_pack_start(hbox, spacer, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic(_("User_name:"));
	gtk_box_pack_start(hbox, label, FALSE, FALSE, 0);

	uidata->username_entry = GTK_ENTRY(gtk_entry_new());
	gtk_box_pack_start(hbox, GTK_WIDGET(uidata->username_entry), TRUE, TRUE, 0);

	hbox = GTK_BOX(gtk_hbox_new(FALSE, 10));
	gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(hbox), TRUE, TRUE, 0);

	spacer = gtk_label_new("   ");
	gtk_box_pack_start(hbox, spacer, FALSE, FALSE, 0);

	uidata->avoid_ifmatch_toggle = GTK_TOGGLE_BUTTON(
			gtk_check_button_new_with_mnemonic(
				_("_Avoid IfMatch (needed on Apache < 2.2.8)")));
	gtk_box_pack_start(hbox, GTK_WIDGET(uidata->avoid_ifmatch_toggle),
			   FALSE, FALSE, 0);

	set_ui_from_source(uidata);

	gtk_widget_show_all(vbox2);

	uidata->box = vbox2;
	g_object_set_data_full(G_OBJECT(epl), "wwidget", uidata, destroy_ui_data);
	g_signal_connect (uidata->box, "destroy", G_CALLBACK (gtk_widget_destroyed), &uidata->box);

	g_signal_connect_swapped (G_OBJECT(uidata->username_entry), "changed", G_CALLBACK (set_source_from_ui), uidata);
	g_signal_connect_swapped (G_OBJECT(uidata->url_entry), "changed", G_CALLBACK (set_source_from_ui), uidata);
	g_signal_connect_swapped (G_OBJECT(uidata->avoid_ifmatch_toggle), "toggled", G_CALLBACK (set_source_from_ui), uidata);

	return NULL;
}

gint
e_plugin_lib_enable(EPlugin *ep, gint enable)
{
	if (enable) {
		ensure_webdav_contacts_source_group();
	} else {
		remove_webdav_contacts_source_group();
	}
	return 0;
}

