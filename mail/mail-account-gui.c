/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Dan Winship <danw@ximian.com>
 *    Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <string.h>

#include <gal/widgets/e-unicode.h>

#include "shell/evolution-shell-client.h"
#include "mail-account-gui.h"
#include "mail-session.h"

extern char *default_drafts_folder_uri, *default_sent_folder_uri;

static void save_service (MailAccountGuiService *gsvc, GHashTable *extra_conf, MailConfigService *service);

static gboolean
is_email (const char *address)
{
	const char *at, *hname;

	at = strchr (address, '@');
	/* make sure we have an '@' and that it's not the first or last char */
	if (!at || at == address || *(at + 1) == '\0')
		return FALSE;

	hname = at + 1;
	/* make sure the first and last chars aren't '.' */
	if (*hname == '.' || hname[strlen (hname) - 1] == '.')
		return FALSE;

	return strchr (hname, '.') != NULL;
}

gboolean
mail_account_gui_identity_complete (MailAccountGui *gui)
{
	char *text;

	text = gtk_entry_get_text (gui->full_name);
	if (!text || !*text)
		return FALSE;
	text = gtk_entry_get_text (gui->email_address);
	if (!text || !is_email (text))
		return FALSE;

	return TRUE;
}

static gboolean
service_complete (MailAccountGuiService *service)
{
	const CamelProvider *prov = service->provider;
	char *text;

	if (!prov)
		return TRUE;

	if (CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_HOST)) {
		text = gtk_entry_get_text (service->hostname);
		if (!text || !*text)
			return FALSE;
	}
	if (CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_USER)) {
		text = gtk_entry_get_text (service->username);
		if (!text || !*text)
			return FALSE;
	}
	if (CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_PATH)) {
		text = gtk_entry_get_text (service->path);
		if (!text || !*text)
			return FALSE;
	}

	return TRUE;
}

gboolean
mail_account_gui_source_complete (MailAccountGui *gui)
{
	return service_complete (&gui->source);
}

gboolean
mail_account_gui_transport_complete (MailAccountGui *gui)
{
	if (!service_complete (&gui->transport))
		return FALSE;

	/* FIXME? */
	if (gtk_toggle_button_get_active (gui->transport_needs_auth) &&
	    CAMEL_PROVIDER_ALLOWS (gui->transport.provider, CAMEL_URL_PART_USER)) {
		char *text = gtk_entry_get_text (gui->transport.username);

		if (!text || !*text)
			return FALSE;
	}

	return TRUE;
}

gboolean
mail_account_gui_management_complete (MailAccountGui *gui)
{
	char *text;

	text = gtk_entry_get_text (gui->account_name);
	return text && *text;
}


static void
service_authtype_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountGuiService *service = user_data;
	CamelServiceAuthType *authtype;

	service->authitem = widget;
	authtype = gtk_object_get_data (GTK_OBJECT (widget), "authtype");

	gtk_widget_set_sensitive (GTK_WIDGET (service->remember), authtype->need_password);
}

static void
build_auth_menu (MailAccountGuiService *service, GList *authtypes)
{
	GtkWidget *menu, *item, *first = NULL;
	CamelServiceAuthType *authtype;
	GList *l;

	menu = gtk_menu_new ();

	for (l = authtypes; l; l = l->next) {
		authtype = l->data;

		item = gtk_menu_item_new_with_label (_(authtype->name));
		gtk_object_set_data (GTK_OBJECT (item), "authtype", authtype);
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    service_authtype_changed, service);

		gtk_menu_append (GTK_MENU (menu), item);

		gtk_widget_show (item);

		if (!first)
			first = item;
	}

	gtk_option_menu_remove_menu (service->authtype);
	gtk_option_menu_set_menu (service->authtype, menu);

	if (first)
		gtk_signal_emit_by_name (GTK_OBJECT (first), "activate", service);
}

