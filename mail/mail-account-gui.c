/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Dan Winship <danw@ximian.com>
 *    Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <string.h>
#include <stdarg.h>

#include <gconf/gconf-client.h>

#include <gtk/gtkentry.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkdialog.h>

#include <e-util/e-account-list.h>
#include <e-util/e-signature-list.h>

#include <widgets/misc/e-error.h>

#include "em-account-prefs.h"
#include "em-folder-selection-button.h"
#include "mail-account-gui.h"
#include "mail-session.h"
#include "mail-send-recv.h"
#include "mail-signature-editor.h"
#include "mail-component.h"
#include "em-utils.h"
#include "em-composer-prefs.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-mt.h"

#if defined (HAVE_NSS)
#include "smime/gui/e-cert-selector.h"
#endif

#define d(x)

static void save_service (MailAccountGuiService *gsvc, GHashTable *extra_conf, EAccountService *service);
static void service_changed (GtkEntry *entry, gpointer user_data);

struct {
	char *label;
	char *value;
} ssl_options[] = {
	{ N_("Always"), "always" },
	{ N_("Whenever Possible"), "when-possible" },
	{ N_("Never"), "never" }
};

static int num_ssl_options = sizeof (ssl_options) / sizeof (ssl_options[0]);

static gboolean
is_email (const char *address)
{
	/* This is supposed to check if the address's domain could be
           an FQDN but alas, it's not worth the pain and suffering. */
	const char *at;
	
	at = strchr (address, '@');
	/* make sure we have an '@' and that it's not the first or last char */
	if (!at || at == address || *(at + 1) == '\0')
		return FALSE;
	
	return TRUE;
}

static GtkWidget *
get_focused_widget (GtkWidget *def, ...)
{
	GtkWidget *widget, *ret = NULL;
	va_list args;
	
	va_start (args, def);
	widget = va_arg (args, GtkWidget *);
	while (widget) {
		if (GTK_WIDGET_HAS_FOCUS (widget)) {
			ret = widget;
			break;
		}
		
		widget = va_arg (args, GtkWidget *);
	}
	va_end (args);
	
	if (ret)
		return ret;
	else
		return def;
}

gboolean
mail_account_gui_identity_complete (MailAccountGui *gui, GtkWidget **incomplete)
{
	const char *text;
	
	text = gtk_entry_get_text (gui->full_name);
	if (!text || !*text) {
		if (incomplete)
			*incomplete = get_focused_widget (GTK_WIDGET (gui->full_name),
							  GTK_WIDGET (gui->email_address),
							  GTK_WIDGET (gui->reply_to),
							  NULL);
		return FALSE;
	}
	
	text = gtk_entry_get_text (gui->email_address);
	if (!text || !is_email (text)) {
		if (incomplete)
			*incomplete = get_focused_widget (GTK_WIDGET (gui->email_address),
							  GTK_WIDGET (gui->full_name),
							  GTK_WIDGET (gui->reply_to),
							  NULL);
		return FALSE;
	}
	
	/* make sure that if the reply-to field is filled in, that it is valid */
	text = gtk_entry_get_text (gui->reply_to);
	if (text && *text && !is_email (text)) {
		if (incomplete)
			*incomplete = get_focused_widget (GTK_WIDGET (gui->reply_to),
							  GTK_WIDGET (gui->email_address),
							  GTK_WIDGET (gui->full_name),
							  NULL);
		return FALSE;
	}
	
	return TRUE;
}

static void
auto_detected_foreach (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

static void
check_button_state (GtkToggleButton *button, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (button)))
		gtk_widget_set_sensitive (GTK_WIDGET (data), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (data), FALSE);
}


static gboolean
populate_text_entry (GtkTextView *view, const char *filename)
{
	FILE *fd;
	char *filebuf;
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	int count;

	g_return_val_if_fail (filename != NULL , FALSE);

	fd = fopen (filename, "r");
	
	if (!fd) {
		/* FIXME: Should never come here */
		return FALSE;
	}
	
	filebuf = g_malloc (1024);

	buffer =  gtk_text_buffer_new (NULL);
	gtk_text_buffer_get_start_iter (buffer, &iter);

	while (!feof (fd) && !ferror (fd)) {
		count = fread (filebuf, 1, sizeof (filebuf), fd);
		gtk_text_buffer_insert (buffer, &iter, filebuf, count);
	}
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (view), 
					GTK_TEXT_BUFFER (buffer));
	fclose (fd);
	g_free (filebuf);
	return TRUE;
}

static gboolean
display_license (CamelProvider *prov)
{
	GladeXML *xml;
	GtkWidget *top_widget;
	GtkTextView *text_entry;
	GtkButton *button_yes, *button_no;
	GtkCheckButton *check_button;
	GtkResponseType response = GTK_RESPONSE_NONE;
	GtkLabel *top_label;
	char *label_text, *dialog_title;
	gboolean status;
	
	xml = glade_xml_new (EVOLUTION_GLADEDIR "/mail-license.glade", "lic_dialog", NULL);
	
	top_widget = glade_xml_get_widget (xml, "lic_dialog");
	text_entry = GTK_TEXT_VIEW (glade_xml_get_widget (xml, "textview1"));
	if (!(status = populate_text_entry (GTK_TEXT_VIEW (text_entry), prov->license_file)))
		goto failed;
	
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text_entry), FALSE);
	
	button_yes = GTK_BUTTON (glade_xml_get_widget (xml, "lic_yes_button"));
	gtk_widget_set_sensitive (GTK_WIDGET (button_yes), FALSE);
	
	button_no = GTK_BUTTON (glade_xml_get_widget (xml, "lic_no_button"));
	
	check_button = GTK_CHECK_BUTTON (glade_xml_get_widget (xml, "lic_checkbutton"));
	
	top_label = GTK_LABEL (glade_xml_get_widget (xml, "lic_top_label"));
	
	label_text = g_strdup_printf (_("\nPlease read carefully the license agreement\n" 
					"for %s displayed below\n" 
					"and tick the check box for accepting it\n"), prov->license);
	
	gtk_label_set_label (top_label, label_text);
	
	dialog_title = g_strdup_printf (_("%s License Agreement"), prov->license);
	
	gtk_window_set_title (GTK_WINDOW (top_widget), dialog_title);
	
	g_signal_connect (check_button, "toggled", G_CALLBACK (check_button_state), button_yes);
	
	response = gtk_dialog_run (GTK_DIALOG (top_widget));
	
	g_free (label_text);
	g_free (dialog_title);
	
 failed:
	gtk_widget_destroy (top_widget);
	g_object_unref (xml);
	
	return (response == GTK_RESPONSE_ACCEPT);
}

static gboolean
service_complete (MailAccountGuiService *service, GHashTable *extra_config, GtkWidget **incomplete)
{
	const CamelProvider *prov = service->provider;
	GtkWidget *path;
	const char *text;
	
	if (!prov)
		return TRUE;
	
	/* transports don't have a path */
	if (service->path)
		path = GTK_WIDGET (service->path);
	else
		path = NULL;
	
	if (CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_HOST)) {
		text = gtk_entry_get_text (service->hostname);
		if (!text || !*text) {
			if (incomplete)
				*incomplete = get_focused_widget (GTK_WIDGET (service->hostname),
								  GTK_WIDGET (service->username),
								  path,
								  NULL);
			return FALSE;
		}
	}
	
	if (CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_USER)) {
		text = gtk_entry_get_text (service->username);
		if (!text || !*text) {
			if (incomplete)
				*incomplete = get_focused_widget (GTK_WIDGET (service->username),
								  GTK_WIDGET (service->hostname),
								  path,
								  NULL);
			return FALSE;
		}
	}
	
	if (CAMEL_PROVIDER_NEEDS (prov, CAMEL_URL_PART_PATH)) {
		if (!path) {
			d(printf ("aagh, transports aren't supposed to have paths.\n"));
			return TRUE;
		}
		
		text = gtk_entry_get_text (service->path);
		if (!text || !*text) {
			if (incomplete)
				*incomplete = get_focused_widget (GTK_WIDGET (service->path),
								  GTK_WIDGET (service->hostname),
								  GTK_WIDGET (service->username),
								  NULL);
			return FALSE;
		}
	}
	
	return TRUE;
}

static gboolean
mail_account_gui_check_for_license (CamelProvider *prov)
{
	GConfClient *gconf;
	gboolean accepted = TRUE, status;
	GSList * providers_list, *l, *n;
	char *provider;

	if (prov->flags & CAMEL_PROVIDER_HAS_LICENSE) {
		gconf = mail_config_get_gconf_client ();
		
		providers_list = gconf_client_get_list (gconf, "/apps/evolution/mail/licenses", GCONF_VALUE_STRING, NULL);
		
		for (l = providers_list, accepted = FALSE; l && !accepted; l = g_slist_next (l))
			accepted = (strcmp ((char *)l->data, prov->protocol) == 0);
		if (!accepted) {
			/* Since the license is not yet accepted, pop-up a 
			 * dialog to display the license agreement and check 
			 * if the user accepts it
			 */

			if ((accepted = display_license (prov)) == TRUE) {
				provider = g_strdup (prov->protocol);
				providers_list = g_slist_append (providers_list,
						 provider);
				status = gconf_client_set_list (gconf, 
						"/apps/evolution/mail/licenses",
						GCONF_VALUE_STRING,
						 providers_list, NULL);
			}
		}
		l = providers_list;
		while (l) {
			n = g_slist_next (l);
			g_free (l->data);
			g_slist_free_1 (l);
			l = n;
		}
	}
	return accepted;
}

gboolean
mail_account_gui_source_complete (MailAccountGui *gui, GtkWidget **incomplete)
{
	return service_complete (&gui->source, gui->extra_config, incomplete);
}

