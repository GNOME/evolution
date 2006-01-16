/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2001-2004 Novell, Inc.
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
 */

/* ExchangeConfigListener: a class that listens to the config database
 * and handles creating the ExchangeAccount object and making sure that
 * default folders are updated as needed.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exchange-config-listener.h"
#include "exchange-operations.h"
#include "exchange-change-password.h"

#include <exchange-constants.h>
#include <exchange-hierarchy.h>
#include <e-folder-exchange.h>
#include <e2k-marshal.h>
#include <e2k-uri.h>
#include <camel/camel-url.h>

#include <e-util/e-error.h>

#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>
#include <libedataserverui/e-passwords.h>
#include <glade/glade-xml.h>

#include <stdlib.h>
#include <string.h>

#define FILENAME CONNECTOR_GLADEDIR "/exchange-passwd-expiry.glade"
#define ROOTNODE "passwd_exp_dialog"

struct _ExchangeConfigListenerPrivate {
	GConfClient *gconf;
	guint idle_id;

	char *configured_uri, *configured_name;
	EAccount *configured_account;

	ExchangeAccount *exchange_account;
};

typedef struct {
	const char *name;
	const char *uri;
	int type;
}FolderInfo;

enum {
	EXCHANGE_ACCOUNT_CREATED,
	EXCHANGE_ACCOUNT_REMOVED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

#define PARENT_TYPE E_TYPE_ACCOUNT_LIST

#define CONF_KEY_SELECTED_CAL_SOURCES "/apps/evolution/calendar/display/selected_calendars"
#define CONF_KEY_SELECTED_TASKS_SOURCES "/apps/evolution/calendar/tasks/selected_tasks"

static EAccountListClass *parent_class = NULL;

static void dispose (GObject *object);
static void finalize (GObject *object);

static void account_added   (EAccountList *account_listener,
			     EAccount     *account);
static void account_changed (EAccountList *account_listener,
			     EAccount     *account);
static void account_removed (EAccountList *account_listener,
			     EAccount     *account);
static void exchange_add_autocompletion_folders (GConfClient *gc_client, 
						 ExchangeAccount *account);
static gboolean exchange_camel_urls_is_equal (const gchar *url1, 
					      const gchar *url2);
static void remove_selected_non_offline_esources (ExchangeAccount *account, 
						  const char *gconf_key);
static void
class_init (GObjectClass *object_class)
{
	EAccountListClass *e_account_list_class =
		E_ACCOUNT_LIST_CLASS (object_class);

	parent_class = g_type_class_ref (PARENT_TYPE);

	/* virtual method override */
	object_class->dispose = dispose;
	object_class->finalize = finalize;

	e_account_list_class->account_added   = account_added;
	e_account_list_class->account_changed = account_changed;
	e_account_list_class->account_removed = account_removed;

	/* signals */
	signals[EXCHANGE_ACCOUNT_CREATED] =
		g_signal_new ("exchange_account_created",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ExchangeConfigListenerClass, exchange_account_created),
			      NULL, NULL,
			      e2k_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	signals[EXCHANGE_ACCOUNT_REMOVED] =
		g_signal_new ("exchange_account_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ExchangeConfigListenerClass, exchange_account_removed),
			      NULL, NULL,
			      e2k_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
init (GObject *object)
{
	ExchangeConfigListener *config_listener =
		EXCHANGE_CONFIG_LISTENER (object);

	config_listener->priv = g_new0 (ExchangeConfigListenerPrivate, 1);
}

static void
dispose (GObject *object)
{
	ExchangeConfigListener *config_listener =
		EXCHANGE_CONFIG_LISTENER (object);

	if (config_listener->priv->idle_id) {
		g_source_remove (config_listener->priv->idle_id);
		config_listener->priv->idle_id = 0;
	}

	if (config_listener->priv->gconf) {
		g_object_unref (config_listener->priv->gconf);
		config_listener->priv->gconf = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	ExchangeConfigListener *config_listener =
		EXCHANGE_CONFIG_LISTENER (object);

	g_free (config_listener->priv->configured_name);
	g_free (config_listener->priv->configured_uri);
	g_free (config_listener->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

E2K_MAKE_TYPE (exchange_config_listener, ExchangeConfigListener, class_init, init, PARENT_TYPE)


#define EVOLUTION_URI_PREFIX     "evolution:/"
#define EVOLUTION_URI_PREFIX_LEN (sizeof (EVOLUTION_URI_PREFIX) - 1)

static EFolder *
standard_folder (ExchangeAccount *account, const char *folder_type)
{
	const char *uri;

	uri = exchange_account_get_standard_uri (account, folder_type);
	if (!uri)
		return NULL;
	return exchange_account_get_folder (account, uri);
}

static void
set_special_mail_folder (ExchangeAccount *account, const char *folder_type,
			 char **physical_uri)
{
	EFolder *folder;

	folder = standard_folder (account, folder_type);
	if (!folder)
		return;

	g_free (*physical_uri);
	*physical_uri = g_strdup (e_folder_get_physical_uri (folder));
}

static void
add_defaults_for_account (ExchangeConfigListener *config_listener,
			  E2kContext *ctx,
			  ExchangeAccount *account)
{
	EAccount *eaccount;

	exchange_add_autocompletion_folders (config_listener->priv->gconf, account);

	eaccount = config_listener->priv->configured_account;
	set_special_mail_folder (account, "drafts",
				 &eaccount->drafts_folder_uri);
	set_special_mail_folder (account, "sentitems",
				 &eaccount->sent_folder_uri);
	e_account_list_change (E_ACCOUNT_LIST (config_listener), eaccount);
	e_account_list_save (E_ACCOUNT_LIST (config_listener));
}


static gboolean
is_active_exchange_account (EAccount *account)
{
	if (!account->enabled)
		return FALSE;
	if (!account->source || !account->source->url)
		return FALSE;
	return (strncmp (account->source->url, EXCHANGE_URI_PREFIX, 11) == 0);
}

#if 0 /* Not using the following code anywhere for the moment */
static void
add_account_esources (ExchangeAccount *account,
		      GSList *folders)
{
	ESource *source = NULL;
	ESourceGroup *cal_source_group = NULL;
	ESourceGroup *tasks_source_group = NULL;
	ESourceGroup *contacts_source_group = NULL;
	char *relative_uri = NULL, *username = NULL;
#if 0
	GSList *ids;
#endif
	GConfClient *client;
	int mode;
	ESourceList *cal_source_list, *tasks_source_list, *contacts_source_list;
	FolderInfo *folder=NULL;
	gboolean offline_mode = FALSE;

	client = gconf_client_get_default ();

	cal_source_list = e_source_list_new_for_gconf ( client, CONF_KEY_CAL);
	tasks_source_list = e_source_list_new_for_gconf ( client, CONF_KEY_TASKS);
	contacts_source_list = e_source_list_new_for_gconf ( client, CONF_KEY_CONTACTS);

	exchange_account_is_offline_sync_set (account, &mode);
	if (mode == OFFLINE_MODE) {
		/* If account is marked for offline sync during account
		 * creation, mark all the folders for offline sync 
		 */
		offline_mode = TRUE;
	}

	username = exchange_account_get_username (account);

	/* For each component create a source group */ 

	cal_source_group = e_source_group_new (account->account_name,
					       EXCHANGE_URI_PREFIX);
	tasks_source_group = e_source_group_new (account->account_name, 
					   	 EXCHANGE_URI_PREFIX);
	contacts_source_group = e_source_group_new (account->account_name, 
					   	    EXCHANGE_URI_PREFIX);

	if (!e_source_list_add_group (contacts_source_list, contacts_source_group, -1) ||
	    !e_source_list_add_group (cal_source_list, cal_source_group, -1) ||
	    !e_source_list_add_group (tasks_source_list, tasks_source_group, -1)) {
		goto done;
	}
	for ( ; folders != NULL ; folders = g_slist_next (folders)) {
		/* Create source for each folder and add to the group */

		folder = folders->data;
		if (folder->type == EXCHANGE_CONTACTS_FOLDER) {
			source = e_source_new_with_absolute_uri (folder->name,
								 folder->uri);
			if (offline_mode)
				e_source_set_property (source, "offline_sync", "1");
			if (username)
				e_source_set_property (source, "username", username);
			e_source_set_property (source, "auth", "1");
			e_source_set_property (source, "auth-domain", "Exchange");
			e_source_group_add_source (contacts_source_group, 
						   source, -1);
			g_object_unref (source);
		}
		else if (folder->type == EXCHANGE_CALENDAR_FOLDER){
			relative_uri = g_strdup (folder->uri + 
						 strlen (EXCHANGE_URI_PREFIX));
			source = e_source_new (folder->name, relative_uri);
			if (offline_mode)
				e_source_set_property (source, "offline_sync", "1");
			if (username)
				e_source_set_property (source, "username", username);
			e_source_set_property (source, "auth", "1");
			e_source_set_property (source, "auth-domain", "Exchange");
			e_source_group_add_source (cal_source_group, 
						   source, -1);
#if 0
			ids = gconf_client_get_list (client,
						     CONF_KEY_SELECTED_CAL_SOURCES,
						     GCONF_VALUE_STRING, NULL);
			ids = g_slist_append (ids,
					      g_strdup (e_source_peek_uid (source)));
			gconf_client_set_list (client,
					       CONF_KEY_SELECTED_CAL_SOURCES,
					       GCONF_VALUE_STRING, ids, NULL);
			g_slist_foreach (ids, (GFunc) g_free, NULL);
			g_slist_free (ids);
#endif
			g_object_unref (source);
			g_free (relative_uri);

	
		}
		else if (folder->type == EXCHANGE_TASKS_FOLDER){
			relative_uri = g_strdup (folder->uri + 
						 strlen (EXCHANGE_URI_PREFIX));
			source = e_source_new (folder->name, relative_uri);
			if (offline_mode == ONLINE_MODE)
				e_source_set_property (source, "offline_sync", "1");
			if (username)
				e_source_set_property (source, "username", username);
			e_source_set_property (source, "auth", "1");
			e_source_set_property (source, "auth-domain", "Exchange");
			e_source_group_add_source (tasks_source_group, 
						   source, -1);
#if 0
			ids = gconf_client_get_list (client,
						     CONF_KEY_SELECTED_TASKS_SOURCES,
						     GCONF_VALUE_STRING, NULL);
			ids = g_slist_append (ids,
					      g_strdup (e_source_peek_uid (source)));
			gconf_client_set_list (client,
					       CONF_KEY_SELECTED_TASKS_SOURCES,
					       GCONF_VALUE_STRING, ids, NULL);
			g_slist_foreach (ids, (GFunc) g_free, NULL);
			g_slist_free (ids);
#endif
			g_object_unref (source);
			g_free (relative_uri);
		}
	}
		
	e_source_list_sync (cal_source_list, NULL);
	e_source_list_sync (tasks_source_list, NULL);
	e_source_list_sync (contacts_source_list, NULL);

done:
	g_object_unref (cal_source_group);
	g_object_unref (tasks_source_group);
	g_object_unref (contacts_source_group);

	g_object_unref (cal_source_list);
	g_object_unref (tasks_source_list);
	g_object_unref (contacts_source_list);

	g_object_unref (client);
}

static void
add_new_sources (ExchangeAccount *account)
{
	GPtrArray *exchange_folders;

	exchange_folders = exchange_account_get_folders (account);                                                                                 
        if (exchange_folders && exchange_folders->len > 0) {
		int i;
		const char *folder_type;
		const char *folder_name;
		const char *folder_uri;
		int type;
		EFolder *folder;
		ExchangeHierarchy *hier;
		gboolean create_esource = FALSE;

		for (i = 0; i < exchange_folders->len; i++) {

			folder = exchange_folders->pdata[i];
			hier = e_folder_exchange_get_hierarchy (folder);
			if (hier->type != EXCHANGE_HIERARCHY_PUBLIC) {
				folder_name = e_folder_get_name (folder);
				folder_uri = e_folder_get_physical_uri (folder); 
				folder_type = e_folder_get_type_string (folder);

				if (!(strcmp (folder_type, "calendar")) ||
				    !(strcmp (folder_type, "calendar/public"))) {
						type = EXCHANGE_CALENDAR_FOLDER;
						create_esource = TRUE;
				}
				else if (!(strcmp (folder_type, "tasks")) ||
					 !(strcmp (folder_type, "tasks/public"))) {
						type = EXCHANGE_TASKS_FOLDER;
						create_esource = TRUE;
				}
				else if (!(strcmp (folder_type, "contacts")) ||
					 !(strcmp (folder_type, "contacts/public")) ||
					 !(strcmp (folder_type, "contacts/ldap"))) {
						type = EXCHANGE_CONTACTS_FOLDER;
						create_esource = TRUE;
				}
				else {
					create_esource = FALSE;
				}

				if (create_esource)
					add_folder_esource (account, type, 
							folder_name, folder_uri);
			} /* End hierarchy type check */
		} /* End for loop */
        } /* End check for a list of folders */
}

static void
add_sources (ExchangeAccount *account)
{
	GPtrArray *exchange_folders;

	exchange_folders = exchange_account_get_folders (account);                                                                                 
        if (exchange_folders && exchange_folders->len > 0) {
		int i;
		const char *folder_type;
		EFolder *folder;
		GSList *folders = NULL;

		for (i = 0; i < exchange_folders->len; i++) {
			FolderInfo *folder_info = g_new0 (FolderInfo, 1);

			folder = exchange_folders->pdata[i];
			folder_type = e_folder_get_type_string (folder);

			if (!(strcmp (folder_type, "calendar")) ||
			    !(strcmp (folder_type, "calendar/public"))) {
				folder_info->name = e_folder_get_name (folder);
				folder_info->uri = e_folder_get_physical_uri (folder); 
				folder_info->type = EXCHANGE_CALENDAR_FOLDER;
				folders = g_slist_append (folders, folder_info);
			}
			else if (!(strcmp (folder_type, "tasks")) ||
			    	 !(strcmp (folder_type, "tasks/public"))) {
				folder_info->name = e_folder_get_name (folder);
				folder_info->uri = e_folder_get_physical_uri (folder); 
				folder_info->type = EXCHANGE_TASKS_FOLDER;
				folders = g_slist_append (folders, folder_info);
			}
			else if (!(strcmp (folder_type, "contacts")) ||
			    	 !(strcmp (folder_type, "contacts/public")) ||
			    	 !(strcmp (folder_type, "contacts/ldap"))) {
				folder_info->name = e_folder_get_name (folder);
				folder_info->uri = e_folder_get_physical_uri (folder); 
				folder_info->type = EXCHANGE_CONTACTS_FOLDER;
				folders = g_slist_append (folders, folder_info);
			}
			else
				g_free (folder_info);
		}
		/* Add e-sources for all the folders */
		add_account_esources (account, folders);
		g_slist_foreach (folders, (GFunc) g_free, NULL);
		g_slist_free (folders);
        }
}
#endif

static void 
remove_account_esource (ExchangeAccount *account, 
		        FolderType folder_type)
{
	ESourceGroup *group;
	ESource *source = NULL;
	GSList *groups;
	GSList *sources;
	GSList *ids, *node_to_be_deleted;
	gboolean found_group;
	const char *source_uid;
	GConfClient *client;
	ESourceList *source_list = NULL;

	/* Remove the ESource group, to remove all the folders in a component */

	client = gconf_client_get_default ();

	if (folder_type == EXCHANGE_CONTACTS_FOLDER)
		source_list = e_source_list_new_for_gconf ( client, 
							CONF_KEY_CONTACTS);
	else if (folder_type == EXCHANGE_CALENDAR_FOLDER)
		source_list = e_source_list_new_for_gconf ( client, 
							CONF_KEY_CAL);
	else if (folder_type == EXCHANGE_TASKS_FOLDER)
		source_list = e_source_list_new_for_gconf ( client,
							CONF_KEY_TASKS);

	groups = e_source_list_peek_groups (source_list);
	found_group = FALSE;

	for ( ; groups != NULL && !found_group; groups = g_slist_next (groups)) {
		group = E_SOURCE_GROUP (groups->data);

		if (strcmp (e_source_group_peek_name (group), account->account_name) == 0
                    &&
                    strcmp (e_source_group_peek_base_uri (group), EXCHANGE_URI_PREFIX) == 0) {
			sources = e_source_group_peek_sources (group);

			for( ; sources != NULL; sources = g_slist_next (sources)) {
				source = E_SOURCE (sources->data);
				source_uid = e_source_peek_uid (source);

				/* Remove from the selected folders */
				if (folder_type == EXCHANGE_CALENDAR_FOLDER) {
					ids = gconf_client_get_list (
							client, 
							CONF_KEY_SELECTED_CAL_SOURCES , 
							GCONF_VALUE_STRING, NULL);
					if (ids) {
						node_to_be_deleted = g_slist_find_custom (
									ids, 
									source_uid, 
									(GCompareFunc) strcmp);
						if (node_to_be_deleted) {
							g_free (node_to_be_deleted->data);
							ids = g_slist_delete_link (ids, 
									node_to_be_deleted);
							gconf_client_set_list (client, 
								CONF_KEY_SELECTED_CAL_SOURCES,
								GCONF_VALUE_STRING, ids, NULL);
						}
						g_slist_foreach (ids, (GFunc) g_free, NULL);
						g_slist_free (ids);
					}
				}
				else if (folder_type == EXCHANGE_TASKS_FOLDER) {
					ids = gconf_client_get_list (client, 
								CONF_KEY_SELECTED_TASKS_SOURCES , 
								GCONF_VALUE_STRING, NULL);
					if (ids) {
						node_to_be_deleted = g_slist_find_custom (
									ids,
									source_uid, 
									(GCompareFunc) strcmp);
						if (node_to_be_deleted) {
							g_free (node_to_be_deleted->data);
							ids = g_slist_delete_link (ids, 
									node_to_be_deleted);
							gconf_client_set_list (client, 
								CONF_KEY_SELECTED_TASKS_SOURCES,
								GCONF_VALUE_STRING, ids, NULL);
						}
						g_slist_foreach (ids, (GFunc) g_free, NULL);
						g_slist_free (ids);
					}
				}
			}
			e_source_list_remove_group (source_list, group);
			e_source_list_sync (source_list, NULL);
			found_group = TRUE;
		}
	}
	g_object_unref (source_list);
	g_object_unref (client);
}

static void
remove_account_esources (ExchangeAccount *account)
{
	/* Remove ESources for all the folders in all the components */

	remove_account_esource (account, EXCHANGE_CALENDAR_FOLDER);
	remove_account_esource (account, EXCHANGE_TASKS_FOLDER);
	remove_account_esource (account, EXCHANGE_CONTACTS_FOLDER);
}

#ifdef HAVE_KRB5
static char * 
get_new_exchange_password (ExchangeAccount *account)
{
	char *old_password, *new_password;

	old_password = exchange_account_get_password (account);
	new_password = exchange_get_new_password (old_password, 0);
	
	if (new_password) {
		exchange_account_set_password (account, 
					       old_password, 
					       new_password);
		g_free (old_password);
		return new_password;
	}
	g_free (old_password);
	return NULL;
}
#endif

#ifdef HAVE_KRB5
static void
change_passwd_cb (GtkWidget *button, ExchangeAccount *account)
{
	char *current_passwd, *new_passwd;

	gtk_widget_hide (gtk_widget_get_toplevel(button));
	current_passwd = exchange_account_get_password (account);
	new_passwd = exchange_get_new_password (current_passwd, TRUE);
	exchange_account_set_password (account, current_passwd, new_passwd);
	g_free (current_passwd);
	g_free (new_passwd);
}
#endif

static void
display_passwd_expiry_message (int max_passwd_age, ExchangeAccount *account)
{
	GladeXML *xml;
	GtkWidget *top_widget, *change_passwd_button;
	GtkResponseType response;
	GtkLabel *warning_msg_label;
	char *passwd_expiry_msg =
		g_strdup_printf (_("Your password will expire in the next %d days"), max_passwd_age);
	
	xml = glade_xml_new (FILENAME, ROOTNODE, NULL);
	g_return_if_fail (xml != NULL);
	top_widget = glade_xml_get_widget (xml, ROOTNODE);
	g_return_if_fail (top_widget != NULL);

	warning_msg_label = GTK_LABEL (glade_xml_get_widget (xml,
						"passwd_exp_label"));
	gtk_label_set_text (warning_msg_label, passwd_expiry_msg);
	change_passwd_button = glade_xml_get_widget (xml,
						"change_passwd_button");
	gtk_widget_set_sensitive (change_passwd_button, TRUE);
#ifdef HAVE_KRB5
	g_signal_connect (change_passwd_button,
			  "clicked",
			  G_CALLBACK (change_passwd_cb),
			  account); 
#endif
	response = gtk_dialog_run (GTK_DIALOG (top_widget));
	
	gtk_widget_destroy (top_widget);
	g_object_unref (xml);
	g_free (passwd_expiry_msg);
}

ExchangeAccountResult 
exchange_config_listener_authenticate (ExchangeConfigListener *ex_conf_listener, ExchangeAccount *account) 
{
	ExchangeConfigListenerPrivate *priv;
	ExchangeAccountResult result;
	char *key, *password, *title, *new_password, *url_string;
	gboolean oldremember, remember = FALSE;
	CamelURL *camel_url;
	const char *remember_password;

	g_return_val_if_fail (EXCHANGE_IS_CONFIG_LISTENER (ex_conf_listener), EXCHANGE_ACCOUNT_CONFIG_ERROR);
	priv = ex_conf_listener->priv;

	camel_url = camel_url_new (priv->configured_uri, NULL);
	key = camel_url_to_string (camel_url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	remember_password = camel_url_get_param (camel_url, "save-passwd");
	password = e_passwords_get_password ("Exchange", key);
	if (!password) {
		oldremember = remember = exchange_account_is_save_password (account);	
		title = g_strdup_printf (_("Enter Password for %s"), account->account_name);
		password = e_passwords_ask_password (title, "Exchange", key, title,
						     E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET,
						     &remember, NULL);
		if (remember != oldremember) {
			exchange_account_set_save_password (account, remember);
			camel_url_set_param (camel_url, "save-passwd", remember? "true" : "false");
			url_string = camel_url_to_string (camel_url, 0);
			e_account_set_string (ex_conf_listener->priv->configured_account, E_ACCOUNT_SOURCE_URL, url_string);
			e_account_set_string (ex_conf_listener->priv->configured_account, E_ACCOUNT_TRANSPORT_URL, url_string);
			e_account_set_bool (ex_conf_listener->priv->configured_account, E_ACCOUNT_SOURCE_SAVE_PASSWD, remember);
			e_account_list_change (E_ACCOUNT_LIST (ex_conf_listener), ex_conf_listener->priv->configured_account);
			e_account_list_save (E_ACCOUNT_LIST (ex_conf_listener));
			g_free (url_string);
		}
		g_free (title);
	}
	else if (remember_password && !g_strcasecmp (remember_password, "false")) {
		/* get_password returns the password cached but user has not
		 * selected remember password option, forget this password
		 * whis is stored temporarily by e2k_validate_user(), to avoid
		 * asking for password again, at the end of account creation.
		 */
		e_passwords_forget_password ("Exchange", key);
		exchange_account_set_save_password (account, FALSE);
	}
 	exchange_account_connect (account, password, &result);
	g_free (password);
	if (result == EXCHANGE_ACCOUNT_PASSWORD_EXPIRED) {
#ifdef HAVE_KRB5
		new_password = get_new_exchange_password (account);
		if (new_password) {
			/* try connecting with new password */
 			exchange_account_connect (account, new_password, &result);
			g_free (new_password);
		}
#endif
	}
	else if (result == EXCHANGE_ACCOUNT_QUOTA_RECIEVE_ERROR ||
		 result == EXCHANGE_ACCOUNT_QUOTA_SEND_ERROR ||
		 result == EXCHANGE_ACCOUNT_QUOTA_WARN) {
		gchar *current_quota_usage;

		switch (result) {
			case EXCHANGE_ACCOUNT_QUOTA_RECIEVE_ERROR:
				current_quota_usage = g_strdup_printf ("%.2f", 
							account->mbox_size);
				e_error_run (NULL, "org-gnome-exchange-operations:account-quota-error", current_quota_usage);
				g_free (current_quota_usage);
				break;
			case EXCHANGE_ACCOUNT_QUOTA_SEND_ERROR:
				current_quota_usage = g_strdup_printf ("%.2f", 
							account->mbox_size);
				e_error_run (NULL, "org-gnome-exchange-operations:account-quota-send-error", current_quota_usage);
				g_free (current_quota_usage);
				break;
			case EXCHANGE_ACCOUNT_QUOTA_WARN:
				current_quota_usage = g_strdup_printf ("%.2f", 
							account->mbox_size);
				e_error_run (NULL, "org-gnome-exchange-operations:account-quota-warn", current_quota_usage);
				g_free (current_quota_usage);
				break;
			default:
				break;
		}
		/* reset result, so that we check if the password 
		 * expiry warning period 
		 */
		result = EXCHANGE_ACCOUNT_CONNECT_SUCCESS;
		
	}
	if (result == EXCHANGE_ACCOUNT_CONNECT_SUCCESS) {
		int max_pwd_age_days;

		/* check for password expiry warning */
		max_pwd_age_days = exchange_account_check_password_expiry (account);
		if (max_pwd_age_days >= 0) {
			display_passwd_expiry_message (max_pwd_age_days, account);
		}
	}
	g_free (key);
	camel_url_free (camel_url);
	return result;
}

static void
account_added (EAccountList *account_list, EAccount *account)
{
	ExchangeConfigListener *config_listener;
	ExchangeAccount *exchange_account;
	gint is_offline;

	if (!is_active_exchange_account (account))
		return;

	config_listener = EXCHANGE_CONFIG_LISTENER (account_list);
	if (config_listener->priv->configured_account) {
		/* Multiple accounts configured. */
		e_error_run (NULL, "org-gnome-exchange-operations:single-account-error", NULL);
		return;
	}

	/* New account! Yippee! */
	exchange_account = exchange_account_new (account_list, account);
	if (!exchange_account) {
		g_warning ("Could not parse exchange uri '%s'",
			   account->source->url);
		return;
	}

	config_listener->priv->exchange_account = exchange_account;
	config_listener->priv->configured_account = account;

	g_free (config_listener->priv->configured_uri);
	config_listener->priv->configured_uri = g_strdup (account->source->url);
	g_free (config_listener->priv->configured_name);
	config_listener->priv->configured_name = g_strdup (account->name);

	if (account == e_account_list_get_default (account_list)) {
		g_signal_connect_swapped (config_listener->priv->exchange_account,
					  "connected",
					  G_CALLBACK (add_defaults_for_account),
					  config_listener);
	}

	g_signal_emit (config_listener, signals[EXCHANGE_ACCOUNT_CREATED], 0,
		       exchange_account);
/*  	add_sources (exchange_account); */

	exchange_config_listener_get_offline_status (config_listener, &is_offline);

	if (is_offline == OFFLINE_MODE) {
		remove_selected_non_offline_esources (exchange_account, CONF_KEY_CAL);
		remove_selected_non_offline_esources (exchange_account, CONF_KEY_TASKS);
		return;
	}
	exchange_account_set_online (exchange_account);
	exchange_config_listener_authenticate (config_listener, exchange_account);
	exchange_account_set_online (exchange_account);
}

struct account_update_data {
	EAccountList *account_list;
	EAccount *account;
};

static void
configured_account_destroyed (gpointer user_data, GObject *where_account_was)
{
	struct account_update_data *aud = user_data;

	if (!EXCHANGE_CONFIG_LISTENER (aud->account_list)->priv->configured_account)
		account_added (aud->account_list, aud->account);

	g_object_unref (aud->account_list);
	g_object_unref (aud->account);
	g_free (aud);
}

static gboolean
requires_relogin (char *current_url, char *new_url)
{
	E2kUri *current_uri, *new_uri;
	const char *current_param_val, *new_param_val;
	const char *params [] = { "owa_url", "ad_server", "use_ssl" };
	const int n_params = G_N_ELEMENTS (params);
	int i;
	gboolean relogin = FALSE;

	current_uri = e2k_uri_new (current_url);
	new_uri = e2k_uri_new (new_url);

	if (strcmp (current_uri->user, new_uri->user) ||
	    strcmp (current_uri->host, new_uri->host)) {
		relogin = TRUE;
		goto end;
	}
	
	if (current_uri->authmech || new_uri->authmech) {
		if (current_uri->authmech && new_uri->authmech) {
	    		if (strcmp (current_uri->authmech, new_uri->authmech)) {
				/* Auth mechanism has changed */
				relogin = TRUE;
				goto end;
			}
		} 
		else {
			/* Auth mechanism is set for the first time */
			relogin = TRUE;
			goto end;
		}
	}

	for (i=0; i<n_params; i++) { 
		current_param_val = e2k_uri_get_param (current_uri, params[i]);
		new_param_val = e2k_uri_get_param (new_uri, params[i]); 
	
		if (current_param_val && new_param_val) {
			/* both the urls have params to be compared */
			if (strcmp (current_param_val, new_param_val)) {
				relogin = TRUE;
				break;
			}
		}
		else if (current_param_val || new_param_val){
			/* check for added or deleted parameter */
			relogin = TRUE;
			break;
		}
	}
end:
	e2k_uri_free (new_uri);
	e2k_uri_free (current_uri);
	return relogin;
}

static void
account_changed (EAccountList *account_list, EAccount *account)
{
	ExchangeConfigListener *config_listener =
		EXCHANGE_CONFIG_LISTENER (account_list);
	ExchangeConfigListenerPrivate *priv = config_listener->priv;

	if (account != config_listener->priv->configured_account) {
		if (!is_active_exchange_account (account))
			return;

		/* The user has converted an existing non-Exchange
		 * account to an Exchange account, so treat it like an
		 * add.
		 */
		account_added (account_list, account);
		return;
	} else if (!is_active_exchange_account (account)) {
		/* The user has disabled the Exchange account or
		 * converted it to non-Exchange, so treat it like a
		 * remove.
		 */
		account_removed (account_list, account);
		return;
	}

	/* FIXME: The order of the parameters in the Camel URL string is not in 
	 * order for the two given strings. So, we will not be able to use
	 * plain string comparison. Instead compare the parameters one by one.
	 */
	if (exchange_camel_urls_is_equal (config_listener->priv->configured_uri, 
					  account->source->url) &&
	    !strcmp (config_listener->priv->configured_name, account->name)) {
		/* The user changed something we don't care about. */
		return;
	}

	/* OK, so he modified the active account in a way we care
	 * about. If the user hasn't connected yet, we're still ok.
	 */
	if (!exchange_account_get_context (config_listener->priv->exchange_account)) {
		/* Good. Remove the current account, and wait for it
		 * to actually go away (which may not happen immediately
		 * since there may be a function higher up on the stack
		 * still holding a ref on it). Then create the new one.
		 * (We have to wait for it to go away because the new
		 * storage probably still has the same name as the old
		 * one, so trying to create it before the old one is
		 * removed would fail.)
		 */
		struct account_update_data *aud;

		aud = g_new (struct account_update_data, 1);
		aud->account = g_object_ref (account);
		aud->account_list = g_object_ref (account_list);
		g_object_weak_ref (G_OBJECT (config_listener->priv->exchange_account), configured_account_destroyed, aud);

		account_removed (account_list, account);
		return;
	}

	/* If account name has changed, or the url value has changed, which 
	 * could be due to change in hostname or some parameter value, 
	 * remove old e-sources 
	 */
	if (requires_relogin (config_listener->priv->configured_uri, 
			      account->source->url)) {
		remove_account_esources (priv->exchange_account);
		exchange_account_forget_password (priv->exchange_account);
	} else if (strcmp (config_listener->priv->configured_name, account->name)) {
/* 		remove_account_esources (priv->exchange_account); */
		exchange_config_listener_modify_esource_group_name (config_listener,
								    config_listener->priv->configured_name, 
								    account->name);
		g_free (config_listener->priv->configured_name);
		config_listener->priv->configured_name = g_strdup (account->name);
		return;
	} else {
		/* FIXME: Do ESources need to be modified? */
		return;
	}
	
	/* Nope. Let the user know we're ignoring him. */
	e_error_run (NULL, "org-gnome-exchange-operations:apply-restart", NULL);

	/* But note the new URI so if he changes something else, we
	 * only warn again if he changes again.
	 */
	g_free (config_listener->priv->configured_uri);
	config_listener->priv->configured_uri = g_strdup (account->source->url);
}

static void
account_removed (EAccountList *account_list, EAccount *account)
{
	ExchangeConfigListener *config_listener =
		EXCHANGE_CONFIG_LISTENER (account_list);
	ExchangeConfigListenerPrivate *priv = config_listener->priv;

	if (account != priv->configured_account)
		return;

	/* Remove all ESources */
	remove_account_esources (priv->exchange_account);

	exchange_account_forget_password (priv->exchange_account);

	if (!exchange_account_get_context (priv->exchange_account)) {
		/* The account isn't connected yet, so we can destroy
		 * it without problems.
		 */
		g_signal_emit (config_listener,
			       signals[EXCHANGE_ACCOUNT_REMOVED], 0,
			       priv->exchange_account);

		g_object_unref (priv->exchange_account);
		priv->exchange_account = NULL;

		priv->configured_account = NULL;
		g_free (priv->configured_uri);
		priv->configured_uri = NULL;
		g_free (priv->configured_name);
		priv->configured_name = NULL;
	} 
}

static gboolean
idle_construct (gpointer data)
{
	ExchangeConfigListener *config_listener = data;

	config_listener->priv->idle_id = 0;
	e_account_list_construct (E_ACCOUNT_LIST (config_listener),
				  config_listener->priv->gconf);
	return FALSE;
}

ExchangeConfigListenerStatus
exchange_config_listener_get_offline_status (ExchangeConfigListener *excl,
					     gint *mode)
{
	ExchangeConfigListenerPrivate *priv;
	GConfValue *value;
	ExchangeConfigListenerStatus status = CONFIG_LISTENER_STATUS_OK;
	gboolean offline = FALSE;

	g_return_val_if_fail (excl != NULL, CONFIG_LISTENER_STATUS_NOT_FOUND);

	priv = excl->priv;
	value = gconf_client_get (priv->gconf,
					"/apps/evolution/shell/start_offline", NULL);
	if (value)
		offline = gconf_value_get_bool (value);

	if (offline)
		*mode = OFFLINE_MODE;
	else
		*mode = ONLINE_MODE;

	return status;

}	

/**
 * exchange_config_listener_new:
 *
 * This creates and returns a new #ExchangeConfigListener, which
 * monitors GConf and creates and (theoretically) destroys accounts
 * accordingly. It will emit an %account_created signal when a new
 * account is created (or shortly after the listener itself is created
 * if an account already exists).
 *
 * Due to various constraints, the user is currently limited to a
 * single account, and it is not possible to destroy an existing
 * account. Thus, the %account_created signal will never be emitted
 * more than once currently.
 *
 * Return value: the new config listener.
 **/
ExchangeConfigListener *
exchange_config_listener_new (void)
{
	ExchangeConfigListener *config_listener;

	config_listener = g_object_new (EXCHANGE_TYPE_CONFIG_LISTENER, NULL);
	config_listener->priv->gconf = gconf_client_get_default ();

	config_listener->priv->idle_id =
		g_idle_add (idle_construct, config_listener);

	return config_listener;
}

GSList *
exchange_config_listener_get_accounts (ExchangeConfigListener *config_listener)
{
	g_return_val_if_fail (EXCHANGE_IS_CONFIG_LISTENER (config_listener), NULL);

	if (config_listener->priv->exchange_account)
		return g_slist_append (NULL, config_listener->priv->exchange_account);
	else
		return NULL;
}

/**
 * exchange_config_listener_modify_esource_group_name
 *
 * @excl: Handle for Exchange Config Listener 
 * @old_name: Old name of the ESourceGroup
 * @new_name: New name of the ESourceGroup
 *
 * This function modifies the old source group name to the specified new
 * source group name
 **/ 
void 
exchange_config_listener_modify_esource_group_name (ExchangeConfigListener *excl,
							 const char *old_name, 
							 const char *new_name)
{
	GConfClient *client;
	ESourceGroup *group;
	GSList *groups;
	ESourceList *c_source_list = NULL, *t_source_list = NULL, 
		*a_source_list = NULL;

	client = excl->priv->gconf;

	c_source_list = e_source_list_new_for_gconf ( client, CONF_KEY_CAL);
	t_source_list = e_source_list_new_for_gconf ( client, CONF_KEY_TASKS);
	a_source_list = e_source_list_new_for_gconf ( client, CONF_KEY_CONTACTS);

	groups = e_source_list_peek_groups (c_source_list);

	for ( ; groups != NULL; groups = g_slist_next (groups)) {
		group = E_SOURCE_GROUP (groups->data);
		if (!strcmp (e_source_group_peek_name (group), old_name)) {
			e_source_group_set_name (group, new_name);
			break;
		}
	}

	groups = e_source_list_peek_groups (t_source_list);

	for ( ; groups != NULL; groups = g_slist_next (groups)) {
		group = E_SOURCE_GROUP (groups->data);
		if (!strcmp (e_source_group_peek_name (group), old_name)) {
			e_source_group_set_name (group, new_name);
			break;
		}
	}

	groups = e_source_list_peek_groups (a_source_list);

	for ( ; groups != NULL; groups = g_slist_next (groups)) {
		group = E_SOURCE_GROUP (groups->data);
		if (!strcmp (e_source_group_peek_name (group), old_name)) {
			e_source_group_set_name (group, new_name);
			break;
		}
	}

	e_source_list_sync (c_source_list, NULL);
	e_source_list_sync (t_source_list, NULL);
	e_source_list_sync (a_source_list, NULL);

	g_object_unref (c_source_list);
	g_object_unref (t_source_list);
	g_object_unref (a_source_list);
}

/**
 * exchange_add_autocompletion_folders:
 * 
 * @gc_client: GConfClient handle
 * @account: ExchangeAccount handle
 *
 * This function adds the GAL of the Exchange account to the autocompletion list
 * while configuring a new Exchange account
 *
 **/ 
static void
exchange_add_autocompletion_folders (GConfClient *gc_client, ExchangeAccount *account)
{
	ESourceList *sl=NULL;
	ESourceGroup *group;
	ESource *source;
	GSList *groups, *sources;
	gboolean found_group=FALSE;

	sl = e_source_list_new_for_gconf (gc_client, CONF_KEY_CONTACTS);
	groups = e_source_list_peek_groups (sl);

	for ( ; groups != NULL && !found_group; groups = g_slist_next (groups)) {
		group = E_SOURCE_GROUP (groups->data);		
		if (strcmp (e_source_group_peek_name (group), account->account_name) == 0
                    &&
		    strcmp (e_source_group_peek_base_uri (group), EXCHANGE_URI_PREFIX) == 0) {
			
			sources = e_source_group_peek_sources (group);
			
			for( ; sources != NULL; sources = g_slist_next (sources)) {
				source = E_SOURCE (sources->data);
				if (g_str_has_prefix (e_source_peek_absolute_uri (source),
						      "gal://")) {
					/* Set autocompletion on GAL alone by default */
					e_source_set_property (source, "completion", "true");
					break;
				}
			}
			found_group = TRUE;
		}
	}

	g_object_unref (sl);
}


/**
 * exchange_camel_urls_is_equal 
 * 
 * @url1: CAMEL URL string 1
 * @url2: CAMEL URL string 2
 *
 * This function checks if the parameters present in two given CAMEL URLS are
 * identical and returns the result.
 *
 * Return Value: Boolean result of the comparision.
 *
 **/ 
static gboolean
exchange_camel_urls_is_equal (const gchar *url1, const gchar *url2)
{
	CamelURL *curl1, *curl2;
	gchar *param1, *param2;
	const char *params[] = {
		"auth",
		"owa_url",
		"owa_path",
		"mailbox",
		"ad_server",
	};
	const int n_params = 5;
	int i;
	
	curl1 = camel_url_new (url1, NULL);
	curl2 = camel_url_new (url2, NULL);

	for (i = 0; i < n_params; ++i) {
		param1 = (gchar*) camel_url_get_param (curl1, params[i]);
		param2 = (gchar*) camel_url_get_param (curl2, params[i]);
		if ((param1 && !param2) || (!param1 && param2) || /* Missing */
		    (param1 && param2 && strcmp (param1, param2))) { /* Differing */
			g_free (param1);
			g_free (param2);
			g_free (curl1);
			g_free (curl2);
			return FALSE;
		}		
		g_free (param1);
		g_free (param2);
	}
	g_free (curl1);
	g_free (curl2);
	return TRUE;
}


/**
 * remove_selected_non_offline_esources
 * 
 * @account: Handle for Exchange Account
 * @gconf_key: GConf key of the calendar or tasks
 *
 * This function removes the non-offline calendars and taks list from the 
 * selection list
 **/ 
static void 
remove_selected_non_offline_esources (ExchangeAccount *account, const char *gconf_key)
{
	ESourceGroup *group;
	ESource *source = NULL;
	GSList *groups;
	GSList *sources;
	GSList *ids, *node_to_be_deleted;
	gboolean found_group;
	const char *source_uid;
	GConfClient *client;
	ESourceList *source_list = NULL;
	const char *offline_mode=NULL;
	char *selected_gconf_key;


	if (gconf_key && !strcmp (gconf_key, CONF_KEY_CAL)) {
		selected_gconf_key = g_strdup (CONF_KEY_SELECTED_CAL_SOURCES);
	} else if (gconf_key && !strcmp (gconf_key, CONF_KEY_TASKS)) {
		selected_gconf_key = g_strdup (CONF_KEY_SELECTED_TASKS_SOURCES);
	}
	else {
		return;
	}

	client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf ( client, gconf_key);

	groups = e_source_list_peek_groups (source_list);
	found_group = FALSE;

	for ( ; groups != NULL && !found_group; groups = g_slist_next (groups)) {
		group = E_SOURCE_GROUP (groups->data);

		if (strcmp (e_source_group_peek_name (group), account->account_name) == 0
                    &&
                    strcmp (e_source_group_peek_base_uri (group), EXCHANGE_URI_PREFIX) == 0) {
			sources = e_source_group_peek_sources (group);

			for( ; sources != NULL; sources = g_slist_next (sources)) {
				source = E_SOURCE (sources->data);
				source_uid = e_source_peek_uid (source);

				/* Remove from the selected folders */
				ids = gconf_client_get_list (client, 
							     selected_gconf_key, 
							     GCONF_VALUE_STRING, NULL);
				if (ids) {
					offline_mode = e_source_get_property (source, "offline_sync");
					if (!offline_mode || 
					    (offline_mode && strcmp (offline_mode, "1"))) {
						while ((node_to_be_deleted = 
							g_slist_find_custom (ids, 
									     source_uid, 
									     (GCompareFunc) strcmp))) {
							g_free (node_to_be_deleted->data);
							ids = g_slist_delete_link (ids, 
										   node_to_be_deleted);
							gconf_client_set_list (client, 
									       selected_gconf_key,
									       GCONF_VALUE_STRING, ids, NULL);
						}
					}
					g_slist_foreach (ids, (GFunc) g_free, NULL);
					g_slist_free (ids);
				}
			}
			found_group = TRUE;
			e_source_list_sync (source_list, NULL);
		}
	}
	
	g_free (selected_gconf_key);
	g_object_unref (source_list);
	g_object_unref (client);
}