static void
source_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountGui *gui = user_data;
	GtkWidget *label, *frame, *dwidget = NULL;
	CamelProvider *provider;

	provider = gtk_object_get_data (GTK_OBJECT (widget), "provider");

	gui->source.provider = provider;

	frame = glade_xml_get_widget (gui->xml, "source_frame");
	if (provider) {
		gtk_widget_show (frame);

		/* hostname */
		label = glade_xml_get_widget (gui->xml, "source_host_label");
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_HOST)) {
			dwidget = GTK_WIDGET (gui->source.hostname);
			gtk_widget_show (GTK_WIDGET (gui->source.hostname));
			gtk_widget_show (label);
		} else {
			gtk_widget_hide (GTK_WIDGET (gui->source.hostname));
			gtk_widget_hide (label);
		}

		/* username */
		label = glade_xml_get_widget (gui->xml, "source_user_label");
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_USER)) {
			if (!dwidget)
				dwidget = GTK_WIDGET (gui->source.username);
			gtk_widget_show (GTK_WIDGET (gui->source.username));
			gtk_widget_show (label);
		} else {
			gtk_widget_hide (GTK_WIDGET (gui->source.username));
			gtk_widget_hide (label);
		}

		/* path */
		label = glade_xml_get_widget (gui->xml, "source_path_label");
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_PATH)) {
			if (!dwidget)
				dwidget = GTK_WIDGET (gui->source.path);

			if (!strcmp (provider->protocol, "mbox")) {
				char *path;

				if (getenv ("MAIL"))
					path = g_strdup (getenv ("MAIL"));
				else
					path = g_strdup_printf (SYSTEM_MAIL_DIR "/%s", g_get_user_name ());
				gtk_entry_set_text (gui->source.path, path);
				g_free (path);
			} else if (!strcmp (provider->protocol, "maildir") &&
				   getenv ("MAILDIR")) {
				gtk_entry_set_text (gui->source.path, getenv ("MAILDIR"));
			} else {
				gtk_entry_set_text (gui->source.path, "");
			}

			gtk_widget_show (GTK_WIDGET (gui->source.path));
			gtk_widget_show (label);
		} else {
			gtk_entry_set_text (gui->source.path, "");
			gtk_widget_hide (GTK_WIDGET (gui->source.path));
			gtk_widget_hide (label);
		}

		/* ssl */
#ifdef HAVE_NSS
		if (provider && provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL)
			gtk_widget_show (GTK_WIDGET (gui->source.use_ssl));
		else
#endif
			gtk_widget_hide (GTK_WIDGET (gui->source.use_ssl));

		/* auth */
		frame = glade_xml_get_widget (gui->xml, "source_auth_frame");
		if (provider && CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH)) {
			build_auth_menu (&gui->source, provider->authtypes);
			gtk_widget_show (frame);
		} else
			gtk_widget_hide (frame);
	} else {
		gtk_widget_hide (frame);
		frame = glade_xml_get_widget (gui->xml, "source_auth_frame");
		gtk_widget_hide (frame);
	}

	gtk_signal_emit_by_name (GTK_OBJECT (gui->source.username), "changed");

	if (dwidget)
		gtk_widget_grab_focus (dwidget);
}


static void
transport_needs_auth_toggled (GtkToggleButton *toggle, gpointer data)
{
	MailAccountGui *gui = data;
	gboolean need = gtk_toggle_button_get_active (toggle);
	GtkWidget *widget;

	widget = glade_xml_get_widget (gui->xml, "transport_auth_frame");
	gtk_widget_set_sensitive (widget, need);
}

static void
transport_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountGui *gui = user_data;
	CamelProvider *provider;
	GtkWidget *label, *frame;

	provider = gtk_object_get_data (GTK_OBJECT (widget), "provider");
	gui->transport.provider = provider;

	frame = glade_xml_get_widget (gui->xml, "transport_frame");
	if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_HOST) ||
	    (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH) &&
	     !CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_AUTH))) {
		gtk_widget_show (frame);

		label = glade_xml_get_widget (gui->xml, "transport_host_label");
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_HOST)) {
			gtk_widget_grab_focus (GTK_WIDGET (gui->transport.hostname));
			gtk_widget_show (GTK_WIDGET (gui->transport.hostname));
			gtk_widget_show (label);
		} else {
			gtk_widget_hide (GTK_WIDGET (gui->transport.hostname));
			gtk_widget_hide (label);
		}

		/* ssl */
#ifdef HAVE_NSS
		if (provider && provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL)
			gtk_widget_show (GTK_WIDGET (gui->transport.use_ssl));
		else
#endif
			gtk_widget_hide (GTK_WIDGET (gui->transport.use_ssl));

		/* auth */
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH) &&
		    !CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_AUTH))
			gtk_widget_show (GTK_WIDGET (gui->transport_needs_auth));
		else
			gtk_widget_hide (GTK_WIDGET (gui->transport_needs_auth));
	} else
		gtk_widget_hide (frame);

	frame = glade_xml_get_widget (gui->xml, "transport_auth_frame");
	if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH)) {
		gtk_widget_show (frame);

		label = glade_xml_get_widget (gui->xml, "transport_user_label");
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_USER)) {
			gtk_widget_show (GTK_WIDGET (gui->transport.username));
			gtk_widget_show (label);
		} else {
			gtk_widget_hide (GTK_WIDGET (gui->transport.username));
			gtk_widget_hide (label);
		}

		build_auth_menu (&gui->transport, provider->authtypes);
		transport_needs_auth_toggled (gui->transport_needs_auth, gui);
	} else
		gtk_widget_hide (frame);

	gtk_signal_emit_by_name (GTK_OBJECT (gui->transport.hostname), "changed");
}

static void
service_changed (GtkEntry *entry, gpointer user_data)
{
	MailAccountGuiService *service = user_data;

	gtk_widget_set_sensitive (GTK_WIDGET (service->check_supported),
				  service_complete (service));
}