void
mail_account_gui_auto_detect_extra_conf (MailAccountGui *gui)
{
	MailAccountGuiService *service = &gui->source;
	CamelProvider *prov = service->provider;
	GHashTable *auto_detected;
	GtkWidget *path;
	CamelURL *url;
	char *text;
	const char *tmp;

	if (!prov)
		return;
	
	/* transports don't have a path */
	if (service->path)
		path = GTK_WIDGET (service->path);
	else
		path = NULL;
	
	url = g_new0 (CamelURL, 1);
	camel_url_set_protocol (url, prov->protocol);
	
	if (CAMEL_PROVIDER_ALLOWS (prov, CAMEL_URL_PART_HOST)) {
		text = g_strdup (gtk_entry_get_text (service->hostname));
		if (*text) {
			char *port;
			
			port = strchr (text, ':');
			if (port) {
				*port++ = '\0';
				camel_url_set_port (url, atoi (port));
			}
			
			camel_url_set_host (url, text);
		}
		g_free (text);
	}
	
	if (CAMEL_PROVIDER_ALLOWS (prov, CAMEL_URL_PART_USER)) {
		text = g_strdup (gtk_entry_get_text (service->username));
		g_strstrip (text);
		camel_url_set_user (url, text);
		g_free (text);
	}
	
	if (path && CAMEL_PROVIDER_ALLOWS (prov, CAMEL_URL_PART_PATH)) {
		tmp = gtk_entry_get_text (service->path);
		if (tmp && *tmp)
			camel_url_set_path (url, tmp);
	}
	
	camel_provider_auto_detect (prov, url, &auto_detected, NULL);
	camel_url_free (url);
	
	if (auto_detected) {
		CamelProviderConfEntry *entries;
		GtkToggleButton *toggle;
		GtkSpinButton *spin;
		GtkEntry *entry;
		char *value;
		int i;
		
		entries = service->provider->extra_conf;
		
		for (i = 0; entries[i].type != CAMEL_PROVIDER_CONF_END; i++) {
			GtkWidget *enable_widget = NULL;

			if (!entries[i].name)
				continue;
			
			value = g_hash_table_lookup (auto_detected, entries[i].name);
			if (!value)
				continue;
			
			switch (entries[i].type) {
			case CAMEL_PROVIDER_CONF_CHECKBOX:
				toggle = g_hash_table_lookup (gui->extra_config, entries[i].name);
				gtk_toggle_button_set_active (toggle, atoi (value));
				enable_widget = (GtkWidget *)toggle;
				break;
				
			case CAMEL_PROVIDER_CONF_ENTRY:
				entry = g_hash_table_lookup (gui->extra_config, entries[i].name);
				if (value)
					gtk_entry_set_text (entry, value);
				enable_widget = (GtkWidget *)entry;
				break;
				
			case CAMEL_PROVIDER_CONF_CHECKSPIN:
			{
				gboolean enable;
				double val;
				char *name;
				
				toggle = g_hash_table_lookup (gui->extra_config, entries[i].name);
				name = g_strdup_printf ("%s_value", entries[i].name);
				spin = g_hash_table_lookup (gui->extra_config, name);
				g_free (name);
				
				enable = *value++ == 'y';
				gtk_toggle_button_set_active (toggle, enable);
				g_assert (*value == ':');
				val = strtod (++value, NULL);
				gtk_spin_button_set_value (spin, val);
				enable_widget = (GtkWidget *)spin;
			}
			break;
			default:
				break;
			}

			if (enable_widget)
				gtk_widget_set_sensitive(enable_widget, e_account_writable_option(gui->account, prov->protocol, entries[i].name));

		}
		
		g_hash_table_foreach (auto_detected, auto_detected_foreach, NULL);
		g_hash_table_destroy (auto_detected);
	}
}

gboolean
mail_account_gui_transport_complete (MailAccountGui *gui, GtkWidget **incomplete)
{
	if (!gui->transport.provider) {
		if (incomplete)
			*incomplete = GTK_WIDGET (gui->transport.type);
		return FALSE;
	}

	/* If it's both source and transport, there's nothing extra to
	 * configure on the transport page.
	 */
	if (CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (gui->transport.provider)) {
		if (gui->transport.provider == gui->source.provider)
			return TRUE;
		if (incomplete)
			*incomplete = GTK_WIDGET (gui->transport.type);
		return FALSE;
	}
	
	if (!service_complete (&gui->transport, NULL, incomplete))
		return FALSE;
	
	/* FIXME? */
	if (gtk_toggle_button_get_active (gui->transport_needs_auth) &&
	    CAMEL_PROVIDER_ALLOWS (gui->transport.provider, CAMEL_URL_PART_USER)) {
		const char *text = gtk_entry_get_text (gui->transport.username);
		
		if (!text || !*text) {
			if (incomplete)
				*incomplete = get_focused_widget (GTK_WIDGET (gui->transport.username),
								  GTK_WIDGET (gui->transport.hostname),
								  NULL);
			return FALSE;
		}
	}
	
	return TRUE;
}

gboolean
mail_account_gui_management_complete (MailAccountGui *gui, GtkWidget **incomplete)
{
	const char *text;
	
	text = gtk_entry_get_text (gui->account_name);
	if (text && *text)
		return TRUE;
	
	if (incomplete)
		*incomplete = GTK_WIDGET (gui->account_name);
	
	return FALSE;
}


static void
service_authtype_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountGuiService *service = user_data;
	CamelServiceAuthType *authtype;
	
	service->authitem = widget;
	authtype = g_object_get_data ((GObject *) widget, "authtype");
	
	gtk_widget_set_sensitive (GTK_WIDGET (service->remember), authtype->need_password);
}

static void
build_auth_menu (MailAccountGuiService *service, GList *all_authtypes,
		 GList *supported_authtypes, gboolean check_supported)
{
	GtkWidget *menu, *item, *first = NULL;
	CamelServiceAuthType *current, *authtype, *sauthtype;
	int history = 0, i;
	GList *l, *s;
	
	if (service->authitem)
		current = g_object_get_data ((GObject *) service->authitem, "authtype");
	else
		current = NULL;
	
	service->authitem = NULL;
	
	menu = gtk_menu_new ();
	
	for (l = all_authtypes, i = 0; l; l = l->next, i++) {
		authtype = l->data;
		
		item = gtk_menu_item_new_with_label (authtype->name);
		for (s = supported_authtypes; s; s = s->next) {
			sauthtype = s->data;
			if (!strcmp (authtype->name, sauthtype->name))
				break;
		}
		
		if (check_supported && !s) {
			gtk_widget_set_sensitive (item, FALSE);
		} else if (current && !strcmp (authtype->name, current->name)) {
			first = item;
			history = i;
		} else if (!first) {
			first = item;
			history = i;
		}
		
		g_object_set_data ((GObject *) item, "authtype", authtype);
		g_signal_connect (item, "activate", G_CALLBACK (service_authtype_changed), service);
		
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		
		gtk_widget_show (item);
	}
	
	gtk_option_menu_remove_menu (service->authtype);
	gtk_option_menu_set_menu (service->authtype, menu);
	
	if (first) {
		gtk_option_menu_set_history (service->authtype, history);
		g_signal_emit_by_name (first, "activate");
	}
}

static void
transport_provider_set_available (MailAccountGui *gui, CamelProvider *provider,
				  gboolean available)
{
	GtkWidget *menuitem;
	
	menuitem = g_object_get_data ((GObject *) gui->transport.type, provider->protocol);
	g_return_if_fail (menuitem != NULL);
	
	gtk_widget_set_sensitive (menuitem, available);
	
	if (available) {
		gpointer number = g_object_get_data ((GObject *) menuitem, "number");
		
		g_signal_emit_by_name (menuitem, "activate");
		gtk_option_menu_set_history (gui->transport.type, GPOINTER_TO_UINT (number));
	}
}

static void
source_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountGui *gui = user_data;
	GtkWidget *file_entry, *label, *frame, *dwidget = NULL;
	CamelProvider *provider;
	gboolean writeable;
	gboolean license_accepted = TRUE;
	
	provider = g_object_get_data ((GObject *) widget, "provider");
	
	/* If the previously-selected provider has a linked transport,
	 * disable it.
	 */
	if (gui->source.provider &&
	    CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (gui->source.provider))
		transport_provider_set_available (gui, gui->source.provider, FALSE);
	
	gui->source.provider = provider;
	
	if (gui->source.provider) {
		writeable = e_account_writable_option (gui->account, gui->source.provider->protocol, "auth");
		gtk_widget_set_sensitive ((GtkWidget *) gui->source.authtype, writeable);
		gtk_widget_set_sensitive ((GtkWidget *) gui->source.check_supported, writeable);
		
		writeable = e_account_writable_option (gui->account, gui->source.provider->protocol, "use_ssl");
		gtk_widget_set_sensitive ((GtkWidget *) gui->source.use_ssl, writeable);
		
		writeable = e_account_writable (gui->account, E_ACCOUNT_SOURCE_SAVE_PASSWD);
		gtk_widget_set_sensitive ((GtkWidget *) gui->source.remember, writeable);
	}
	
	if (provider)
		gtk_label_set_text (gui->source.description, provider->description);
	else
		gtk_label_set_text (gui->source.description, "");
	
	if (gui->source.provider)	
		license_accepted = mail_account_gui_check_for_license (gui->source.provider);

	frame = glade_xml_get_widget (gui->xml, "source_frame");
	if (provider && license_accepted) {
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
		file_entry = glade_xml_get_widget (gui->xml, "source_path_entry");
		
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_PATH)) {
			if (!dwidget)
				dwidget = GTK_WIDGET (gui->source.path);
			
			gtk_widget_show (GTK_WIDGET (file_entry));
			gtk_widget_show (label);
		} else {
			gtk_entry_set_text (gui->source.path, "");
			gtk_widget_hide (GTK_WIDGET (file_entry));
			gtk_widget_hide (label);
		}
		
		/* ssl */
