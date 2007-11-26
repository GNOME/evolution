/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: 
 *  	Srinivasa Ragavan <sragavan@novell.com>
 * 	Suman Manjunath <msuman@novell.com>
 *  Copyright (C) 2007 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exchange-account-listener.h>
#include <string.h>
#include <camel/camel-i18n.h>
#include <libedataserverui/e-passwords.h>
#include "e-util/e-error.h"
#include <libedataserver/e-account.h>
#include <libecal/e-cal.h>

#include <libmapi/libmapi.h>


/* FIXME: The mapi should not be needed in the include statement.
LIMBAPI_CFLAGS or something is going wrong */

#include <mapi/exchange-mapi-folder.h>
#include <mapi/exchange-mapi-connection.h>

/*stores some info about all currently existing mapi accounts 
  list of ExchangeAccountInfo structures */

static 	GList *mapi_accounts = NULL;
static	GSList *folders_list = NULL;
struct _ExchangeAccountListenerPrivate {
	GConfClient *gconf_client;
	/* we get notification about mail account changes from this object */
	EAccountList *account_list;                  
};

struct _ExchangeAccountInfo {
	char *uid;
	char *name;
	char *source_url;
};

typedef struct _ExchangeAccountInfo ExchangeAccountInfo;

#define MAPI_URI_PREFIX   "mapi://" 
#define MAPI_PREFIX_LENGTH 7

#define PARENT_TYPE G_TYPE_OBJECT

static GObjectClass *parent_class = NULL;

static void dispose (GObject *object);
static void finalize (GObject *object);

void
exchange_account_listener_get_folder_list (void)
{
	if (folders_list)
		return;

	folders_list = exchange_mapi_peek_folder_list ();
}

static void 
exchange_account_listener_class_init (ExchangeAccountListenerClass *class)
{
	GObjectClass *object_class;
	
	parent_class =  g_type_class_ref (PARENT_TYPE);
	object_class = G_OBJECT_CLASS (class);
	
	/* virtual method override */
	object_class->dispose = dispose;
	object_class->finalize = finalize;
}

static void 
exchange_account_listener_init (ExchangeAccountListener *config_listener,  ExchangeAccountListenerClass *class)
{
	config_listener->priv = g_new0 (ExchangeAccountListenerPrivate, 1);	
}