static void
service_check_supported (GtkButton *button, gpointer user_data)
{
	MailAccountGuiService *gsvc = user_data;
	MailConfigService *service;
	GList *authtypes = NULL;

	service = g_new0 (MailConfigService, 1);
	save_service (gsvc, NULL, service);

	if (mail_config_check_service (service->url, CAMEL_PROVIDER_STORE, &authtypes)) {
		build_auth_menu (gsvc, authtypes);
		g_list_free (authtypes);
	}

	service_destroy (service);
}


static void
toggle_sensitivity (GtkToggleButton *toggle, GtkWidget *widget)
{
	gtk_widget_set_sensitive (widget, gtk_toggle_button_get_active (toggle));
}

static void
setup_toggle (GtkWidget *widget, const char *depname, MailAccountGui *gui)
{
	GtkToggleButton *toggle;

	if (!strcmp (depname, "UNIMPLEMENTED")) {
		gtk_widget_set_sensitive (widget, FALSE);
		return;
	}

	toggle = g_hash_table_lookup (gui->extra_config, depname);
	gtk_signal_connect (GTK_OBJECT (toggle), "toggled",
			    GTK_SIGNAL_FUNC (toggle_sensitivity),
			    widget);
	toggle_sensitivity (toggle, widget);
}

void
mail_account_gui_build_extra_conf (MailAccountGui *gui, const char *url_string)
{
	CamelURL *url;
	GtkWidget *mailcheck_frame, *main_vbox, *cur_vbox;
	CamelProviderConfEntry *entries;
	GList *children, *child;
	char *name;
	int i;

	if (url_string)
		url = camel_url_new (url_string, NULL);
	else
		url = NULL;

	main_vbox = glade_xml_get_widget (gui->xml, "extra_vbox");

	mailcheck_frame = glade_xml_get_widget (gui->xml, "extra_mailcheck_frame");

	/* Remove any additional mailcheck items. */
	children = gtk_container_children (GTK_CONTAINER (mailcheck_frame));
	if (children) {
		cur_vbox = children->data;
		g_list_free (children);
		children = gtk_container_children (GTK_CONTAINER (cur_vbox));
		for (child = children; child; child = child->next) {
			if (child != children) {
				gtk_container_remove (GTK_CONTAINER (cur_vbox),
						      child->data);
			}
		}
		g_list_free (children);
	}

	/* Remove the contents of the extra_vbox except for the
	 * mailcheck_frame.
	 */
	children = gtk_container_children (GTK_CONTAINER (main_vbox));
	for (child = children; child; child = child->next) {
		if (child != children) {
			gtk_container_remove (GTK_CONTAINER (main_vbox),
					      child->data);
		}
	}
	g_list_free (children);

	if (!gui->source.provider) {
		gtk_widget_set_sensitive (main_vbox, FALSE);
		return;
	} else
		gtk_widget_set_sensitive (main_vbox, TRUE);

	/* Set up our hash table. */
	if (gui->extra_config)
		g_hash_table_destroy (gui->extra_config);
	gui->extra_config = g_hash_table_new (g_str_hash, g_str_equal);

	entries = gui->source.provider->extra_conf;
	if (!entries)
		goto done;

	cur_vbox = main_vbox;
	for (i = 0; ; i++) {
		switch (entries[i].type) {
		case CAMEL_PROVIDER_CONF_SECTION_START:
		{
			GtkWidget *frame;

			if (entries[i].name && !strcmp (entries[i].name, "mailcheck"))
				cur_vbox = glade_xml_get_widget (gui->xml, "extra_mailcheck_vbox");
			else {
				frame = gtk_frame_new (_(entries[i].text));
				gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
				cur_vbox = gtk_vbox_new (FALSE, 4);
				gtk_container_set_border_width (GTK_CONTAINER (cur_vbox), 4);
				gtk_container_add (GTK_CONTAINER (frame), cur_vbox);
			}
			break;
		}
		case CAMEL_PROVIDER_CONF_SECTION_END:
			cur_vbox = main_vbox;
			break;

		case CAMEL_PROVIDER_CONF_CHECKBOX:
		{
			GtkWidget *checkbox;
			gboolean active;

			checkbox = gtk_check_button_new_with_label (_(entries[i].text));
			if (url)
				active = camel_url_get_param (url, entries[i].name) != NULL;
			else
				active = atoi (entries[i].value);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), active);
			gtk_box_pack_start (GTK_BOX (cur_vbox), checkbox, FALSE, FALSE, 0);
			g_hash_table_insert (gui->extra_config, entries[i].name, checkbox);
			if (entries[i].depname)
				setup_toggle (checkbox, entries[i].depname, gui);
			break;
		}

		case CAMEL_PROVIDER_CONF_ENTRY:
		{
			GtkWidget *hbox, *label, *entry;
			const char *text;

			hbox = gtk_hbox_new (FALSE, 8);
			label = gtk_label_new (_(entries[i].text));
			entry = gtk_entry_new ();
			if (url)
				text = camel_url_get_param (url, entries[i].name);
			else
				text = entries[i].value;
			if (text)
				gtk_entry_set_text (GTK_ENTRY (entry), text);

			gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
			gtk_box_pack_end (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

			gtk_box_pack_start (GTK_BOX (cur_vbox), hbox, FALSE, FALSE, 0);
			g_hash_table_insert (gui->extra_config, entries[i].name, entry);
			if (entries[i].depname) {
				setup_toggle (entry, entries[i].depname, gui);
				setup_toggle (label, entries[i].depname, gui);
			}
			break;
		}

		case CAMEL_PROVIDER_CONF_CHECKSPIN:
		{
			GtkWidget *hbox, *checkbox, *spin, *label;
			GtkObject *adj;
			char *data, *pre, *post, *p;
			double min, def, max;
			gboolean enable;

			data = _(entries[i].text);
			p = strstr (data, "%s");
			g_return_if_fail (p != NULL);

			pre = g_strndup (data, p - data);
			post = p + 2;

			data = entries[i].value;
			enable = *data++ == 'y';
			g_return_if_fail (*data == ':');
			min = strtod (++data, &data);
			g_return_if_fail (*data == ':');
			def = strtod (++data, &data);
			g_return_if_fail (*data == ':');
			max = strtod (++data, NULL);

			if (url) {
				const char *val;

				val = camel_url_get_param (url, entries[i].name);
				if (!val)
					enable = FALSE;
				else {
					enable = TRUE;
					def = atof (val);
				}
			}

			hbox = gtk_hbox_new (FALSE, 0);
			checkbox = gtk_check_button_new_with_label (pre);
			g_free (pre);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), enable);
			adj = gtk_adjustment_new (def, min, max, 1, 1, 1);
			spin = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
			label = gtk_label_new (post);

			gtk_box_pack_start (GTK_BOX (hbox), checkbox, FALSE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, FALSE, 0);
			gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 4);

			gtk_box_pack_start (GTK_BOX (cur_vbox), hbox, FALSE, FALSE, 0);
			g_hash_table_insert (gui->extra_config, entries[i].name, checkbox);
			name = g_strdup_printf ("%s_value", entries[i].name);
			g_hash_table_insert (gui->extra_config, name, spin);
			if (entries[i].depname) {
				setup_toggle (checkbox, entries[i].depname, gui);
				setup_toggle (spin, entries[i].depname, gui);
				setup_toggle (label, entries[i].depname, gui);
			}
			break;
		}

		case CAMEL_PROVIDER_CONF_END:
			goto done;
		}
	}

 done:
	gtk_widget_show_all (main_vbox);
}