#ifdef HAVE_SSL
		gtk_widget_hide (gui->source.no_ssl);
		if (provider && provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL) {
			gtk_widget_show (gui->source.ssl_frame);
			gtk_widget_show (gui->source.ssl_hbox);
		} else {
			gtk_widget_hide (gui->source.ssl_frame);
			gtk_widget_hide (gui->source.ssl_hbox);
		}
#else
		gtk_widget_hide (gui->source.ssl_hbox);
		gtk_widget_show (gui->source.no_ssl);
#endif
		
		/* auth */
		frame = glade_xml_get_widget (gui->xml, "source_auth_frame");
		if (provider && CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH)) {
			build_auth_menu (&gui->source, provider->authtypes, NULL, FALSE);
			gtk_widget_show (frame);
		} else
			gtk_widget_hide (frame);
	} else {
		gtk_widget_hide (frame);
		frame = glade_xml_get_widget (gui->xml, "source_auth_frame");
		gtk_widget_hide (frame);
	}
	
	g_signal_emit_by_name (gui->source.username, "changed");
	
	if (dwidget)
		gtk_widget_grab_focus (dwidget);
	
	mail_account_gui_build_extra_conf (gui, gui && gui->account && gui->account->source ? gui->account->source->url : NULL);
	
	if (provider && CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider))
		transport_provider_set_available (gui, provider, TRUE);
}


static void
transport_needs_auth_toggled (GtkToggleButton *toggle, gpointer data)
{
	MailAccountGui *gui = data;
	gboolean need = gtk_toggle_button_get_active (toggle);
	GtkWidget *widget;
	
	widget = glade_xml_get_widget (gui->xml, "transport_auth_frame");
	gtk_widget_set_sensitive (widget, need);
	if (need)
		service_changed (NULL, &gui->transport);
}

static void
transport_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountGui *gui = user_data;
	CamelProvider *provider;
	GtkWidget *label, *frame;
	gboolean writeable;
	
	provider = g_object_get_data ((GObject *) widget, "provider");
	gui->transport.provider = provider;
	
	if (gui->transport.provider) {
		writeable = e_account_writable_option (gui->account, gui->transport.provider->protocol, "auth");
		gtk_widget_set_sensitive ((GtkWidget *) gui->transport.authtype, writeable);
		gtk_widget_set_sensitive ((GtkWidget *) gui->transport.check_supported, writeable);
		
		writeable = e_account_writable_option (gui->account, gui->transport.provider->protocol, "use_ssl");
		gtk_widget_set_sensitive ((GtkWidget *) gui->transport.use_ssl, writeable);
		
		writeable = e_account_writable (gui->account, E_ACCOUNT_TRANSPORT_SAVE_PASSWD);
		gtk_widget_set_sensitive ((GtkWidget *) gui->transport.remember, writeable);
	}
	
	/* description */
	gtk_label_set_text (gui->transport.description, provider->description);
	
	frame = glade_xml_get_widget (gui->xml, "transport_frame");
	if (!CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider) &&
	    (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_HOST) ||
	     (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH) &&
	      !CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_AUTH)))) {
		gtk_widget_show (frame);
		
		label = glade_xml_get_widget (gui->xml, "transport_host_label");
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_HOST)) {
			gtk_widget_show (GTK_WIDGET (gui->transport.hostname));
			gtk_widget_show (label);
		} else {
			gtk_widget_hide (GTK_WIDGET (gui->transport.hostname));
			gtk_widget_hide (label);
		}
		
		/* ssl */
#ifdef HAVE_SSL
		gtk_widget_hide (gui->transport.no_ssl);
		if (provider && provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL) {
			gtk_widget_show (gui->transport.ssl_frame);
			gtk_widget_show (gui->transport.ssl_hbox);
		} else {
			gtk_widget_hide (gui->transport.ssl_frame);
			gtk_widget_hide (gui->transport.ssl_hbox);
		}
#else
		gtk_widget_hide (gui->transport.ssl_hbox);
		gtk_widget_show (gui->transport.no_ssl);
#endif
		
		/* auth */
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH) &&
		    !CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_AUTH))
			gtk_widget_show (GTK_WIDGET (gui->transport_needs_auth));
		else
			gtk_widget_hide (GTK_WIDGET (gui->transport_needs_auth));
	} else
		gtk_widget_hide (frame);
	
	frame = glade_xml_get_widget (gui->xml, "transport_auth_frame");
	if (!CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider) &&
	    CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH)) {
		gtk_widget_show (frame);
		
		label = glade_xml_get_widget (gui->xml, "transport_user_label");
		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_USER)) {
			gtk_widget_show (GTK_WIDGET (gui->transport.username));
			gtk_widget_show (label);
		} else {
			gtk_widget_hide (GTK_WIDGET (gui->transport.username));
			gtk_widget_hide (label);
		}
		
		build_auth_menu (&gui->transport, provider->authtypes, NULL, FALSE);
		transport_needs_auth_toggled (gui->transport_needs_auth, gui);
	} else
		gtk_widget_hide (frame);
	
	g_signal_emit_by_name (gui->transport.hostname, "changed");
}

static void
service_changed (GtkEntry *entry, gpointer user_data)
{
	MailAccountGuiService *service = user_data;
	
	gtk_widget_set_sensitive (GTK_WIDGET (service->check_supported),
				  service_complete (service, NULL, NULL));
}

static void
service_check_supported (GtkButton *button, gpointer user_data)
{
	MailAccountGuiService *gsvc = user_data;
	EAccountService *service;
	GList *authtypes = NULL;
	GtkWidget *authitem;
	GtkWidget *window;
	
	service = g_new0 (EAccountService, 1);
	
	/* This is sort of a hack, when checking for supported AUTH
           types we don't want to use whatever authtype is selected
           because it may not be available. */
	authitem = gsvc->authitem;
	gsvc->authitem = NULL;
	
	save_service (gsvc, NULL, service);
	
	gsvc->authitem = authitem;
	
	window = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW);
	
	if (mail_config_check_service (service->url, gsvc->provider_type, &authtypes, GTK_WINDOW (window))) {
		build_auth_menu (gsvc, gsvc->provider->authtypes, authtypes, TRUE);
		if (!authtypes) {
			/* provider doesn't support any authtypes */
			gtk_widget_set_sensitive (GTK_WIDGET (gsvc->check_supported), FALSE);
		}
		g_list_free (authtypes);
	}
	
	g_free (service->url);
	g_free (service);
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
	g_signal_connect (toggle, "toggled", G_CALLBACK (toggle_sensitivity), widget);
	toggle_sensitivity (toggle, widget);
}