static void 
dispose (GObject *object)
{
	ExchangeAccountListener *config_listener = EXCHANGE_ACCOUNT_LISTENER (object);
	
	g_object_unref (config_listener->priv->gconf_client);
	g_object_unref (config_listener->priv->account_list);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void 
finalize (GObject *object)
{
	ExchangeAccountListener *config_listener = EXCHANGE_ACCOUNT_LISTENER (object);
	GList *list;
	ExchangeAccountInfo *info;

	if (config_listener->priv) {
		g_free (config_listener->priv);
	}

	for (list = g_list_first (mapi_accounts); list ; list = g_list_next (list)) {
	       
		info = (ExchangeAccountInfo *) (list->data);

		if (info) {
			g_free (info->uid);
			g_free (info->name);
			g_free (info->source_url);
			g_free (info);
		}
	}
	
	g_list_free (mapi_accounts);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*determines whehter the passed in account is exchange or not by looking at source url */

static gboolean
is_mapi_account (EAccount *account)
{
	if (account->source->url != NULL) {
		return (strncmp (account->source->url,  MAPI_URI_PREFIX, MAPI_PREFIX_LENGTH ) == 0);
	} else {
		return FALSE;
	}
}

/* looks up for an existing exchange account info in the mapi_accounts list based on uid */

static ExchangeAccountInfo* 
lookup_account_info (const char *key)
{
	GList *list;
        ExchangeAccountInfo *info ;
	int found = 0;
                                                                      
        if (!key)
                return NULL;

	info = NULL;

        for (list = g_list_first (mapi_accounts);  list;  list = g_list_next (list)) {
                info = (ExchangeAccountInfo *) (list->data);
                found = (strcmp (info->uid, key) == 0);
		if (found)
			break;
	}

	if (found)
		return info;

	return NULL;
}

#define CALENDAR_SOURCES 	"/apps/evolution/calendar/sources"
#define TASK_SOURCES 		"/apps/evolution/tasks/sources"
#define JOURNAL_SOURCES 	"/apps/evolution/memos/sources"
#define SELECTED_CALENDARS 	"/apps/evolution/calendar/display/selected_calendars"
#define SELECTED_TASKS 		"/apps/evolution/calendar/tasks/selected_tasks"
#define SELECTED_JOURNALS 	"/apps/evolution/calendar/memos/selected_memos"

static void
add_cal_esource (EAccount *account, GSList *folders, ExchangeMAPIFolderType folder_type, CamelURL *url)
{
	ESourceList *source_list = NULL;
	ESourceGroup *group = NULL;
	const gchar *conf_key = NULL, *source_selection_key = NULL, *primary_source_name = NULL;
 	GSList *temp_list = NULL;
	GConfClient* client;
	GSList *ids, *temp ;
	gchar *relative_uri;

	if (folder_type ==  MAPI_FOLDER_TYPE_APPOINTMENT) { 
		conf_key = CALENDAR_SOURCES;
		source_selection_key = SELECTED_CALENDARS;
//		primary_source_name = "Calendar";
	} else if (folder_type == MAPI_FOLDER_TYPE_TASK) { 
		conf_key = TASK_SOURCES;
		source_selection_key = SELECTED_TASKS;
//		primary_source_name = "Tasks";
	} else if (folder_type == MAPI_FOLDER_TYPE_MEMO) {
		conf_key = JOURNAL_SOURCES;
		source_selection_key = SELECTED_JOURNALS;
//		primary_source_name = "Notes";
	} else {
		g_warning ("%s(%d): %s: Unknown ExchangeMAPIFolderType\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		return;
	} 

	client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (client, conf_key);
	group = e_source_group_new (account->name, MAPI_URI_PREFIX);
	e_source_group_set_property (group, "create_source", "yes");
	e_source_group_set_property (group, "profile", camel_url_get_param (url, "profile"));
	e_source_group_set_property (group, "username", url->user);
	e_source_group_set_property (group, "host", url->host);
	e_source_group_set_property (group, "domain", camel_url_get_param (url, "domain"));
	e_source_group_set_property (group, "use_ssl", camel_url_get_param (url, "use_ssl"));

	for (temp_list = folders; temp_list != NULL; temp_list = g_slist_next (temp_list)) {
 		ExchangeMAPIFolder *folder = temp_list->data;
		ESource *source = NULL;
		gchar *tmp = NULL;

		if (folder->container_class != folder_type)
			continue;

		tmp = g_strdup_printf ("%016llx", folder->folder_id);
		relative_uri = g_strdup_printf ("%s@%s/%s/", url->user, url->host, tmp);
		source = e_source_new (folder->folder_name, relative_uri);
		e_source_set_property (source, "auth", "1");
		e_source_set_property (source, "auth-domain", "MAPI");
		e_source_set_property (source, "use_ssl", camel_url_get_param (url, "use_ssl"));
		e_source_set_property (source, "username", url->user);
		e_source_set_property (source, "host", url->host);
		e_source_set_property (source, "offline_sync", 
					       camel_url_get_param (url, "offline_sync") ? "1" : "0");

		e_source_set_property (source, "profile", camel_url_get_param (url, "profile"));
		e_source_set_property (source, "domain", camel_url_get_param (url, "domain"));
		e_source_set_property (source, "folder-id", tmp);
		g_free (tmp);
		/* FIXME: The primary folders cannot be deleted */
#if 0
		if (strcmp (folder->folder_name, primary_source_name) == 0) 
			e_source_set_property (source, "delete", "no");
#endif
		if (folder->parent_folder_id) {
			tmp = g_strdup_printf ("%016llx", folder->parent_folder_id);
			e_source_set_property (source, "parent-fid", tmp);
			g_free (tmp);
		}

		e_source_group_add_source (group, source, -1);

		if (source_selection_key) {
			ids = gconf_client_get_list (client, source_selection_key , GCONF_VALUE_STRING, NULL);
			ids = g_slist_append (ids, g_strdup (e_source_peek_uid (source)));
			gconf_client_set_list (client,  source_selection_key, GCONF_VALUE_STRING, ids, NULL);

			for (temp = ids; temp != NULL; temp = g_slist_next (temp))
				g_free (temp->data);

			g_slist_free (ids);
		}

		g_object_unref (source);
		g_free (relative_uri);
	}

	if (!e_source_list_add_group (source_list, group, -1))
		return;
	if (!e_source_list_sync (source_list, NULL))
		return;

	g_object_unref (group);
	g_object_unref (source_list);
	g_object_unref (client);
}

static void 
remove_cal_esource (EAccount *existing_account_info, ExchangeMAPIFolderType folder_type, const gchar* relative_uri)
{
	ESourceList *list;
	const gchar *conf_key = NULL, *source_selection_key = NULL;
        GSList *groups;
	gboolean found_group;
	GConfClient* client;
	GSList *ids;
	GSList *node_tobe_deleted;

	if (folder_type ==  MAPI_FOLDER_TYPE_APPOINTMENT) { 
		conf_key = CALENDAR_SOURCES;
		source_selection_key = SELECTED_CALENDARS;
	} else if (folder_type == MAPI_FOLDER_TYPE_TASK) { 
		conf_key = TASK_SOURCES;
		source_selection_key = SELECTED_TASKS;
	} else if (folder_type == MAPI_FOLDER_TYPE_MEMO) {
		conf_key = JOURNAL_SOURCES;
		source_selection_key = SELECTED_JOURNALS;
	} else {
		g_warning ("%s(%d): %s: Unknown ExchangeMAPIFolderType\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		return;
	} 

        client = gconf_client_get_default();
        list = e_source_list_new_for_gconf (client, conf_key);
	groups = e_source_list_peek_groups (list); 

	found_group = FALSE;

	for ( ; groups != NULL && !found_group; groups = g_slist_next (groups)) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);

		if (strcmp (e_source_group_peek_name (group), existing_account_info->name) == 0 && 
		   strcmp (e_source_group_peek_base_uri (group), MAPI_URI_PREFIX) == 0) {
			GSList *sources = e_source_group_peek_sources (group);
			
			for( ; sources != NULL; sources = g_slist_next (sources)) {
				ESource *source = E_SOURCE (sources->data);
				const gchar *source_relative_uri;

				source_relative_uri = e_source_peek_relative_uri (source);
				if (source_relative_uri == NULL)
					continue;

				if (g_str_has_prefix (source_relative_uri, relative_uri) && source_selection_key) {
					ids = gconf_client_get_list (client, source_selection_key , 
								     GCONF_VALUE_STRING, NULL);
					node_tobe_deleted = g_slist_find_custom (ids, e_source_peek_uid (source), (GCompareFunc) strcmp);
					if (node_tobe_deleted) {
						g_free (node_tobe_deleted->data);
						ids = g_slist_delete_link (ids, node_tobe_deleted);
					}
					gconf_client_set_list (client,  source_selection_key, 
							       GCONF_VALUE_STRING, ids, NULL);
				}
			}
			e_source_list_remove_group (list, group);
			e_source_list_sync (list, NULL);	
			found_group = TRUE;
			break;
		}
	}

	g_object_unref (list);
	g_object_unref (client);		
}

static void 
modify_cal_esource (EAccount *account, ExchangeMAPIFolderType folder_type, ExchangeAccountInfo *existing_account_info, const gchar *profile)
{
	ESourceList *list;
        GSList *groups;
	gboolean found_group;
      	GConfClient* client;
	const gchar *conf_key = NULL;
 
	if (folder_type ==  MAPI_FOLDER_TYPE_APPOINTMENT) { 
		conf_key = CALENDAR_SOURCES;
	} else if (folder_type == MAPI_FOLDER_TYPE_TASK) { 
		conf_key = TASK_SOURCES;
	} else if (folder_type == MAPI_FOLDER_TYPE_MEMO) {
		conf_key = JOURNAL_SOURCES;
	} else {
		g_warning ("%s(%d): %s: Unknown ExchangeMAPIFolderType\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		return;
	}

        client = gconf_client_get_default();
        list = e_source_list_new_for_gconf (client, conf_key);
	groups = e_source_list_peek_groups (list); 

	found_group = FALSE;

	for ( ; groups != NULL && !found_group; groups = g_slist_next (groups)) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		gchar *group_profile = e_source_group_get_property (group, "profile");

		if (strcmp (e_source_group_peek_name (group), existing_account_info->name) == 0 && 
		    strcmp (e_source_group_peek_base_uri (group), MAPI_URI_PREFIX) == 0 &&
		    group_profile && !strcmp (group_profile, profile)) {
			found_group = TRUE;
			e_source_group_set_name (group, account->name);
			e_source_list_sync (list, NULL);
		}
		if (group_profile)
			g_free (group_profile);
	}

	g_object_unref (list);
	g_object_unref (client);
}

/* add sources for calendar and tasks if the account added is exchange account
   adds the new account info to  mapi_accounts list */

static void 
add_calendar_sources (EAccount *account, GSList *folders, ExchangeAccountInfo *info)
{
	CamelURL *url;

	url = camel_url_new (info->source_url, NULL);

	add_cal_esource (account, folders, MAPI_FOLDER_TYPE_APPOINTMENT, url);
	add_cal_esource (account, folders, MAPI_FOLDER_TYPE_TASK, url);
	add_cal_esource (account, folders, MAPI_FOLDER_TYPE_MEMO, url);

	camel_url_free (url);
}

/* removes calendar and tasks sources if the account removed is exchange account 
   removes the the account info from mapi_account list */

static void 
remove_calendar_sources (EAccount *account, ExchangeAccountInfo *info)
{
	CamelURL *url;
	gchar *relative_uri;

	url = camel_url_new (info->source_url, NULL);
	relative_uri =  g_strdup_printf ("%s@%s/", url->user, url->host);

	remove_cal_esource (account, MAPI_FOLDER_TYPE_APPOINTMENT, relative_uri);
	remove_cal_esource (account, MAPI_FOLDER_TYPE_TASK, relative_uri);
	remove_cal_esource (account, MAPI_FOLDER_TYPE_MEMO, relative_uri);

	camel_url_free (url);
	g_free (relative_uri);
}

/* This is called only when the source-group name is to be changed */
static void 
modify_calendar_sources (EAccount *account, ExchangeAccountInfo *existing_account_info, CamelURL *new_url)
{
	const gchar *profile = camel_url_get_param (new_url, "profile");

	modify_cal_esource (account, MAPI_FOLDER_TYPE_APPOINTMENT, existing_account_info, profile);
	modify_cal_esource (account, MAPI_FOLDER_TYPE_TASK, existing_account_info, profile);
	modify_cal_esource (account, MAPI_FOLDER_TYPE_MEMO, existing_account_info, profile);
}

static gboolean
add_addressbook_sources (EAccount *account, GSList *folders)
{
	CamelURL *url;
	ESourceList *list;
        ESourceGroup *group;
        ESource *source;
       	char *base_uri;
	GSList *books_list, *temp_list;
	GConfClient* client;
	const char* use_ssl;
	gboolean is_frequent_contacts = FALSE, is_writable = FALSE;

	printf("URL %s\n", account->source->url);
        url = camel_url_new (account->source->url, NULL);
	if (url == NULL) {
		return FALSE;
	}

	base_uri =  g_strdup_printf ("mapi://%s@%s", url->user, url->host);
	client = gconf_client_get_default ();
	list = e_source_list_new_for_gconf (client, "/apps/evolution/addressbook/sources" );
	group = e_source_group_new (account->name, base_uri);
	e_source_group_set_property (group, "user", url->user);
	e_source_group_set_property (group, "host", url->host);
	e_source_group_set_property (group, "profile", camel_url_get_param (url, "profile"));
	e_source_group_set_property (group, "domain", camel_url_get_param (url, "domain"));

	for (temp_list = folders; temp_list != NULL; temp_list = g_slist_next (temp_list)) {
 		ExchangeMAPIFolder *folder = temp_list->data;
		char *tmp = NULL;
		if (folder->container_class != MAPI_FOLDER_TYPE_CONTACT)
			continue;

		source = e_source_new (folder->folder_name, g_strconcat (";",folder->folder_name, NULL));
		e_source_set_property (source, "auth", "plain/password");
		e_source_set_property (source, "auth-domain", "MAPI");
		e_source_set_property(source, "user", url->user);
		e_source_set_property(source, "host", url->host);
		e_source_set_property(source, "profile", camel_url_get_param (url, "profile"));
		e_source_set_property(source, "domain", camel_url_get_param (url, "domain"));
		tmp = g_strdup_printf ("%016llx", folder->folder_id);
		e_source_set_property(source, "folder-id", tmp);
		g_free (tmp);
		e_source_set_property (source, "offline_sync", 
					       camel_url_get_param (url, "offline_sync") ? "1" : "0");
		e_source_set_property (source, "completion", "true");
		e_source_group_add_source (group, source, -1);
		g_object_unref (source);
	}

	//Add GAL
	{
		char *uri;
		uri = g_strdup_printf("galldap://%s@%s;Global Address List", url->user, url->host);
		source = e_source_new_with_absolute_uri ("Global Address List", uri);
//		source = e_source_new ("Global Address List", g_strconcat (";","Global Address List" , NULL));
		e_source_set_property (source, "auth", "plain/password");
		e_source_set_property (source, "auth-domain", "GALLDAP");
		e_source_set_property(source, "user", url->user);
		e_source_set_property(source, "host", camel_url_get_param (url, "ad_server"));
		e_source_set_property(source, "view-limit", camel_url_get_param (url, "ad_limit"));		
		e_source_set_property(source, "profile", camel_url_get_param (url, "profile"));
		e_source_set_property(source, "domain", camel_url_get_param (url, "domain"));
//		e_source_set_property (source, "offline_sync", 
//					       camel_url_get_param (url, "offline_sync") ? "1" : "0");
		e_source_set_property(source, "offline_sync", "1");
		e_source_set_property (source, "completion", "true");
		e_source_group_add_source (group, source, -1);
		g_object_unref (source);		
	}
	e_source_list_add_group (list, group, -1);
	e_source_list_sync (list, NULL);
	g_object_unref (group);
	g_object_unref (list);
	g_object_unref (client);
	g_free (base_uri);

	return TRUE;
}

static void 
remove_addressbook_sources (ExchangeAccountInfo *existing_account_info)
{
	ESourceList *list;
        ESourceGroup *group;
	GSList *groups;
       	gboolean found_group;
	CamelURL *url;
	char *base_uri;
	GConfClient *client;

	url = camel_url_new (existing_account_info->source_url, NULL);
	if (url == NULL) {
		return;
	}

	base_uri =  g_strdup_printf ("mapi://%s@%s", url->user,  url->host);
	client = gconf_client_get_default ();
	list = e_source_list_new_for_gconf (client, "/apps/evolution/addressbook/sources" );
	groups = e_source_list_peek_groups (list); 

	found_group = FALSE;

	for ( ; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		if ( strcmp ( e_source_group_peek_base_uri (group), base_uri) == 0 && strcmp (e_source_group_peek_name (group), existing_account_info->name) == 0) {

			e_source_list_remove_group (list, group);
			e_source_list_sync (list, NULL);
			found_group = TRUE;
		}
	}

	g_object_unref (list);
	g_object_unref (client);
	g_free (base_uri);
	camel_url_free (url);
}

static void 
modify_addressbook_sources (EAccount *account, ExchangeAccountInfo *existing_account_info)
{
	CamelURL *url;
	ESourceList *list;
        ESourceGroup *group;
	GSList *groups;
       	gboolean found_group;
	char *old_base_uri;
	GConfClient *client;

	url = camel_url_new (existing_account_info->source_url, NULL);
	if (url == NULL) 
		return;

	old_base_uri =  g_strdup_printf ("mapi://%s@%s", url->user, url->host);
	camel_url_free (url);

	client = gconf_client_get_default ();
	list = e_source_list_new_for_gconf (client, "/apps/evolution/addressbook/sources");
	groups = e_source_list_peek_groups (list); 

	group = NULL;
	found_group = FALSE;
	for ( ; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		if (strcmp (e_source_group_peek_base_uri(group), old_base_uri) == 0 && strcmp (e_source_group_peek_name (group), existing_account_info->name) == 0) {
			found_group = TRUE;
			e_source_group_set_name (group, account->name);
			e_source_list_sync (list, NULL);
		}
	}

	g_free (old_base_uri);
	g_object_unref (list);
	g_object_unref (client);
}

static void
account_added (EAccountList *account_listener, EAccount *account)
{
	ExchangeAccountInfo *info;
	EAccount *parent;
	gboolean status;
	CamelURL *parent_url;

	if (!is_mapi_account (account))
		return;
	
	info = g_new0 (ExchangeAccountInfo, 1);
	info->uid = g_strdup (account->uid);
	info->name = g_strdup (account->name);
	info->source_url = g_strdup (account->source->url);
	printf("account happens\n");

	/* Fetch the folders into a global list for future use.*/
	exchange_account_listener_get_folder_list ();

	add_addressbook_sources (account, folders_list);
	add_calendar_sources (account, folders_list, info);
	/*FIXME: Maybe the folders_list above should be freed */

	mapi_accounts = g_list_append (mapi_accounts, info);
}

static void 
account_removed (EAccountList *account_listener, EAccount *account)
{
       	ExchangeAccountInfo *info;
	
	if (!is_mapi_account (account))
		return;
	
	info = lookup_account_info (account->uid);
	if (info == NULL) 
		return;

	exchange_account_listener_get_folder_list ();

	/* This foo needs a lotta work.. at present, using this to remove calendar sources */

	remove_addressbook_sources (info);
	remove_calendar_sources (account, info);

	mapi_accounts = g_list_remove (mapi_accounts, info);
	g_free (info->uid);
	g_free (info->name);
	g_free (info->source_url);
        g_free (info);
}

static void
account_changed (EAccountList *account_listener, EAccount *account)
{
	gboolean bis_mapi_account;
	CamelURL *old_url, *new_url;
	ExchangeAccountInfo *existing_account_info;
	const char *old_use_ssl, *new_use_ssl;
	const char *old_poa_address, *new_poa_address;
	
	bis_mapi_account = is_mapi_account (account);
	
	existing_account_info = lookup_account_info (account->uid);
       
	exchange_account_listener_get_folder_list ();

	if (existing_account_info == NULL && bis_mapi_account) {
		if (!account->enabled)
			return;

		/* some account of other type is changed to MAPI */
		account_added (account_listener, account);
	} else if (existing_account_info != NULL && !bis_mapi_account) {
		/*MAPI account is changed to some other type */
		remove_calendar_sources (account, existing_account_info);
		remove_addressbook_sources (existing_account_info);
		mapi_accounts = g_list_remove (mapi_accounts, existing_account_info);
		g_free (existing_account_info->uid);
		g_free (existing_account_info->name);
		g_free (existing_account_info->source_url);
		g_free (existing_account_info);		
	} else if (existing_account_info != NULL && bis_mapi_account) {		
		if (!account->enabled) {
			account_removed (account_listener, account);
			return;
		}

		/* some info of mapi account is changed . update the sources with new info if required */
		old_url = camel_url_new (existing_account_info->source_url, NULL);
		old_poa_address = old_url->host; 
		old_use_ssl = camel_url_get_param (old_url, "use_ssl");

		if (!old_poa_address || strlen (old_poa_address) == 0)
			return;

		new_url = camel_url_new (account->source->url, NULL);
		new_poa_address = new_url->host; 
		new_use_ssl = camel_url_get_param (new_url, "use_ssl");

		if (!new_poa_address || strlen (new_poa_address) == 0)
			return;

		if (  strcmp (old_poa_address, new_poa_address)
		   || strcmp (old_url->user, new_url->user)
//	           || (!old_use_ssl) 
//		   || strcmp (old_use_ssl, new_use_ssl)
		   ) {			
			account_removed (account_listener, account);
			account_added (account_listener, account);
		} else if (strcmp (existing_account_info->name, account->name)) {
			/* just the source group names have to be modified.. no sweat.. */
			modify_addressbook_sources (account, existing_account_info);
			modify_calendar_sources (account, existing_account_info, new_url);
		}
		
		g_free (existing_account_info->name);
		g_free (existing_account_info->source_url);
		existing_account_info->name = g_strdup (account->name);
		existing_account_info->source_url = g_strdup (account->source->url);
		camel_url_free (old_url);
		camel_url_free (new_url);
	}	
} 

static void
exchange_account_listener_construct (ExchangeAccountListener *config_listener)
{
	EIterator *iter;
	EAccount *account;
	ExchangeAccountInfo *info ;

       	config_listener->priv->account_list = e_account_list_new (config_listener->priv->gconf_client);

	for ( iter = e_list_get_iterator (E_LIST ( config_listener->priv->account_list) ) ; e_iterator_is_valid (iter); e_iterator_next (iter) ) {
		
		account = E_ACCOUNT (e_iterator_get (iter));

		if ( is_mapi_account (account) && account->enabled) {
			
		        info = g_new0 (ExchangeAccountInfo, 1);
			info->uid = g_strdup (account->uid);
			info->name = g_strdup (account->name);
			info->source_url = g_strdup (account->source->url);
			mapi_accounts = g_list_append (mapi_accounts, info);
			
		}
			
	}

	printf ("\n\alistener is constructed \n\a");

	g_signal_connect (config_listener->priv->account_list, "account_added", G_CALLBACK (account_added), NULL);
	g_signal_connect (config_listener->priv->account_list, "account_changed", G_CALLBACK (account_changed), NULL);
	g_signal_connect (config_listener->priv->account_list, "account_removed", G_CALLBACK (account_removed), NULL);    
}

GSList *
exchange_account_listener_peek_folder_list ()
{
	if (!folders_list)
		folders_list = exchange_mapi_peek_folder_list ();
	
	return folders_list;
}

GType
exchange_account_listener_get_type (void)
{
	static GType exchange_account_listener_type  = 0;

	if (!exchange_account_listener_type) {
		static GTypeInfo info = {
                        sizeof (ExchangeAccountListenerClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) exchange_account_listener_class_init,
                        NULL, NULL,
                        sizeof (ExchangeAccountListener),
                        0,
                        (GInstanceInitFunc) exchange_account_listener_init
                };
		exchange_account_listener_type = g_type_register_static (PARENT_TYPE, "ExchangeAccountListener", &info, 0);
	}

	return exchange_account_listener_type;
}

ExchangeAccountListener*
exchange_account_listener_new ()
{
	ExchangeAccountListener *config_listener;
       
	config_listener = g_object_new (EXCHANGE_TYPE_ACCOUNT_LISTENER, NULL);
	config_listener->priv->gconf_client = gconf_client_get_default();
	
	exchange_account_listener_construct (config_listener);

	return config_listener;
}