static void
extract_values (MailAccountGuiService *source, GHashTable *extra_config, CamelURL *url)
{
	CamelProviderConfEntry *entries;
	GtkToggleButton *toggle;
	GtkEntry *entry;
	GtkSpinButton *spin;
	char *name;
	int i;

	if (!source->provider || !source->provider->extra_conf)
		return;
	entries = source->provider->extra_conf;

	for (i = 0; ; i++) {
		if (entries[i].depname) {
			toggle = g_hash_table_lookup (extra_config, entries[i].depname);
			if (!toggle || !gtk_toggle_button_get_active (toggle))
				continue;
		}

		switch (entries[i].type) {
		case CAMEL_PROVIDER_CONF_CHECKBOX:
			toggle = g_hash_table_lookup (extra_config, entries[i].name);
			if (gtk_toggle_button_get_active (toggle))
				camel_url_set_param (url, entries[i].name, "");
			break;

		case CAMEL_PROVIDER_CONF_ENTRY:
			entry = g_hash_table_lookup (extra_config, entries[i].name);
			camel_url_set_param (url, entries[i].name, gtk_entry_get_text (entry));
			break;

		case CAMEL_PROVIDER_CONF_CHECKSPIN:
			toggle = g_hash_table_lookup (extra_config, entries[i].name);
			if (!gtk_toggle_button_get_active (toggle))
				break;
			name = g_strdup_printf ("%s_value", entries[i].name);
			spin = g_hash_table_lookup (extra_config, name);
			g_free (name);
			name = g_strdup_printf ("%d", gtk_spin_button_get_value_as_int (spin));
			camel_url_set_param (url, entries[i].name, name);
			g_free (name);
			break;

		case CAMEL_PROVIDER_CONF_END:
			return;

		default:
			break;
		}
	}
}


extern EvolutionShellClient *global_shell_client;

static void
set_folder_picker_label (GtkButton *button, const char *name)
{
	char *string;

	string = e_utf8_to_gtk_string (GTK_WIDGET (button), name);
	gtk_label_set_text (GTK_LABEL (GTK_BIN (button)->child), string);
	g_free (string);
}