void
mail_account_gui_build_extra_conf (MailAccountGui *gui, const char *url_string)
{
	CamelURL *url;
	GtkWidget *mailcheck_frame, *mailcheck_hbox;
	GtkWidget *hostname_label, *username_label, *path_label;
	GtkWidget *hostname, *username, *path;
	GtkTable *main_table, *cur_table;
	CamelProviderConfEntry *entries;
	GList *child, *next;
	char *name;
	int i, rows;
	
	if (url_string)
		url = camel_url_new (url_string, NULL);
	else
		url = NULL;
	
	hostname_label = glade_xml_get_widget (gui->xml, "source_host_label");
	gtk_label_set_text_with_mnemonic (GTK_LABEL (hostname_label), _("_Host:"));
	hostname = glade_xml_get_widget (gui->xml, "source_host");
	
	username_label = glade_xml_get_widget (gui->xml, "source_user_label");
	gtk_label_set_text_with_mnemonic (GTK_LABEL (username_label), _("User_name:"));
	username = glade_xml_get_widget (gui->xml, "source_user");
	
	path_label = glade_xml_get_widget (gui->xml, "source_path_label");
	gtk_label_set_text_with_mnemonic (GTK_LABEL (path_label), _("_Path:"));
	path = glade_xml_get_widget (gui->xml, "source_path");
	
	/* Remove the contents of the extra_table except for the
	 * mailcheck_frame.
	 */
	main_table = (GtkTable *) glade_xml_get_widget (gui->xml, "extra_table");
	gtk_container_set_border_width ((GtkContainer *) main_table, 12);
	gtk_table_set_row_spacings (main_table, 6);
	gtk_table_set_col_spacings (main_table, 8);
	mailcheck_frame = glade_xml_get_widget (gui->xml, "extra_mailcheck_frame");
	child = gtk_container_get_children (GTK_CONTAINER (main_table));
	while (child != NULL) {
		next = child->next;
		if (child->data != (gpointer) mailcheck_frame)
			gtk_container_remove (GTK_CONTAINER (main_table), child->data);
		g_list_free_1 (child);
		child = next;
	}
	
	gtk_table_resize (main_table, 1, 2);
	
	/* Remove any additional mailcheck items. */
	cur_table = (GtkTable *) glade_xml_get_widget (gui->xml, "extra_mailcheck_table");
	gtk_container_set_border_width ((GtkContainer *) cur_table, 12);
	gtk_table_set_row_spacings (cur_table, 6);
	gtk_table_set_col_spacings (cur_table, 8);
	mailcheck_hbox = glade_xml_get_widget (gui->xml, "extra_mailcheck_hbox");
	child = gtk_container_get_children (GTK_CONTAINER (cur_table));
	while (child != NULL) {
		next = child->next;
		if (child->data != (gpointer) mailcheck_hbox)
			gtk_container_remove (GTK_CONTAINER (cur_table), child->data);
		g_list_free_1 (child);
		child = next;
	}
	
	gtk_table_resize (cur_table, 1, 2);

	if (!gui->source.provider) {
		gtk_widget_set_sensitive (GTK_WIDGET (main_table), FALSE);
		if (url)
			camel_url_free (url);
		return;
	} else
		gtk_widget_set_sensitive(GTK_WIDGET(main_table), e_account_writable(gui->account, E_ACCOUNT_SOURCE_URL));
	
	/* Set up our hash table. */
	if (gui->extra_config)
		g_hash_table_destroy (gui->extra_config);
	gui->extra_config = g_hash_table_new (g_str_hash, g_str_equal);
	
	entries = gui->source.provider->extra_conf;
	if (!entries)
		goto done;
	
	cur_table = main_table;
	rows = main_table->nrows;
	for (i = 0; ; i++) {
		GtkWidget *enable_widget = NULL;
		
		switch (entries[i].type) {
		case CAMEL_PROVIDER_CONF_SECTION_START:
		{
			GtkWidget *frame, *label;
			char *markup;
			
			if (entries[i].name && !strcmp (entries[i].name, "mailcheck")) {
				cur_table = (GtkTable *) glade_xml_get_widget (gui->xml, "extra_mailcheck_table");
				rows = cur_table->nrows;
				break;
			}
			
			markup = g_strdup_printf ("<span weight=\"bold\">%s</span>", entries[i].text);
			label = gtk_label_new (NULL);
			gtk_label_set_markup ((GtkLabel *) label, markup);
			gtk_label_set_justify ((GtkLabel *) label, GTK_JUSTIFY_LEFT);
			gtk_label_set_use_markup ((GtkLabel *) label, TRUE);
			gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
			gtk_widget_show (label);
			g_free (markup);
			
			cur_table = (GtkTable *) gtk_table_new (0, 2, FALSE);
			gtk_container_set_border_width ((GtkContainer *) cur_table, 12);
			gtk_table_set_row_spacings (cur_table, 6);
			gtk_table_set_col_spacings (cur_table, 8);
			gtk_widget_show ((GtkWidget *) cur_table);
			
			frame = gtk_vbox_new (FALSE, 0);
			gtk_box_pack_start ((GtkBox *) frame, label, FALSE, FALSE, 0);
			gtk_box_pack_start ((GtkBox *) frame, (GtkWidget *) cur_table, FALSE, FALSE, 0);
			gtk_widget_show (frame);
			
			gtk_table_attach (main_table, frame, 0, 2,
					  rows, rows + 1,
					  GTK_EXPAND | GTK_FILL, 0, 0, 0);
			
			rows = 0;
			
			break;
		}
		case CAMEL_PROVIDER_CONF_SECTION_END:
			cur_table = main_table;
			rows = main_table->nrows;
			break;
			
		case CAMEL_PROVIDER_CONF_LABEL:
			if (entries[i].name && entries[i].text) {
				GtkWidget *label;
				
				if (!strcmp (entries[i].name, "username")) {
					gtk_label_set_text_with_mnemonic (GTK_LABEL (username_label), entries[i].text);
					enable_widget = username_label;
				} else if (!strcmp (entries[i].name, "hostname")) {
					gtk_label_set_text_with_mnemonic (GTK_LABEL (hostname_label), entries[i].text);
					enable_widget = hostname_label;
				} else if (!strcmp (entries[i].name, "path")) {
					gtk_label_set_text_with_mnemonic (GTK_LABEL (path_label), entries[i].text);
					enable_widget = path_label;
				} else {
					/* make a new label */
					label = gtk_label_new (entries[i].text);
					gtk_table_resize (cur_table, cur_table->nrows + 1, 2);
					gtk_table_attach (cur_table, label, 0, 2, rows, rows + 1,
							  GTK_EXPAND | GTK_FILL, 0, 0, 0);
					rows++;
					enable_widget = label;
				}
			}
			break;
			
		case CAMEL_PROVIDER_CONF_CHECKBOX:
		{
			GtkWidget *checkbox;
			gboolean active;
			
			checkbox = gtk_check_button_new_with_label (entries[i].text);
			if (url)
				active = camel_url_get_param (url, entries[i].name) != NULL;
			else
				active = atoi (entries[i].value);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), active);
			
			gtk_table_attach (cur_table, checkbox, 0, 2, rows, rows + 1,
					  GTK_EXPAND | GTK_FILL, 0, 0, 0);
			rows++;
			g_hash_table_insert (gui->extra_config, entries[i].name, checkbox);
			if (entries[i].depname)
				setup_toggle (checkbox, entries[i].depname, gui);
			
			enable_widget = checkbox;
			break;
		}
		
		case CAMEL_PROVIDER_CONF_ENTRY:
		{
			GtkWidget *label, *entry;
			const char *text;
			
			if (!strcmp (entries[i].name, "username")) {
				gtk_label_set_text_with_mnemonic (GTK_LABEL (username_label), entries[i].text);
				label = username_label;
				entry = username;
			} else if (!strcmp (entries[i].name, "hostname")) {
				gtk_label_set_text_with_mnemonic (GTK_LABEL (hostname_label), entries[i].text);
				label = hostname_label;
				entry = hostname;
			} else if (!strcmp (entries[i].name, "path")) {
				gtk_label_set_text_with_mnemonic (GTK_LABEL (path_label), entries[i].text);
				label = path_label;
				entry = path;
			} else {
				/* make a new text entry with label */
				label = gtk_label_new (entries[i].text);
				gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
				entry = gtk_entry_new ();
				
				gtk_table_attach (cur_table, label, 0, 1, rows, rows + 1,
						  GTK_FILL, 0, 0, 0);
				gtk_table_attach (cur_table, entry, 1, 2, rows, rows + 1,
						  GTK_EXPAND | GTK_FILL, 0, 0, 0);
				rows++;
			}
			
			if (url)
				text = camel_url_get_param (url, entries[i].name);
			else
				text = entries[i].value;
			
			if (text)
				gtk_entry_set_text (GTK_ENTRY (entry), text);
			
			if (entries[i].depname) {
				setup_toggle (entry, entries[i].depname, gui);
				setup_toggle (label, entries[i].depname, gui);
			}
			
			g_hash_table_insert (gui->extra_config, entries[i].name, entry);
			
			enable_widget = entry;
			break;
		}
		
		case CAMEL_PROVIDER_CONF_CHECKSPIN:
		{
			GtkWidget *hbox, *checkbox, *spin, *label;
			GtkObject *adj;
			char *data, *pre, *post, *p;
			double min, def, max;
			gboolean enable;
			
			/* FIXME: this is pretty fucked... */
			data = entries[i].text;
			p = strstr (data, "%s");
			g_return_if_fail (p != NULL);
			
			pre = g_strndup (data, p - data);
			post = p + 2;
			
			data = entries[i].value;
			enable = *data++ == 'y';
			g_return_if_fail (*data == ':');
			min = strtod (data + 1, &data);
			g_return_if_fail (*data == ':');
			def = strtod (data + 1, &data);
			g_return_if_fail (*data == ':');
			max = strtod (data + 1, NULL);
			
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
			
			gtk_table_attach (cur_table, hbox, 0, 2, rows, rows + 1,
					  GTK_EXPAND | GTK_FILL, 0, 0, 0);
			rows++;
			g_hash_table_insert (gui->extra_config, entries[i].name, checkbox);
			name = g_strdup_printf ("%s_value", entries[i].name);
			g_hash_table_insert (gui->extra_config, name, spin);
			if (entries[i].depname) {
				setup_toggle (checkbox, entries[i].depname, gui);
				setup_toggle (spin, entries[i].depname, gui);
				setup_toggle (label, entries[i].depname, gui);
			}
			
			enable_widget = hbox;
			break;
		}
		
		case CAMEL_PROVIDER_CONF_END:
			goto done;
		}
		
		if (enable_widget)
			gtk_widget_set_sensitive(enable_widget, e_account_writable_option(gui->account, gui->source.provider->protocol, entries[i].name));
	}
	
 done:
	gtk_widget_show_all (GTK_WIDGET (main_table));
	if (url)
		camel_url_free (url);
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
			if (strcmp (entries[i].name, "username") == 0
			    || strcmp (entries[i].name, "hostname") == 0
			    || strcmp (entries[i].name, "path") == 0) {
				break;
			}
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

static void
folder_selected (EMFolderSelectionButton *button, gpointer user_data)
{
	char **folder_name = user_data;
	
	g_free (*folder_name);
	*folder_name = g_strdup(em_folder_selection_button_get_selection(button));
}

static void
default_folders_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountGui *gui = user_data;
	
	/* Drafts folder */
	g_free (gui->drafts_folder_uri);
	gui->drafts_folder_uri = g_strdup(mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS));
	em_folder_selection_button_set_selection((EMFolderSelectionButton *)gui->drafts_folder_button, gui->drafts_folder_uri);
	
	/* Sent folder */
	g_free (gui->sent_folder_uri);
	gui->sent_folder_uri = g_strdup(mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT));
	em_folder_selection_button_set_selection((EMFolderSelectionButton *)gui->sent_folder_button, gui->sent_folder_uri);
}

GtkWidget *mail_account_gui_folder_selector_button_new (char *widget_name, char *string1, char *string2, int int1, int int2);

GtkWidget *
mail_account_gui_folder_selector_button_new (char *widget_name,
					     char *string1, char *string2,
					     int int1, int int2)
{
	return (GtkWidget *)em_folder_selection_button_new(_("Select Folder"), NULL);
}

static gboolean
setup_service (MailAccountGui *gui, MailAccountGuiService *gsvc, EAccountService *service)
{
	CamelURL *url = camel_url_new (service->url, NULL);
	gboolean has_auth = FALSE;
	
	if (url == NULL || gsvc->provider == NULL)
		return FALSE;
	
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
		GList *children, *item;
		const char *use_ssl;
		int i;
		
		use_ssl = camel_url_get_param (url, "use_ssl");
		if (!use_ssl)
			use_ssl = "never";
		else if (!*use_ssl)  /* old config code just used an empty string as the value */
			use_ssl = "always";
		
		children = gtk_container_get_children(GTK_CONTAINER (gtk_option_menu_get_menu (gsvc->use_ssl)));
		for (item = children, i = 0; item; item = item->next, i++) {
			if (!strcmp (use_ssl, ssl_options[i].value)) {
				gtk_option_menu_set_history (gsvc->use_ssl, i);
				g_signal_emit_by_name (item->data, "activate", gsvc);
				break;
			}
		}
	}
	
	if (url->authmech && CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_AUTH)) {
		GList *children, *item;
		CamelServiceAuthType *authtype;
		int i;
		
		children = gtk_container_get_children(GTK_CONTAINER (gtk_option_menu_get_menu (gsvc->authtype)));
		for (item = children, i = 0; item; item = item->next, i++) {
			authtype = g_object_get_data ((GObject *) item->data, "authtype");
			if (!authtype)
				continue;
			if (!strcmp (authtype->authproto, url->authmech)) {
				gtk_option_menu_set_history (gsvc->authtype, i);
				g_signal_emit_by_name (item->data, "activate");
				break;
			}
		}
		g_list_free (children);
		
		has_auth = TRUE;
	}
	camel_url_free (url);
	
	gtk_toggle_button_set_active (gsvc->remember, service->save_passwd);

	gtk_widget_set_sensitive((GtkWidget *)gsvc->authtype, e_account_writable_option(gui->account, gsvc->provider->protocol, "auth"));
	gtk_widget_set_sensitive((GtkWidget *)gsvc->use_ssl, e_account_writable_option(gui->account, gsvc->provider->protocol, "use_ssl"));
	
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

static void
ssl_option_activate (GtkWidget *widget, gpointer user_data)
{
	MailAccountGuiService *service = user_data;
	
	service->ssl_selected = widget;
}

static void
construct_ssl_menu (MailAccountGuiService *service)
{
	GtkWidget *menu, *item = NULL;
	int i;
	
	menu = gtk_menu_new ();
	
	for (i = 0; i < num_ssl_options; i++) {
		item = gtk_menu_item_new_with_label (_(ssl_options[i].label));
		g_object_set_data ((GObject *) item, "use_ssl", ssl_options[i].value);
		g_signal_connect (item, "activate", G_CALLBACK (ssl_option_activate), service);
		gtk_widget_show (item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
	
	gtk_option_menu_remove_menu (service->use_ssl);
	gtk_option_menu_set_menu (service->use_ssl, menu);
	
	gtk_option_menu_set_history (service->use_ssl, i - 1);
	g_signal_emit_by_name (item, "activate", service);
}

static void
sig_activate (GtkWidget *item, MailAccountGui *gui)
{
	ESignature *sig;
	
	sig = g_object_get_data ((GObject *) item, "sig");
	
	gui->sig_uid = sig ? sig->uid : NULL;
}

static void
signature_added (ESignatureList *signatures, ESignature *sig, MailAccountGui *gui)
{
	GtkWidget *menu, *item;
	
	menu = gtk_option_menu_get_menu (gui->sig_menu);
	if (sig->autogen)
		item = gtk_menu_item_new_with_label (_("Autogenerated"));
	else
		item = gtk_menu_item_new_with_label (sig->name);
	g_object_set_data ((GObject *) item, "sig", sig);
	g_signal_connect (item, "activate", G_CALLBACK (sig_activate), gui);
	gtk_widget_show (item);
	
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	
	gtk_option_menu_set_history (gui->sig_menu, g_list_length (GTK_MENU_SHELL (menu)->children));
}

static void
signature_removed (ESignatureList *signatures, ESignature *sig, MailAccountGui *gui)
{
	GtkWidget *menu;
	ESignature *cur;
	GList *items;
	
	if (gui->sig_uid == sig->uid)
		gui->sig_uid = NULL;
	
	menu = gtk_option_menu_get_menu (gui->sig_menu);
	items = GTK_MENU_SHELL (menu)->children;
	while (items != NULL) {
		cur = g_object_get_data (items->data, "sig");
		if (cur == sig) {
			gtk_widget_destroy (items->data);
			break;
		}
		items = items->next;
	}
}

static void
menu_item_set_label (GtkMenuItem *item, const char *label)
{
	GtkWidget *widget;
	
	widget = gtk_bin_get_child ((GtkBin *) item);
	if (GTK_IS_LABEL (widget))
		gtk_label_set_text ((GtkLabel *) widget, label);
}

static void
signature_changed (ESignatureList *signatures, ESignature *sig, MailAccountGui *gui)
{
	GtkWidget *menu;
	ESignature *cur;
	GList *items;
	
	menu = gtk_option_menu_get_menu (gui->sig_menu);
	items = GTK_MENU_SHELL (menu)->children;
	while (items != NULL) {
		cur = g_object_get_data (items->data, "sig");
		if (cur == sig) {
			menu_item_set_label (items->data, sig->name);
			break;
		}
		items = items->next;
	}
}

static void
clear_menu (GtkWidget *menu)
{
	while (GTK_MENU_SHELL (menu)->children)
		gtk_container_remove (GTK_CONTAINER (menu), GTK_MENU_SHELL (menu)->children->data);
}

static void
sig_fill_menu (MailAccountGui *gui)
{
	ESignatureList *signatures;
	GtkWidget *menu, *item;
	EIterator *it;
	
	menu = gtk_option_menu_get_menu (gui->sig_menu);
	clear_menu (menu);
	
	item = gtk_menu_item_new_with_label (_("None"));
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	
	signatures = mail_config_get_signatures ();
	it = e_list_get_iterator ((EList *) signatures);
	
	while (e_iterator_is_valid (it)) {
		ESignature *sig;
		
		sig = (ESignature *) e_iterator_get (it);
		signature_added (signatures, sig, gui);
		e_iterator_next (it);
	}
	
	g_object_unref (it);
	
	gui->sig_added_id = g_signal_connect (signatures, "signature-added", G_CALLBACK (signature_added), gui);
	gui->sig_removed_id = g_signal_connect (signatures, "signature-removed", G_CALLBACK (signature_removed), gui);
	gui->sig_changed_id = g_signal_connect (signatures, "signature-changed", G_CALLBACK (signature_changed), gui);
	
	gtk_option_menu_set_history (gui->sig_menu, 0);
}

static void
sig_switch_to_list (GtkWidget *w, MailAccountGui *gui)
{
	gtk_window_set_transient_for (GTK_WINDOW (gtk_widget_get_toplevel (w)), NULL);
	gdk_window_raise (GTK_WIDGET (gui->dialog)->window);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (glade_xml_get_widget (gui->dialog->gui, "notebook")), 3);
}

static void
sig_add_new_signature (GtkWidget *w, MailAccountGui *gui)
{
	GConfClient *gconf;
	gboolean send_html;
	GtkWidget *parent;
	
	if (!gui->dialog)
		return;
	
	sig_switch_to_list (w, gui);
	
	gconf = mail_config_get_gconf_client ();
	send_html = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/send_html", NULL);
	
	parent = gtk_widget_get_toplevel (w);
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;
	
	em_composer_prefs_new_signature ((GtkWindow *) parent, send_html, NULL);
}

static void
select_account_signature (MailAccountGui *gui)
{
	ESignature *sig, *cur;
	GtkWidget *menu;
	GList *items;
	int i = 0;
	
	if (!gui->account->id->sig_uid || !(sig = mail_config_get_signature_by_uid (gui->account->id->sig_uid)))
		return;
	
	menu = gtk_option_menu_get_menu (gui->sig_menu);
	items = GTK_MENU_SHELL (menu)->children;
	while (items != NULL) {
		cur = g_object_get_data (items->data, "sig");
		if (cur == sig) {
			gtk_option_menu_set_history (gui->sig_menu, i);
			gtk_menu_item_activate (items->data);
			break;
		}
		items = items->next;
		i++;
	}
}

static void
prepare_signatures (MailAccountGui *gui)
{
	GtkWidget *button;
	
	gui->sig_menu = (GtkOptionMenu *) glade_xml_get_widget (gui->xml, "sigOption");
	sig_fill_menu (gui);
	
	button = glade_xml_get_widget (gui->xml, "sigAddNew");
	g_signal_connect (button, "clicked", G_CALLBACK (sig_add_new_signature), gui);
	
	if (!gui->dialog) {
		gtk_widget_hide (glade_xml_get_widget (gui->xml, "sigLabel"));
		gtk_widget_hide (glade_xml_get_widget (gui->xml, "sigOption"));
		gtk_widget_hide (glade_xml_get_widget (gui->xml, "sigAddNew"));
	}
	
	select_account_signature (gui);
}

#if defined (HAVE_NSS)
static void
smime_changed(MailAccountGui *gui)
{
	int act;
	const char *tmp;

	tmp = gtk_entry_get_text(gui->smime_sign_key);
	act = tmp && tmp[0];
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_sign_key_clear, act);
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_sign_default, act);
	if (!act)
		gtk_toggle_button_set_active(gui->smime_sign_default, FALSE);

	tmp = gtk_entry_get_text(gui->smime_encrypt_key);
	act = tmp && tmp[0];
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_encrypt_key_clear, act);
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_encrypt_default, act);
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_encrypt_to_self, act);
	if (!act) {
		gtk_toggle_button_set_active(gui->smime_encrypt_default, FALSE);
		gtk_toggle_button_set_active(gui->smime_encrypt_to_self, FALSE);
	}
}

static void
smime_sign_key_selected(GtkWidget *dialog, const char *key, MailAccountGui *gui)
{
	if (key != NULL) {
		gtk_entry_set_text(gui->smime_sign_key, key);
		smime_changed(gui);
	}

	gtk_widget_destroy(dialog);
}

static void
smime_sign_key_select(GtkWidget *button, MailAccountGui *gui)
{
	GtkWidget *w;

	w = e_cert_selector_new(E_CERT_SELECTOR_SIGNER, gtk_entry_get_text(gui->smime_sign_key));
	gtk_window_set_modal((GtkWindow *)w, TRUE);
	gtk_window_set_transient_for((GtkWindow *)w, (GtkWindow *)gui->dialog);
	g_signal_connect(w, "selected", G_CALLBACK(smime_sign_key_selected), gui);
	gtk_widget_show(w);
}