static void
folder_picker_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountGuiFolder *folder = user_data;
	const char *allowed_types[] = { "mail", NULL };
	char *physical_uri, *evolution_uri;

	physical_uri = evolution_uri = NULL;
	evolution_shell_client_user_select_folder (
		global_shell_client, _("Select Folder"), folder->uri,
		allowed_types, &evolution_uri, &physical_uri);
	if (!physical_uri || !*physical_uri) {
		g_free (physical_uri);
		g_free (evolution_uri);
		return;
	}

	g_free (folder->uri);
	folder->uri = physical_uri;
	g_free (folder->name);
	folder->name = g_strdup (g_basename (evolution_uri));
	g_free (evolution_uri);
	set_folder_picker_label (button, folder->name);
}


static gboolean
setup_service (MailAccountGuiService *gsvc, MailConfigService *service)
{
	CamelURL *url = camel_url_new (service->url, NULL);
	gboolean has_auth = FALSE;

	if (url->user && CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_USER))
		gtk_entry_set_text (gsvc->username, url->user);
	if (url->host && CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_HOST)) {
		char *hostname;

		if (url->port)
			hostname = g_strdup_printf ("%s:%d", url->host, url->port);
		else
			hostname = g_strdup (url->host);

		gtk_entry_set_text (gsvc->hostname, hostname);
		g_free (hostname);
	}
	if (url->path && CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_PATH))
		gtk_entry_set_text (gsvc->path, url->path);
	if (gsvc->provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL) {
		gboolean use_ssl = camel_url_get_param (url, "use_ssl") != NULL;
		gtk_toggle_button_set_active (gsvc->use_ssl, use_ssl);
	}
	if (url->authmech && CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_AUTH)) {
		GList *children, *item;
		CamelServiceAuthType *authtype;
		int i;

		children = gtk_container_children (GTK_CONTAINER (gtk_option_menu_get_menu (gsvc->authtype)));
		for (item = children, i = 0; item; item = item->next, i++) {
			authtype = gtk_object_get_data (item->data, "authtype");
			if (!authtype)
				continue;
			if (!strcmp (authtype->authproto, url->authmech)) {
				gtk_option_menu_set_history (gsvc->authtype, i);
				gtk_signal_emit_by_name (item->data, "activate");
				break;
			}
		}
		g_list_free (children);

		has_auth = TRUE;
	}
	
	gtk_toggle_button_set_active (gsvc->remember, service->save_passwd);

	return has_auth;
}

static gint
provider_compare (const CamelProvider *p1, const CamelProvider *p2)
{
	/* sort providers based on "location" (ie. local or remote) */
	if (p1->flags & CAMEL_PROVIDER_IS_REMOTE) {
		if (p2->flags & CAMEL_PROVIDER_IS_REMOTE)
			return 0;
		return -1;
	} else {
		if (p2->flags & CAMEL_PROVIDER_IS_REMOTE)
			return 1;
		return 0;
	}
}