static void
smime_sign_key_clear(GtkWidget *w, MailAccountGui *gui)
{
	gtk_entry_set_text(gui->smime_sign_key, "");
	smime_changed(gui);
}

static void
smime_encrypt_key_selected(GtkWidget *dialog, const char *key, MailAccountGui *gui)
{
	if (key != NULL) {
		gtk_entry_set_text(gui->smime_encrypt_key, key);
		smime_changed(gui);
	}

	gtk_widget_destroy(dialog);
}

static void
smime_encrypt_key_select(GtkWidget *button, MailAccountGui *gui)
{
	GtkWidget *w;

	w = e_cert_selector_new(E_CERT_SELECTOR_SIGNER, gtk_entry_get_text(gui->smime_encrypt_key));
	gtk_window_set_modal((GtkWindow *)w, TRUE);
	gtk_window_set_transient_for((GtkWindow *)w, (GtkWindow *)gui->dialog);
	g_signal_connect(w, "selected", G_CALLBACK(smime_encrypt_key_selected), gui);
	gtk_widget_show(w);
}

static void
smime_encrypt_key_clear(GtkWidget *w, MailAccountGui *gui)
{
	gtk_entry_set_text(gui->smime_encrypt_key, "");
	smime_changed(gui);
}
#endif

MailAccountGui *
mail_account_gui_new (EAccount *account, EMAccountPrefs *dialog)
{
	MailAccountGui *gui;
	
	g_object_ref (account);
	
	gui = g_new0 (MailAccountGui, 1);
	gui->account = account;
	gui->dialog = dialog;
	gui->xml = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", NULL, NULL);
	
	/* Management */
	gui->account_name = GTK_ENTRY (glade_xml_get_widget (gui->xml, "management_name"));
	gui->default_account = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "management_default"));
	if (account->name)
		gtk_entry_set_text (gui->account_name, account->name);
	if (!mail_config_get_default_account ()
	    || (account == mail_config_get_default_account ()))
		gtk_toggle_button_set_active (gui->default_account, TRUE);
	
	/* Identity */
	gui->full_name = GTK_ENTRY (glade_xml_get_widget (gui->xml, "identity_full_name"));
	gui->email_address = GTK_ENTRY (glade_xml_get_widget (gui->xml, "identity_address"));
	gui->reply_to = GTK_ENTRY (glade_xml_get_widget (gui->xml, "identity_reply_to"));
	gui->organization = GTK_ENTRY (glade_xml_get_widget (gui->xml, "identity_organization"));
	
	if (account->id->name)
		gtk_entry_set_text (gui->full_name, account->id->name);
	if (account->id->address)
		gtk_entry_set_text (gui->email_address, account->id->address);
	if (account->id->reply_to)
		gtk_entry_set_text (gui->reply_to, account->id->reply_to);
	if (account->id->organization)
		gtk_entry_set_text (gui->organization, account->id->organization);
	
	prepare_signatures (gui);
	
	/* Source */
	gui->source.provider_type = CAMEL_PROVIDER_STORE;
	gui->source.container = glade_xml_get_widget(gui->xml, "source_vbox");
	gui->source.type = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "source_type_omenu"));
	gui->source.description = GTK_LABEL (glade_xml_get_widget (gui->xml, "source_description"));
	gui->source.hostname = GTK_ENTRY (glade_xml_get_widget (gui->xml, "source_host"));
	g_signal_connect (gui->source.hostname, "changed",
			  G_CALLBACK (service_changed), &gui->source);
	gui->source.username = GTK_ENTRY (glade_xml_get_widget (gui->xml, "source_user"));
	g_signal_connect (gui->source.username, "changed",
			  G_CALLBACK (service_changed), &gui->source);
	gui->source.path = GTK_ENTRY (glade_xml_get_widget (gui->xml, "source_path"));
	g_signal_connect (gui->source.path, "changed",
			  G_CALLBACK (service_changed), &gui->source);
	gui->source.ssl_frame = glade_xml_get_widget (gui->xml, "source_security_frame");
	gtk_widget_hide (gui->source.ssl_frame);
	gui->source.ssl_hbox = glade_xml_get_widget (gui->xml, "source_ssl_hbox");
	gui->source.use_ssl = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "source_use_ssl"));
	construct_ssl_menu (&gui->source);
	gui->source.no_ssl = glade_xml_get_widget (gui->xml, "source_ssl_disabled");
	gui->source.authtype = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "source_auth_omenu"));
	gui->source.remember = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "source_remember_password"));
	gui->source.check_supported = GTK_BUTTON (glade_xml_get_widget (gui->xml, "source_check_supported"));
	g_signal_connect (gui->source.check_supported, "clicked",
			  G_CALLBACK (service_check_supported), &gui->source);
	gui->source_auto_check = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "extra_auto_check"));
	gui->source_auto_check_min = GTK_SPIN_BUTTON (glade_xml_get_widget (gui->xml, "extra_auto_check_min"));
	
	/* Transport */
	gui->transport.provider_type = CAMEL_PROVIDER_TRANSPORT;
	gui->transport.container = glade_xml_get_widget(gui->xml, "transport_vbox");
	gui->transport.type = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "transport_type_omenu"));
	gui->transport.description = GTK_LABEL (glade_xml_get_widget (gui->xml, "transport_description"));
	gui->transport.hostname = GTK_ENTRY (glade_xml_get_widget (gui->xml, "transport_host"));
	g_signal_connect (gui->transport.hostname, "changed",
			  G_CALLBACK (service_changed), &gui->transport);
	gui->transport.username = GTK_ENTRY (glade_xml_get_widget (gui->xml, "transport_user"));
	g_signal_connect (gui->transport.username, "changed",
			  G_CALLBACK (service_changed), &gui->transport);
	gui->transport.ssl_frame = glade_xml_get_widget (gui->xml, "transport_security_frame");
	gtk_widget_hide (gui->transport.ssl_frame);
	gui->transport.ssl_hbox = glade_xml_get_widget (gui->xml, "transport_ssl_hbox");
	gui->transport.use_ssl = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "transport_use_ssl"));
	construct_ssl_menu (&gui->transport);
	gui->transport.no_ssl = glade_xml_get_widget (gui->xml, "transport_ssl_disabled");
	gui->transport_needs_auth = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "transport_needs_auth"));
	g_signal_connect (gui->transport_needs_auth, "toggled",
			  G_CALLBACK (transport_needs_auth_toggled), gui);
	gui->transport.authtype = GTK_OPTION_MENU (glade_xml_get_widget (gui->xml, "transport_auth_omenu"));
	gui->transport.remember = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "transport_remember_password"));
	gui->transport.check_supported = GTK_BUTTON (glade_xml_get_widget (gui->xml, "transport_check_supported"));
	g_signal_connect (gui->transport.check_supported, "clicked",
			  G_CALLBACK (service_check_supported), &gui->transport);
	
	/* Drafts folder */
	gui->drafts_folder_button = GTK_BUTTON (glade_xml_get_widget (gui->xml, "drafts_button"));
	g_signal_connect (gui->drafts_folder_button, "selected", G_CALLBACK (folder_selected), &gui->drafts_folder_uri);
	if (account->drafts_folder_uri)
		gui->drafts_folder_uri = em_uri_to_camel (account->drafts_folder_uri);
	else
		gui->drafts_folder_uri = g_strdup(mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS));
	em_folder_selection_button_set_selection((EMFolderSelectionButton *)gui->drafts_folder_button, gui->drafts_folder_uri);
	gtk_widget_show ((GtkWidget *) gui->drafts_folder_button);
	
	/* Sent folder */
	gui->sent_folder_button = GTK_BUTTON (glade_xml_get_widget (gui->xml, "sent_button"));
	g_signal_connect (gui->sent_folder_button, "selected", G_CALLBACK (folder_selected), &gui->sent_folder_uri);
	if (account->sent_folder_uri)
		gui->sent_folder_uri = em_uri_to_camel (account->sent_folder_uri);
	else
		gui->sent_folder_uri = g_strdup(mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT));
	em_folder_selection_button_set_selection((EMFolderSelectionButton *)gui->sent_folder_button, gui->sent_folder_uri);
	gtk_widget_show ((GtkWidget *) gui->sent_folder_button);
	
	/* Special Folders "Reset Defaults" button */
	gui->restore_folders_button = (GtkButton *)glade_xml_get_widget (gui->xml, "default_folders_button");
	g_signal_connect (gui->restore_folders_button, "clicked", G_CALLBACK (default_folders_clicked), gui);
	
	/* Always Cc */
	gui->always_cc = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "always_cc"));
	gtk_toggle_button_set_active (gui->always_cc, account->always_cc);
	gui->cc_addrs = GTK_ENTRY (glade_xml_get_widget (gui->xml, "cc_addrs"));
	if (account->cc_addrs)
		gtk_entry_set_text (gui->cc_addrs, account->cc_addrs);
	
	/* Always Bcc */
	gui->always_bcc = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "always_bcc"));
	gtk_toggle_button_set_active (gui->always_bcc, account->always_bcc);
	gui->bcc_addrs = GTK_ENTRY (glade_xml_get_widget (gui->xml, "bcc_addrs"));
	if (account->bcc_addrs)
		gtk_entry_set_text (gui->bcc_addrs, account->bcc_addrs);
	
	/* Security */
	gui->pgp_key = GTK_ENTRY (glade_xml_get_widget (gui->xml, "pgp_key"));
	if (account->pgp_key)
		gtk_entry_set_text (gui->pgp_key, account->pgp_key);
	gui->pgp_encrypt_to_self = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "pgp_encrypt_to_self"));
	gtk_toggle_button_set_active (gui->pgp_encrypt_to_self, account->pgp_encrypt_to_self);
	gui->pgp_always_sign = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "pgp_always_sign"));
	gtk_toggle_button_set_active (gui->pgp_always_sign, account->pgp_always_sign);
	gui->pgp_no_imip_sign = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "pgp_no_imip_sign"));
	gtk_toggle_button_set_active (gui->pgp_no_imip_sign, account->pgp_no_imip_sign);
	gui->pgp_always_trust = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "pgp_always_trust"));
	gtk_toggle_button_set_active (gui->pgp_always_trust, account->pgp_always_trust);
	