MailAccountGui *
mail_account_gui_new (MailConfigAccount *account)
{
	MailAccountGui *gui;

	gui = g_new0 (MailAccountGui, 1);
	gui->account = account;
	gui->xml = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", NULL);

	/* Management */
	gui->account_name = GTK_ENTRY (glade_xml_get_widget (gui->xml, "management_name"));
	gui->default_account = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "management_default"));
	if (account->name)
		e_utf8_gtk_entry_set_text (gui->account_name, account->name);
	gtk_toggle_button_set_active (gui->default_account, account->default_account);

	/* Identity */
	gui->full_name = GTK_ENTRY (glade_xml_get_widget (gui->xml, "identity_full_name"));
	gui->email_address = GTK_ENTRY (glade_xml_get_widget (gui->xml, "identity_address"));
	gui->organization = GTK_ENTRY (glade_xml_get_widget (gui->xml, "identity_organization"));
	gui->signature = GNOME_FILE_ENTRY (glade_xml_get_widget (gui->xml, "identity_signature"));
	gnome_file_entry_set_default_path (gui->signature, g_get_home_dir ());
	if (account->id) {
		if (account->id->name)
			e_utf8_gtk_entry_set_text (gui->full_name, account->id->name);
		if (account->id->address)
			gtk_entry_set_text (gui->email_address, account->id->address);
		if (account->id->organization)
			e_utf8_gtk_entry_set_text (gui->organization, account->id->organization);
		if (account->id->signature) {
			gnome_file_entry_set_default_path (gui->signature, account->id->signature);
			gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (gui->signature)),
					    account->id->signature);
		}
	}

	/* Source */
	gui->source.type = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "source_type_omenu"));
	gui->source.hostname = GTK_ENTRY (glade_xml_get_widget (gui->xml, "source_host"));
	gtk_signal_connect (GTK_OBJECT (gui->source.hostname), "changed",
			    GTK_SIGNAL_FUNC (service_changed), &gui->source);
	gui->source.username = GTK_ENTRY (glade_xml_get_widget (gui->xml, "source_user"));
	gtk_signal_connect (GTK_OBJECT (gui->source.username), "changed",
			    GTK_SIGNAL_FUNC (service_changed), &gui->source);
	gui->source.path = GTK_ENTRY (glade_xml_get_widget (gui->xml, "source_path"));
	gtk_signal_connect (GTK_OBJECT (gui->source.path), "changed",
			    GTK_SIGNAL_FUNC (service_changed), &gui->source);
	gui->source.use_ssl = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "source_use_ssl"));
	gui->source.authtype = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "source_auth_omenu"));
	gui->source.remember = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "source_remember_password"));
	gui->source.check_supported = GTK_BUTTON (glade_xml_get_widget (gui->xml, "source_check_supported"));
	gtk_signal_connect (GTK_OBJECT (gui->source.check_supported), "clicked",
			    GTK_SIGNAL_FUNC (service_check_supported), &gui->source);
	gui->source_auto_check = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "extra_auto_check"));
	gui->source_auto_check_min = GTK_SPIN_BUTTON (glade_xml_get_widget (gui->xml, "extra_auto_check_min"));

	/* Transport */
	gui->transport.type = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "transport_type_omenu"));
	gui->transport.hostname = GTK_ENTRY (glade_xml_get_widget (gui->xml, "transport_host"));
	gtk_signal_connect (GTK_OBJECT (gui->transport.hostname), "changed",
			    GTK_SIGNAL_FUNC (service_changed), &gui->source);
	gui->transport.username = GTK_ENTRY (glade_xml_get_widget (gui->xml, "transport_user"));
	gtk_signal_connect (GTK_OBJECT (gui->transport.username), "changed",
			    GTK_SIGNAL_FUNC (service_changed), &gui->source);
	gui->transport.use_ssl = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "transport_use_ssl"));
	gui->transport_needs_auth = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "transport_needs_auth"));
	gtk_signal_connect (GTK_OBJECT (gui->transport_needs_auth), "toggled", transport_needs_auth_toggled, gui);
	gui->transport.authtype = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "transport_auth_omenu"));
	gui->transport.remember = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "transport_remember_password"));
	gui->transport.check_supported = GTK_BUTTON (glade_xml_get_widget (gui->xml, "transport_check_supported"));
	gtk_signal_connect (GTK_OBJECT (gui->transport.check_supported), "clicked",
			    GTK_SIGNAL_FUNC (service_check_supported), &gui->transport);

	/* Drafts folder */
	gui->drafts_folder_button = GTK_BUTTON (glade_xml_get_widget (gui->xml, "drafts_button"));
	gtk_signal_connect (GTK_OBJECT (gui->drafts_folder_button), "clicked",
			    GTK_SIGNAL_FUNC (folder_picker_clicked), &gui->drafts_folder);
	if (account->drafts_folder_uri) {
		gui->drafts_folder.uri = g_strdup (account->drafts_folder_uri);
		gui->drafts_folder.name = g_strdup (account->drafts_folder_name);
	} else {
		gui->drafts_folder.uri = g_strdup (default_drafts_folder_uri);
		gui->drafts_folder.name = g_strdup (strrchr (default_drafts_folder_uri, '/') + 1);
	}
	set_folder_picker_label (gui->drafts_folder_button, gui->drafts_folder.name);
	
	/* Sent folder */
	gui->sent_folder_button = GTK_BUTTON (glade_xml_get_widget (gui->xml, "sent_button"));
	gtk_signal_connect (GTK_OBJECT (gui->sent_folder_button), "clicked",
			    GTK_SIGNAL_FUNC (folder_picker_clicked), &gui->sent_folder);
	if (account->sent_folder_uri) {
		gui->sent_folder.uri = g_strdup (account->sent_folder_uri);
		gui->sent_folder.name = g_strdup (account->sent_folder_name);
	} else {
		gui->sent_folder.uri = g_strdup (default_sent_folder_uri);
		gui->sent_folder.name = g_strdup (strrchr (default_sent_folder_uri, '/') + 1);
	}
	set_folder_picker_label (gui->sent_folder_button, gui->sent_folder.name);

	return gui;
}