#if defined (HAVE_NSS)
	gui->smime_sign_key = (GtkEntry *)glade_xml_get_widget (gui->xml, "smime_sign_key");
	if (account->smime_sign_key)
		gtk_entry_set_text(gui->smime_sign_key, account->smime_sign_key);
	gui->smime_sign_key_select = (GtkButton *)glade_xml_get_widget (gui->xml, "smime_sign_key_select");
	gui->smime_sign_key_clear = (GtkButton *)glade_xml_get_widget (gui->xml, "smime_sign_key_clear");
	g_signal_connect(gui->smime_sign_key_select, "clicked", G_CALLBACK(smime_sign_key_select), gui);
	g_signal_connect(gui->smime_sign_key_clear, "clicked", G_CALLBACK(smime_sign_key_clear), gui);

	gui->smime_sign_default = (GtkToggleButton *)glade_xml_get_widget (gui->xml, "smime_sign_default");
	gtk_toggle_button_set_active(gui->smime_sign_default, account->smime_sign_default);

	gui->smime_encrypt_key = (GtkEntry *)glade_xml_get_widget (gui->xml, "smime_encrypt_key");
	if (account->smime_encrypt_key)
		gtk_entry_set_text(gui->smime_encrypt_key, account->smime_encrypt_key);
	gui->smime_encrypt_key_select = (GtkButton *)glade_xml_get_widget (gui->xml, "smime_encrypt_key_select");
	gui->smime_encrypt_key_clear = (GtkButton *)glade_xml_get_widget (gui->xml, "smime_encrypt_key_clear");
	g_signal_connect(gui->smime_encrypt_key_select, "clicked", G_CALLBACK(smime_encrypt_key_select), gui);
	g_signal_connect(gui->smime_encrypt_key_clear, "clicked", G_CALLBACK(smime_encrypt_key_clear), gui);

	gui->smime_encrypt_default = (GtkToggleButton *)glade_xml_get_widget (gui->xml, "smime_encrypt_default");
	gtk_toggle_button_set_active(gui->smime_encrypt_default, account->smime_encrypt_default);
	gui->smime_encrypt_to_self = (GtkToggleButton *)glade_xml_get_widget (gui->xml, "smime_encrypt_to_self");
	gtk_toggle_button_set_active(gui->smime_encrypt_to_self, account->smime_encrypt_to_self);
	smime_changed(gui);
#else
	{
		/* Since we don't have NSS, hide the S/MIME config options */
		GtkWidget *frame;
		
		frame = glade_xml_get_widget (gui->xml, "smime_vbox");
		gtk_widget_destroy (frame);
	}
#endif /* HAVE_NSS */

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
	gboolean writeable;
	
	printf("account gui setup\n");
	
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
	providers = camel_provider_list(TRUE);
	
	/* sort the providers, remote first */
	providers = g_list_sort (providers, (GCompareFunc) provider_compare);
	
	for (l = providers; l; l = l->next) {
		CamelProvider *provider = l->data;
		
		if (!(!strcmp (provider->domain, "mail") || !strcmp (provider->domain, "news")))
			continue;
		
		item = NULL;
		if (provider->object_types[CAMEL_PROVIDER_STORE] && provider->flags & CAMEL_PROVIDER_IS_SOURCE) {
			item = gtk_menu_item_new_with_label (provider->name);
			g_object_set_data ((GObject *) gui->source.type, provider->protocol, item);
			g_object_set_data ((GObject *) item, "provider", provider);
			g_object_set_data ((GObject *) item, "number", GUINT_TO_POINTER (si));
			g_signal_connect (item, "activate", G_CALLBACK (source_type_changed), gui);
			
			gtk_menu_shell_append(GTK_MENU_SHELL(stores), item);
			
			gtk_widget_show (item);
			
			if (!fstore) {
				fstore = item;
				hstore = si;
			}
			
			if (source_proto && !strcasecmp (provider->protocol, source_proto)) {
				fstore = item;
				hstore = si;
			}
			
			si++;
		}
		
		if (provider->object_types[CAMEL_PROVIDER_TRANSPORT]) {
			item = gtk_menu_item_new_with_label (provider->name);
			g_object_set_data ((GObject *) gui->transport.type, provider->protocol, item);
			g_object_set_data ((GObject *) item, "provider", provider);
			g_object_set_data ((GObject *) item, "number", GUINT_TO_POINTER (ti));
			g_signal_connect (item, "activate", G_CALLBACK (transport_type_changed), gui);
			
			gtk_menu_shell_append(GTK_MENU_SHELL(transports), item);
			
			gtk_widget_show (item);
			
			if (CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider))
				gtk_widget_set_sensitive (item, FALSE);
			
			if (!ftransport) {
				ftransport = item;
				htransport = ti;
			}
			
			if (transport_proto && !strcasecmp (provider->protocol, transport_proto)) {
				ftransport = item;
				htransport = ti;
			}
			
			ti++;
		}
		
		if (item && provider->authtypes) {
			/*GdkFont *font = GTK_WIDGET (item)->style->font;*/
			CamelServiceAuthType *at;
			int width;
			GList *a;
			
			for (a = provider->authtypes; a; a = a->next) {
				at = a->data;
				
				/* Just using string length is probably good enough,
				   as we only use the width of the widget, not the string */
				/*width = gdk_string_width (font, at->name);*/
				width = strlen(at->name) * 14;
				if (width > max_width) {
					max_authname = at->name;
					max_width = width;
				}
			}
		}
	}
	g_list_free (providers);
	
	/* add a "None" option to the stores menu */
	item = gtk_menu_item_new_with_label (_("None"));
	g_object_set_data ((GObject *) item, "provider", NULL);
	g_signal_connect (item, "activate", G_CALLBACK (source_type_changed), gui);
	
	gtk_menu_shell_append(GTK_MENU_SHELL(stores), item);
	
	gtk_widget_show (item);
	
	if (!fstore || !source_proto) {
		fstore = item;
		hstore = si;
	}
	
	/* set the menus on the optionmenus */
	gtk_option_menu_remove_menu (gui->source.type);
	gtk_option_menu_set_menu (gui->source.type, stores);
	
	gtk_option_menu_remove_menu (gui->transport.type);
	gtk_option_menu_set_menu (gui->transport.type, transports);
	
	/* Force the authmenus to the width of the widest element */
	if (max_authname) {
		GtkWidget *menu;
		GtkRequisition size_req;
		
		menu = gtk_menu_new ();
		item = gtk_menu_item_new_with_label (max_authname);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show_all (menu);
		gtk_option_menu_set_menu (gui->source.authtype, menu);
		gtk_widget_show (GTK_WIDGET (gui->source.authtype));
		gtk_widget_size_request (GTK_WIDGET (gui->source.authtype),
					 &size_req);
		
		gtk_widget_set_size_request (GTK_WIDGET (gui->source.authtype),
				      size_req.width, -1);
		gtk_widget_set_size_request (GTK_WIDGET (gui->transport.authtype),
				      size_req.width, -1);
	}
	
	if (top != NULL) {
		gtk_widget_show (top);
	}
	
	if (fstore) {
		g_signal_emit_by_name (fstore, "activate");
		gtk_option_menu_set_history (gui->source.type, hstore);
	}
	
	if (ftransport) {
		g_signal_emit_by_name (ftransport, "activate");
		gtk_option_menu_set_history (gui->transport.type, htransport);
	}
	
	if (source_proto) {
		setup_service (gui, &gui->source, gui->account->source);
		gui->source.provider_type = CAMEL_PROVIDER_STORE;
		g_free (source_proto);
		if (gui->account->source->auto_check) {
			gtk_toggle_button_set_active (gui->source_auto_check, TRUE);
			gtk_spin_button_set_value (gui->source_auto_check_min,
						   gui->account->source->auto_check_time);
		}
	}
	
	if (transport_proto) {
		if (setup_service (gui, &gui->transport, gui->account->transport))
			gtk_toggle_button_set_active (gui->transport_needs_auth, TRUE);
		gui->transport.provider_type = CAMEL_PROVIDER_TRANSPORT;
		g_free (transport_proto);
	}

	/* FIXME: drive by table?? */
	if (!e_account_writable (gui->account, E_ACCOUNT_SOURCE_URL)) {
		gtk_widget_set_sensitive (gui->source.container, FALSE);
	} else {
		gtk_widget_set_sensitive (gui->source.container, TRUE);
		
		if (gui->source.provider) {
			writeable = e_account_writable_option (gui->account, gui->source.provider->protocol, "auth");
			gtk_widget_set_sensitive ((GtkWidget *) gui->source.authtype, writeable);
			gtk_widget_set_sensitive ((GtkWidget *) gui->source.check_supported, writeable);
			
			writeable = e_account_writable_option (gui->account, gui->source.provider->protocol, "use_ssl");
			gtk_widget_set_sensitive ((GtkWidget *) gui->source.use_ssl, writeable);
			
			writeable = e_account_writable (gui->account, E_ACCOUNT_SOURCE_SAVE_PASSWD);
			gtk_widget_set_sensitive ((GtkWidget *) gui->source.remember, writeable);
		}
	}
	
	if (!e_account_writable (gui->account, E_ACCOUNT_TRANSPORT_URL)) {
		gtk_widget_set_sensitive (gui->transport.container, FALSE);
	} else {
		gtk_widget_set_sensitive (gui->transport.container, TRUE);
		
		if (gui->transport.provider) {
			writeable = e_account_writable_option (gui->account, gui->transport.provider->protocol, "auth");
			gtk_widget_set_sensitive ((GtkWidget *) gui->transport.authtype, writeable);
			gtk_widget_set_sensitive ((GtkWidget *) gui->transport.check_supported, writeable);
			
			writeable = e_account_writable_option (gui->account, gui->transport.provider->protocol, "use_ssl");
			gtk_widget_set_sensitive ((GtkWidget *) gui->transport.use_ssl, writeable);
			
			writeable = e_account_writable (gui->account, E_ACCOUNT_TRANSPORT_SAVE_PASSWD);
			gtk_widget_set_sensitive ((GtkWidget *) gui->transport.remember, writeable);
		}
	}

	gtk_widget_set_sensitive((GtkWidget *)gui->drafts_folder_button, e_account_writable(gui->account, E_ACCOUNT_DRAFTS_FOLDER_URI));
	gtk_widget_set_sensitive((GtkWidget *)gui->sent_folder_button, e_account_writable(gui->account, E_ACCOUNT_SENT_FOLDER_URI));
	gtk_widget_set_sensitive((GtkWidget *)gui->restore_folders_button,
				 e_account_writable(gui->account, E_ACCOUNT_SENT_FOLDER_URI)
				 || e_account_writable(gui->account, E_ACCOUNT_DRAFTS_FOLDER_URI));
	gtk_widget_set_sensitive((GtkWidget *)gui->sig_menu, e_account_writable(gui->account, E_ACCOUNT_ID_SIGNATURE));
	gtk_widget_set_sensitive(glade_xml_get_widget(gui->xml, "sigAddNew"),
				 gconf_client_key_is_writable(mail_config_get_gconf_client(),
							      "/apps/evolution/mail/signatures", NULL));
	gtk_widget_set_sensitive((GtkWidget *)gui->source_auto_check, e_account_writable(gui->account, E_ACCOUNT_SOURCE_AUTO_CHECK));
	gtk_widget_set_sensitive((GtkWidget *)gui->source_auto_check_min, e_account_writable(gui->account, E_ACCOUNT_SOURCE_AUTO_CHECK_TIME));
}

static void
save_service (MailAccountGuiService *gsvc, GHashTable *extra_config, EAccountService *service)
{
	CamelURL *url;
	const char *str;
	
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
			url->user = g_strstrip (g_strdup (str));
	}
	
	if (CAMEL_PROVIDER_ALLOWS (gsvc->provider, CAMEL_URL_PART_AUTH) &&
	    GTK_WIDGET_IS_SENSITIVE (gsvc->authtype) && gsvc->authitem && url->user) {
		CamelServiceAuthType *authtype;
		
		authtype = g_object_get_data(G_OBJECT(gsvc->authitem), "authtype");
		if (authtype && authtype->authproto && *authtype->authproto)
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
		const char *use_ssl;
		
		use_ssl = g_object_get_data(G_OBJECT(gsvc->ssl_selected), "use_ssl");
		
		/* set the value to either "always" or "when-possible"
		   but don't bother setting it for "never" */
		if (strcmp (use_ssl, "never"))
			camel_url_set_param (url, "use_ssl", use_ssl);
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

static void
add_new_store (char *uri, CamelStore *store, void *user_data)
{
	MailComponent *component = mail_component_peek ();
	EAccount *account = user_data;
	
	if (store == NULL)
		return;
	
	mail_component_add_store (component, store, account->name);
}

gboolean
mail_account_gui_save (MailAccountGui *gui)
{
	EAccount *account, *new;
	CamelProvider *provider = NULL;
	gboolean is_new = FALSE;
	const char *new_name;
	gboolean is_storage;
	
	if (!mail_account_gui_identity_complete (gui, NULL) ||
	    !mail_account_gui_source_complete (gui, NULL) ||
	    !mail_account_gui_transport_complete (gui, NULL) ||
	    !mail_account_gui_management_complete (gui, NULL))
		return FALSE;
	
	new = gui->account;
	
	/* this would happen at an inconvenient time in the druid,
	 * but the druid performs its own check so this can't happen
	 * here. */
	
	new_name = gtk_entry_get_text (gui->account_name);
	account = mail_config_get_account_by_name (new_name);
	
	if (account && account != new) {
		e_error_run((GtkWindow *)gtk_widget_get_toplevel((GtkWidget *)gui->account_name),
			    "mail:account-notunique", NULL);
		return FALSE;
	}
	
	account = new;
	
	new = e_account_new ();
	new->name = g_strdup (new_name);
	new->enabled = account->enabled;
	
	/* construct the identity */
	new->id->name = g_strdup (gtk_entry_get_text (gui->full_name));
	new->id->address = g_strdup (gtk_entry_get_text (gui->email_address));
	new->id->reply_to = g_strdup (gtk_entry_get_text (gui->reply_to));
	new->id->organization = g_strdup (gtk_entry_get_text (gui->organization));
	
	/* signatures */
	new->id->sig_uid = g_strdup (gui->sig_uid);
	
	/* source */
	save_service (&gui->source, gui->extra_config, new->source);
	if (new->source->url)
		provider = camel_provider_get(new->source->url, NULL);
	
	new->source->auto_check = gtk_toggle_button_get_active (gui->source_auto_check);
	if (new->source->auto_check)
		new->source->auto_check_time = gtk_spin_button_get_value_as_int (gui->source_auto_check_min);
	
	/* transport */
	if (CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (gui->transport.provider)) {
		/* The transport URI is the same as the source URI. */
		save_service (&gui->source, gui->extra_config, new->transport);
	} else
		save_service (&gui->transport, NULL, new->transport);
	
	/* Check to make sure that the Drafts folder uri is "valid" before assigning it */
	if (mail_config_get_account_by_source_url (gui->drafts_folder_uri) ||
		!strncmp (gui->drafts_folder_uri, "mbox:", 5)) {
		new->drafts_folder_uri = em_uri_from_camel (gui->drafts_folder_uri);
	} else {
		/* assign defaults - the uri is unknown to us (probably pointed to an old source url) */
		new->drafts_folder_uri = em_uri_from_camel(mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS));
	}
	
	/* Check to make sure that the Sent folder uri is "valid" before assigning it */
	if (mail_config_get_account_by_source_url (gui->sent_folder_uri) ||
		!strncmp (gui->sent_folder_uri, "mbox:", 5)) {
		new->sent_folder_uri = em_uri_from_camel (gui->sent_folder_uri);
	} else {
		/* assign defaults - the uri is unknown to us (probably pointed to an old source url) */
		new->sent_folder_uri = em_uri_from_camel(mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT));
	}
	
	new->always_cc = gtk_toggle_button_get_active (gui->always_cc);
	new->cc_addrs = g_strdup (gtk_entry_get_text (gui->cc_addrs));
	new->always_bcc = gtk_toggle_button_get_active (gui->always_bcc);
	new->bcc_addrs = g_strdup (gtk_entry_get_text (gui->bcc_addrs));
	
	new->pgp_key = g_strdup (gtk_entry_get_text (gui->pgp_key));
	new->pgp_encrypt_to_self = gtk_toggle_button_get_active (gui->pgp_encrypt_to_self);
	new->pgp_always_sign = gtk_toggle_button_get_active (gui->pgp_always_sign);
	new->pgp_no_imip_sign = gtk_toggle_button_get_active (gui->pgp_no_imip_sign);
	new->pgp_always_trust = gtk_toggle_button_get_active (gui->pgp_always_trust);
	
#if defined (HAVE_NSS)
	new->smime_sign_default = gtk_toggle_button_get_active (gui->smime_sign_default);
	new->smime_sign_key = g_strdup (gtk_entry_get_text (gui->smime_sign_key));
	
	new->smime_encrypt_default = gtk_toggle_button_get_active (gui->smime_encrypt_default);
	new->smime_encrypt_key = g_strdup (gtk_entry_get_text (gui->smime_encrypt_key));
	new->smime_encrypt_to_self = gtk_toggle_button_get_active (gui->smime_encrypt_to_self);
#endif /* HAVE_NSS */
	
	is_storage = provider && (provider->flags & CAMEL_PROVIDER_IS_STORAGE);
	
	if (!mail_config_find_account (account)) {
		/* this is a new account so add it to our account-list */
		is_new = TRUE;
	}
	
	/* update the old account with the new settings */
	e_account_import (account, new);
	g_object_unref (new);
	
	if (is_new) {
		mail_config_add_account (account);
		
		/* if the account provider is something we can stick
		   in the folder-tree and not added by some other
		   component, then get the CamelStore and add it to
		   the folder-tree */
		if (is_storage && account->enabled)
			mail_get_store (account->source->url, NULL, add_new_store, account);
	} else {
		e_account_list_change (mail_config_get_accounts (), account);
	}
	
	if (gtk_toggle_button_get_active (gui->default_account))
		mail_config_set_default_account (account);
	
	mail_config_save_accounts ();
	
	mail_autoreceive_setup ();
	
	return TRUE;
}

void
mail_account_gui_destroy (MailAccountGui *gui)
{
	ESignatureList *signatures;
	
	g_object_unref (gui->xml);
	g_object_unref (gui->account);
	
	signatures = mail_config_get_signatures ();
	g_signal_handler_disconnect (signatures, gui->sig_added_id);
	g_signal_handler_disconnect (signatures, gui->sig_removed_id);
	g_signal_handler_disconnect (signatures, gui->sig_changed_id);
	
	if (gui->extra_config)
		g_hash_table_destroy (gui->extra_config);
	
	g_free (gui->drafts_folder_uri);
	g_free (gui->sent_folder_uri);
	g_free (gui);
}