void
mail_account_gui_setup (MailAccountGui *gui, GtkWidget *top)
{
	GtkWidget *stores, *transports, *item;
	GtkWidget *fstore = NULL, *ftransport = NULL;
	int si = 0, hstore = 0, ti = 0, htransport = 0;
	int max_width = 0;
	char *max_authname = NULL;
	char *source_proto, *transport_proto;
	GList *providers, *l;

	if (gui->account->source && gui->account->source->url) {
		source_proto = gui->account->source->url;
		source_proto = g_strndup (source_proto, strcspn (source_proto, ":"));
	} else
		source_proto = NULL;
	if (gui->account->transport && gui->account->transport->url) {
		transport_proto = gui->account->transport->url;
		transport_proto = g_strndup (transport_proto, strcspn (transport_proto, ":"));
	} else
		transport_proto = NULL;

	/* Construct source/transport option menus */
	stores = gtk_menu_new ();
	transports = gtk_menu_new ();
	providers = camel_session_list_providers (session, TRUE);

	/* sort the providers, remote first */
	providers = g_list_sort (providers, (GCompareFunc) provider_compare);

	for (l = providers; l; l = l->next) {
		CamelProvider *provider = l->data;

		if (strcmp (provider->domain, "mail"))
			continue;

		item = NULL;
		if (provider->object_types[CAMEL_PROVIDER_STORE] && provider->flags & CAMEL_PROVIDER_IS_SOURCE) {
			item = gtk_menu_item_new_with_label (_(provider->name));
			gtk_object_set_data (GTK_OBJECT (item), "provider", provider);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (source_type_changed),
					    gui);

			gtk_menu_append (GTK_MENU (stores), item);

			gtk_widget_show (item);

			if (!fstore) {
				fstore = item;
				hstore = si;
			}

			if (source_proto && !g_strcasecmp (provider->protocol, source_proto)) {
				fstore = item;
				hstore = si;
			}

			si++;
		}

		if (provider->object_types[CAMEL_PROVIDER_TRANSPORT]) {
			item = gtk_menu_item_new_with_label (_(provider->name));
			gtk_object_set_data (GTK_OBJECT (item), "provider", provider);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (transport_type_changed),
					    gui);

			gtk_menu_append (GTK_MENU (transports), item);

			gtk_widget_show (item);

			if (!ftransport) {
				ftransport = item;
				htransport = ti;
			}

			if (transport_proto && !g_strcasecmp (provider->protocol, transport_proto)) {
				ftransport = item;
				htransport = ti;
			}

			ti++;
		}

		if (item && provider->authtypes) {
			GdkFont *font = GTK_WIDGET (item)->style->font;
			CamelServiceAuthType *at;
			int width;
			GList *a;

			for (a = provider->authtypes; a; a = a->next) {
				at = a->data;

				width = gdk_string_width (font, _(at->name));
				if (width > max_width) {
					max_authname = _(at->name);
					max_width = width;
				}
			}
		}
	}
	g_list_free (providers);

	/* add a "None" option to the stores menu */
	item = gtk_menu_item_new_with_label (_("None"));
	gtk_object_set_data (GTK_OBJECT (item), "provider", NULL);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (source_type_changed),
			    gui);

	gtk_menu_append (GTK_MENU (stores), item);

	gtk_widget_show (item);

	if (!fstore || !source_proto) {
		fstore = item;
		hstore = si;
	}

	/* set the menus on the optionmenus */
	gtk_option_menu_remove_menu (gui->source.type);
	gtk_option_menu_set_menu (gui->source.type, stores);
	gtk_option_menu_set_history (gui->source.type, hstore);

	gtk_option_menu_remove_menu (gui->transport.type);
	gtk_option_menu_set_menu (gui->transport.type, transports);
	gtk_option_menu_set_history (gui->transport.type, htransport);

	/* Force the authmenus to the width of the widest element */
	if (max_authname) {
		GtkWidget *menu;
		GtkRequisition size_req;

		menu = gtk_menu_new ();
		item = gtk_menu_item_new_with_label (max_authname);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show_all (menu);
		gtk_option_menu_set_menu (gui->source.authtype, menu);
		gtk_widget_show (GTK_WIDGET (gui->source.authtype));
		gtk_widget_size_request (GTK_WIDGET (gui->source.authtype),
					 &size_req);

		gtk_widget_set_usize (GTK_WIDGET (gui->source.authtype),
				      size_req.width, -1);
		gtk_widget_set_usize (GTK_WIDGET (gui->transport.authtype),
				      size_req.width, -1);
	}

	gtk_widget_show_all (top);

	/* Force some other elements to keep their current sizes even if
	 * widgets are hidden
	 */
	item = glade_xml_get_widget (gui->xml, "source_frame");
	gtk_widget_set_usize (item, -1, item->allocation.height);
	item = glade_xml_get_widget (gui->xml, "source_auth_frame");
	gtk_widget_set_usize (item, -1, item->allocation.height);
	item = glade_xml_get_widget (gui->xml, "source_vbox");
	gtk_widget_set_usize (item, -1, item->allocation.height);
	item = glade_xml_get_widget (gui->xml, "transport_frame");
	gtk_widget_set_usize (item, -1, item->allocation.height);
	item = glade_xml_get_widget (gui->xml, "transport_auth_frame");
	gtk_widget_set_usize (item, -1, item->allocation.height);
	item = glade_xml_get_widget (gui->xml, "transport_vbox");
	gtk_widget_set_usize (item, -1, item->allocation.height);

	if (fstore)
		gtk_signal_emit_by_name (GTK_OBJECT (fstore), "activate", gui);

	if (ftransport)
		gtk_signal_emit_by_name (GTK_OBJECT (ftransport), "activate", gui);

	if (source_proto) {
		setup_service (&gui->source, gui->account->source);
		g_free (source_proto);
		if (gui->account->source->auto_check) {
			gtk_toggle_button_set_active (gui->source_auto_check, TRUE);
			gtk_spin_button_set_value (gui->source_auto_check_min,
						   gui->account->source->auto_check_time);
		}
	}
	if (transport_proto) {
		if (setup_service (&gui->transport, gui->account->transport))
			gtk_toggle_button_set_active (gui->transport_needs_auth, TRUE);
		g_free (transport_proto);
	}
}

static void
save_service (MailAccountGuiService *gsvc, GHashTable *extra_config,
	      MailConfigService *service)
{
	CamelURL *url;
	char *str;

	if (!gsvc->provider) {
		g_free (service->url);
		service->url = NULL;
		return;
	}

	url = g_new0 (CamelURL, 1);
	url->protocol = g_strdup (gsvc->provider->protocol);

	if (CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_USER)) {
		str = gtk_entry_get_text (gsvc->username);
		if (str && *str)
			url->user = g_strdup (str);
	}

	if (CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_AUTH) &&
	    GTK_WIDGET_IS_SENSITIVE (gsvc->authtype)) {
		CamelServiceAuthType *authtype;

		authtype = gtk_object_get_data (GTK_OBJECT (gsvc->authitem), "authtype");
		if (authtype->authproto && *authtype->authproto)
			url->authmech = g_strdup (authtype->authproto);

		service->save_passwd = gtk_toggle_button_get_active (gsvc->remember);
	}

	if (CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_HOST)) {
		char *pport;

		str = gtk_entry_get_text (gsvc->hostname);
		if (str && *str) {
			pport = strchr (str, ':');
			if (pport) {
				url->host = g_strndup (str, pport - str);
				url->port = atoi (pport + 1);
			} else
				url->host = g_strdup (str);
		}
	}

	if (CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_PATH)) {
		str = gtk_entry_get_text (gsvc->path);
		if (str && *str)
			url->path = g_strdup (str);
	}

	if (gsvc->provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL) {
		if (gtk_toggle_button_get_active (gsvc->use_ssl))
			camel_url_set_param (url, "use_ssl", "");
	}

	if (extra_config)
		extract_values (gsvc, extra_config, url);

	g_free (service->url);
	service->url = camel_url_to_string (url, 0);

	/* Temporary until keep_on_server moves into the POP provider */
	if (camel_url_get_param (url, "keep_on_server"))
		service->keep_on_server = TRUE;

	camel_url_free (url);
}

gboolean
mail_account_gui_save (MailAccountGui *gui)
{
	MailConfigAccount *account = gui->account;
	gboolean old_enabled;

	if (!mail_account_gui_identity_complete (gui) ||
	    !mail_account_gui_source_complete (gui) ||
	    !mail_account_gui_transport_complete (gui) ||
	    !mail_account_gui_management_complete (gui))
		return FALSE;

	g_free (account->name);
	account->name = e_utf8_gtk_entry_get_text (gui->account_name);
	account->default_account = gtk_toggle_button_get_active (gui->default_account);

	/* construct the identity */
	identity_destroy (account->id);
	account->id = g_new0 (MailConfigIdentity, 1);
	account->id->name = e_utf8_gtk_entry_get_text (gui->full_name);
	account->id->address = e_utf8_gtk_entry_get_text (gui->email_address);
	account->id->organization = e_utf8_gtk_entry_get_text (gui->organization);
	account->id->signature = gnome_file_entry_get_full_path (gui->signature, TRUE);

	old_enabled = account->source && account->source->enabled;
	service_destroy (account->source);
	account->source = g_new0 (MailConfigService, 1);
	save_service (&gui->source, gui->extra_config, account->source);
	if (account->source && account->source->url && old_enabled)
		account->source->enabled = TRUE;
	account->source->auto_check = gtk_toggle_button_get_active (gui->source_auto_check);
	if (account->source->auto_check)
		account->source->auto_check_time = gtk_spin_button_get_value_as_int (gui->source_auto_check_min);

	service_destroy (account->transport);
	account->transport = g_new0 (MailConfigService, 1);
	save_service (&gui->transport, NULL, account->transport);

	g_free (account->drafts_folder_name);
	account->drafts_folder_name = g_strdup (gui->drafts_folder.name);
	g_free (account->drafts_folder_uri);
	account->drafts_folder_uri = g_strdup (gui->drafts_folder.uri);
	g_free (account->sent_folder_name);
	account->sent_folder_name = g_strdup (gui->sent_folder.name);
	g_free (account->sent_folder_uri);
	account->sent_folder_uri = g_strdup (gui->sent_folder.uri);

	return TRUE;
}

void
mail_account_gui_destroy (MailAccountGui *gui)
{
	gtk_object_unref (GTK_OBJECT (gui->xml));
	if (gui->extra_config)
		g_hash_table_destroy (gui->extra_config);
}

