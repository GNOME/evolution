/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Srinivasa Ragavan <sragavan@novell.com>
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

#include "exchange-account-listener.h"
#include <string.h>
#include  <camel/camel-i18n.h>
#include <libedataserverui/e-passwords.h>
#include "e-util/e-error.h"
#include <libedataserver/e-account.h>
#include <libecal/e-cal.h>

#include <libmapi/libmapi.h>
#include "exchange-mapi-folder.h"

/*stores some info about all currently existing mapi accounts 
  list of ExchangeAccountInfo structures */

static 	GList *mapi_accounts = NULL;
static  GList *mapi_folders = NULL;

struct _ExchangeAccountListenerPrivate {
	GConfClient *gconf_client;
	/* we get notification about mail account changes form this object */
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

	for ( list = g_list_first (mapi_accounts); list ; list = g_list_next (list) ) {
	       
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

#define CALENDAR_SOURCES "/apps/evolution/calendar/sources"
#define TASKS_SOURCES "/apps/evolution/tasks/sources"
#define NOTES_SOURCES "/apps/evolution/memos/sources"
#define SELECTED_CALENDARS "/apps/evolution/calendar/display/selected_calendars"
#define SELECTED_TASKS   "/apps/evolution/calendar/tasks/selected_tasks"
#define SELECTED_NOTES   "/apps/evolution/calendar/memos/selected_memos"

static void
add_esource (const char *conf_key, const char *group_name,  const char *source_name, CamelURL *url, const char* parent_id_name, gboolean can_create)
{
	ESourceList *source_list;
	ESourceGroup *group;
	ESource *source;
        GConfClient* client;
	GSList *ids, *temp ;
	char *source_selection_key;
	char *relative_uri;
	const char *soap_port;
	const char * use_ssl;
	const char *poa_address;
	const char *offline_sync;
	
	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;

	soap_port = camel_url_get_param (url, "soap_port");

 	if (!soap_port || strlen (soap_port) == 0)
		soap_port = "7191";

	use_ssl = camel_url_get_param (url, "use_ssl");


	offline_sync = camel_url_get_param (url, "offline_sync");
	
	client = gconf_client_get_default();	
	source_list = e_source_list_new_for_gconf (client, conf_key);
	group = e_source_group_new (group_name,  MAPI_URI_PREFIX);

	if (!e_source_list_add_group (source_list, group, -1))
		return;

	if (!can_create)
		e_source_group_set_property (group, "create_source", "no");

	relative_uri = g_strdup_printf ("%s@%s/", url->user, poa_address);
	source = e_source_new (source_name, relative_uri);
	e_source_set_property (source, "auth", "1");
	e_source_set_property (source, "username", url->user);
	e_source_set_property (source, "port", camel_url_get_param (url, "soap_port"));
	e_source_set_property (source, "auth-domain", "MAPI");
	e_source_set_property (source, "use_ssl", use_ssl);
	e_source_set_property (source, "offline_sync", offline_sync ? "1" : "0" );
	e_source_set_property (source, "delete", "no");
	if (parent_id_name) {
		e_source_set_property (source, "parent_id_name", parent_id_name);
		e_source_set_color_spec (source, camel_url_get_param (url, "color"));
	} else
		e_source_set_color_spec (source, "#EEBC60");
	e_source_group_add_source (group, source, -1);
	e_source_list_sync (source_list, NULL);

	if (!strcmp (conf_key, CALENDAR_SOURCES)) 
		source_selection_key = SELECTED_CALENDARS;
	else if (!strcmp (conf_key, TASKS_SOURCES))
		source_selection_key = SELECTED_TASKS;
	else if (!strcmp (conf_key, NOTES_SOURCES))
		source_selection_key = SELECTED_NOTES;
	else 
		source_selection_key = NULL;

	if (source_selection_key) {
		ids = gconf_client_get_list (client, source_selection_key , GCONF_VALUE_STRING, NULL);
		ids = g_slist_append (ids, g_strdup (e_source_peek_uid (source)));
		gconf_client_set_list (client,  source_selection_key, GCONF_VALUE_STRING, ids, NULL);
		temp  = ids;

		for (; temp != NULL; temp = g_slist_next (temp))
			g_free (temp->data);

		g_slist_free (ids);
	}
	
	g_object_unref (source);
	g_object_unref (group);
	g_object_unref (source_list);
	g_object_unref (client);
	g_free (relative_uri);
}

static void 
remove_esource (const char *conf_key, const char *group_name, char* source_name, const char* relative_uri)
{
	ESourceList *list;
        GSList *groups;
	gboolean found_group;
	GConfClient* client;
	GSList *ids;
	GSList *node_tobe_deleted;
	char *source_selection_key;
                                                                                                                             
        client = gconf_client_get_default();
        list = e_source_list_new_for_gconf (client, conf_key);
	groups = e_source_list_peek_groups (list); 
	
	found_group = FALSE;
	
	for ( ; groups != NULL && !found_group; groups = g_slist_next (groups)) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		
		if (strcmp (e_source_group_peek_name (group), group_name) == 0 && 
		   strcmp (e_source_group_peek_base_uri (group), MAPI_URI_PREFIX ) == 0) {
			GSList *sources = e_source_group_peek_sources (group);
			
			for( ; sources != NULL; sources = g_slist_next (sources)) {
				ESource *source = E_SOURCE (sources->data);
				const gchar *source_relative_uri;

				source_relative_uri = e_source_peek_relative_uri (source);
				if (source_relative_uri == NULL)
					continue;
				if (strcmp (source_relative_uri, relative_uri) == 0) {
				
					if (!strcmp (conf_key, CALENDAR_SOURCES)) 
						source_selection_key = SELECTED_CALENDARS;
					else if (!strcmp (conf_key, TASKS_SOURCES))
						source_selection_key = SELECTED_TASKS;
					else if (!strcmp (conf_key, NOTES_SOURCES))
						source_selection_key = SELECTED_NOTES;
					else source_selection_key = NULL;
					if (source_selection_key) {
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
					e_source_list_remove_group (list, group);
					e_source_list_sync (list, NULL);	
					found_group = TRUE;
					break;
					
				}
			}

		}
		
	      
	}

	g_object_unref (list);
	g_object_unref (client);		
	
}

/* looks up for e-source with having same info as old_account_info and changes its values passed in new values */

static void 
modify_esource (const char* conf_key, ExchangeAccountInfo *old_account_info, const char* new_group_name, CamelURL *new_url)
{
	ESourceList *list;
        GSList *groups;
	char *old_relative_uri;
	CamelURL *url;
	gboolean found_group;
      	GConfClient* client;
	const char *poa_address;
	const char *new_poa_address;
	
	url = camel_url_new (old_account_info->source_url, NULL);
	poa_address = url->host; 
	if (!poa_address || strlen (poa_address) ==0)
		return;
	new_poa_address = new_url->host;
	
	old_relative_uri =  g_strdup_printf ("%s@%s/", url->user, poa_address);
	client = gconf_client_get_default ();
        list = e_source_list_new_for_gconf (client, conf_key);
	groups = e_source_list_peek_groups (list); 

	found_group = FALSE;

	for ( ; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		
		if (strcmp (e_source_group_peek_name (group), old_account_info->name) == 0 && 
		    strcmp (e_source_group_peek_base_uri (group), MAPI_URI_PREFIX) == 0) {
			GSList *sources = e_source_group_peek_sources (group);
			
			for ( ; sources != NULL; sources = g_slist_next (sources)) {
				ESource *source = E_SOURCE (sources->data);
				const gchar *source_relative_uri;

				source_relative_uri = e_source_peek_relative_uri (source);
				if (source_relative_uri == NULL)
					continue;
				if (strcmp (source_relative_uri, old_relative_uri) == 0) {
					gchar *new_relative_uri;

					new_relative_uri = g_strdup_printf ("%s@%s/", new_url->user, new_poa_address); 
					e_source_group_set_name (group, new_group_name);
					e_source_set_relative_uri (source, new_relative_uri);
					e_source_set_property (source, "username", new_url->user);
					e_source_set_property (source, "port", camel_url_get_param (new_url,"soap_port"));
					e_source_set_property (source, "use_ssl",  camel_url_get_param (url, "use_ssl"));
					e_source_set_property (source, "offline_sync",  camel_url_get_param (url, "offline_sync") ? "1" : "0");
					e_source_list_sync (list, NULL);
					found_group = TRUE;
					g_free (new_relative_uri);
					break;
				}
			}
		}
	}

	g_object_unref (list);
	g_object_unref (client);
	camel_url_free (url);
	g_free (old_relative_uri);

	
}

#if 0
/* add sources for calendar and tasks if the account added is exchange account
   adds the new account info to  mapi_accounts list */

static void 
add_calendar_tasks_sources (ExchangeAccountInfo *info)
{
	CamelURL *url;
	
	url = camel_url_new (info->source_url, NULL);
	add_esource ("/apps/evolution/calendar/sources", info->name, _("Calendar"), url, NULL, FALSE);
	add_esource ("/apps/evolution/tasks/sources", info->name, _("Tasks"), url, NULL, FALSE);
	add_esource ("/apps/evolution/memos/sources", info->name, _("Notes"), url, NULL, TRUE);
	
	camel_url_free (url);


}

/* removes calendar and tasks sources if the account removed is exchange account 
   removes the the account info from mapi_account list */

static void 
remove_calendar_tasks_sources (ExchangeAccountInfo *info)
{
	CamelURL *url;
	char *relative_uri;
        const char *soap_port;
	const char *poa_address;

	url = camel_url_new (info->source_url, NULL);

	poa_address = url->host; 
	if (!poa_address || strlen (poa_address) ==0)
		return;

	soap_port = camel_url_get_param (url, "soap_port");
	if (!soap_port || strlen (soap_port) == 0)
		soap_port = "7191";

	relative_uri =  g_strdup_printf ("%s@%s/", url->user, poa_address);
	remove_esource ("/apps/evolution/calendar/sources", info->name, _("Calendar"), relative_uri);
	remove_esource ("/apps/evolution/tasks/sources", info->name,  _("Checklist"), relative_uri);
	remove_esource ("/apps/evolution/memos/sources", info->name,  _("Notes"), relative_uri);
	
	camel_url_free (url);
	g_free (relative_uri);

}
#endif

static gboolean
add_addressbook_sources (EAccount *account, GList *folders)
{
	CamelURL *url;
	ESourceList *list;
        ESourceGroup *group;
        ESource *source;
       	char *base_uri;
	GList *books_list, *temp_list;
	GConfClient* client;
	const char* use_ssl;
	const char *poa_address;
	gboolean is_frequent_contacts = FALSE, is_writable = FALSE;

        url = camel_url_new (account->source->url, NULL);
	if (url == NULL) {
		return FALSE;
	}

	base_uri =  g_strdup_printf ("mapi://%s@%s", url->user, url->host);
	client = gconf_client_get_default ();
	list = e_source_list_new_for_gconf (client, "/apps/evolution/addressbook/sources" );
	group = e_source_group_new (account->name, base_uri);

	for (temp_list = folders; temp_list != NULL; temp_list = g_list_next (temp_list)) {
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

	e_source_list_add_group (list, group, -1);
	e_source_list_sync (list, NULL);
	g_object_unref (group);
	g_object_unref (list);
	g_object_unref (client);
	g_free (base_uri);

	return TRUE;
}

static char *utf8tolinux(TALLOC_CTX *mem_ctx, const char *wstring)
{
	char		*newstr;

	newstr = windows_to_utf8(mem_ctx, wstring);
	return newstr;
}

static const char *
get_container_class(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		count;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_CONTAINER_CLASS);
	retval = GetProps(&obj_folder, SPropTagArray, &lpProps, &count);
	MAPIFreeBuffer(SPropTagArray);
	if ((lpProps[0].ulPropTag != PR_CONTAINER_CLASS) || (retval != MAPI_E_SUCCESS)) {
		errno = 0;
		return IPF_NOTE;
	}
	return lpProps[0].value.lpszA;
}

static gboolean
get_child_folders(TALLOC_CTX *mem_ctx, mapi_object_t *parent, const char *parent_name, mapi_id_t folder_id, int count, GList *folders)
{
	enum MAPISTATUS		retval;
	bool			ret;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	const char	       	*name;
	const char 		*class;
	char			*newname;
	const uint32_t		*child;
	uint32_t		index;
	const uint64_t		*fid;
	int			i;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) 
		return FALSE;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable);
	if (retval != MAPI_E_SUCCESS) 
		return FALSE;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
					  PR_DISPLAY_NAME,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS)
		return FALSE;
	
	while ((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, &rowset) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			ExchangeMAPIFolder *folder;
			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			name = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME);
			child = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_FOLDER_CHILD_COUNT);
			class = get_container_class(mem_ctx, parent, *fid);
			newname = utf8tolinux(mem_ctx, name);
			printf("|---+ %-15s : (Container class: %s %016llx)\n", newname, class, *fid);
			folder = exchange_mapi_folder_new (newname, parent_name, class, *fid, folder_id, child);
			folders = g_list_append (folders, folder);
			if (*child)
				get_child_folders(mem_ctx, &obj_folder, newname, *fid, count + 1, folders);
			MAPIFreeBuffer(newname);

			
		}
	}
	return FALSE;
}

void
exchange_account_fetch_folders ()
{
	TALLOC_CTX *mem_ctx;
	mapi_object_t obj_store;
	enum MAPISTATUS			retval;
	mapi_id_t			id_mailbox;
	struct SPropTagArray		*SPropTagArray;
	struct SPropValue		*lpProps = NULL;
	uint32_t			cValues;
	const char			*mailbox_name;
	char				*utf8_mailbox_name;
	ExchangeMAPIFolder *folder;

	mem_ctx = talloc_init("Evolution");
	mapi_object_init(&obj_store);

	retval = OpenMsgStore(&obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", GetLastError());
		return NULL;
	}

	/* Retrieve the mailbox folder name */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(&obj_store, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) 
		return NULL;

	if (lpProps[0].value.lpszA) {
		mailbox_name = lpProps[0].value.lpszA;
	} else {
		return NULL;
	}	

	/* Prepare the directory listing */
	retval = GetDefaultFolder(&obj_store, &id_mailbox, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("[Openchange_plugin] Error ", retval);
		return NULL;
	}
	utf8_mailbox_name = utf8tolinux(mem_ctx, mailbox_name);

	/* FIXME: May have to get the child folders count? Do we need/use it? */
	folder = exchange_mapi_folder_new (utf8_mailbox_name, NULL, IPF_NOTE, id_mailbox, 0, 0); 

	mapi_folders = g_list_append (mapi_folders, folder);
	get_child_folders(mem_ctx, &obj_store, utf8_mailbox_name, id_mailbox, 0, mapi_folders);

	MAPIFreeBuffer(utf8_mailbox_name);

}
/*
static void 
modify_addressbook_sources ( EAccount *account, ExchangeAccountInfo *existing_account_info )
{
	CamelURL *url;
	ESourceList *list;
        ESourceGroup *group;
	GSList *groups;
       	gboolean found_group;
	gboolean delete_group;
	char *old_base_uri;
	char *new_base_uri;
	const char *soap_port;
	const char *use_ssl;
	GSList *sources;
	ESource *source;
	GConfClient *client;
	const char *poa_address;
	

	url = camel_url_new (existing_account_info->source_url, NULL);
	if (url == NULL) {
		return;
	}

	poa_address = url->host; 
	if (!poa_address || strlen (poa_address) ==0)
		return;

	old_base_uri =  g_strdup_printf ("mapi://%s@%s", url->user, poa_address);
	camel_url_free (url);
	
	url = camel_url_new (account->source->url, NULL);
	if (url == NULL)
		return ;
	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;
	new_base_uri = g_strdup_printf ("mapi://%s@%s", url->user, poa_address);
	soap_port = camel_url_get_param (url, "soap_port");
	if (!soap_port || strlen (soap_port) == 0)
		soap_port = "7191";
	use_ssl = camel_url_get_param (url, "use_ssl");

	client = gconf_client_get_default ();
	list = e_source_list_new_for_gconf (client, "/apps/evolution/addressbook/sources" );
	groups = e_source_list_peek_groups (list); 
	delete_group = FALSE;
	if (strcmp (old_base_uri, new_base_uri) != 0)
		delete_group = TRUE;
	group = NULL;
	found_group = FALSE;
	for ( ; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		if ( strcmp ( e_source_group_peek_base_uri(group), old_base_uri) == 0 && strcmp (e_source_group_peek_name (group), existing_account_info->name) == 0) {
			found_group = TRUE;
			if (!delete_group) {
				e_source_group_set_name (group, account->name);
				sources = e_source_group_peek_sources (group);
				for (; sources != NULL; sources = g_slist_next (sources)) {
					source = E_SOURCE (sources->data);
					e_source_set_property (source, "port", soap_port);
					e_source_set_property (source, "use_ssl", use_ssl);
				}
					
				e_source_list_sync (list, NULL);
			}
		
		}
	}
	if (found_group && delete_group) {
		e_source_list_remove_group (list, group);
		e_source_list_sync (list, NULL);
		g_object_unref (list);
		list = NULL;
		add_addressbook_sources (account);
	}
	g_free (old_base_uri);
	if (list)
		g_object_unref (list);
	camel_url_free (url);
	g_object_unref (client);


}
*/

static void 
remove_addressbook_sources (ExchangeAccountInfo *existing_account_info)
{
	ESourceList *list;
        ESourceGroup *group;
	GSList *groups;
       	gboolean found_group;
	CamelURL *url;
	char *base_uri;
	const char *soap_port;
	GConfClient *client;
	const char *poa_address;

	url = camel_url_new (existing_account_info->source_url, NULL);
	if (url == NULL) {
		return;
	}

	poa_address = url->host; 
	if (!poa_address || strlen (poa_address) ==0)
		return;

	soap_port = camel_url_get_param (url, "soap_port");
	if (!soap_port || strlen (soap_port) == 0)
		soap_port = "7191";
	base_uri =  g_strdup_printf ("mapi://%s@%s", url->user,  poa_address);
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
account_added (EAccountList *account_listener, EAccount *account)
{
	ExchangeAccountInfo *info;
	EAccount *parent;
	gboolean status;
	CamelURL *parent_url;
	GList *folders;

	if (!is_mapi_account (account))
		return;
	
	info = g_new0 (ExchangeAccountInfo, 1);
	info->uid = g_strdup (account->uid);
	info->name = g_strdup (account->name);
	info->source_url = g_strdup (account->source->url);
	printf("account happens\n");

	add_addressbook_sources (account, mapi_folders);	
		
//	if (status) 
//		add_calendar_tasks_sources (info);
	mapi_accounts = g_list_append (mapi_accounts, info);
}

#if 0
static void 
account_removed (EAccountList *account_listener, EAccount *account)
{
       	ExchangeAccountInfo *info;
	
	if (!is_mapi_account (account))
		return;
	
	info = lookup_account_info (account->uid);
	if (info == NULL) 
		return;

	remove_calendar_tasks_sources (info);
	remove_addressbook_sources (info);
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
	const char *old_soap_port, *new_soap_port;
	ExchangeAccountInfo *existing_account_info;
	const char *old_use_ssl, *new_use_ssl;
	const char *old_poa_address, *new_poa_address;
	
	bis_mapi_account = is_mapi_account (account);
	
	existing_account_info = lookup_account_info (account->uid);
       
	if (existing_account_info == NULL && bis_mapi_account) {

		if (!account->enabled)
			return;

		/* some account of other type is changed to MAPI */
		account_added (account_listener, account);

	} else if ( existing_account_info != NULL && !bis_mapi_account) {

		/*MAPI account is changed to some other type */
		remove_calendar_tasks_sources (existing_account_info);
		remove_addressbook_sources (existing_account_info);
		mapi_accounts = g_list_remove (mapi_accounts, existing_account_info);
		g_free (existing_account_info->uid);
		g_free (existing_account_info->name);
		g_free (existing_account_info->source_url);
		g_free (existing_account_info);
		
	} else if ( existing_account_info != NULL && bis_mapi_account ) {
		
		if (!account->enabled) {
			account_removed (account_listener, account);
			return;
		}
		
		/* some info of mapi account is changed . update the sources with new info if required */
		old_url = camel_url_new (existing_account_info->source_url, NULL);
		old_poa_address = old_url->host; 
		old_soap_port = camel_url_get_param (old_url, "soap_port");
		old_use_ssl = camel_url_get_param (old_url, "use_ssl");
		new_url = camel_url_new (account->source->url, NULL);
		new_poa_address = new_url->host; 

		if (!new_poa_address || strlen (new_poa_address) ==0)
			return;

		new_soap_port = camel_url_get_param (new_url, "soap_port");

		if (!new_soap_port || strlen (new_soap_port) == 0)
			new_soap_port = "7191";

		new_use_ssl = camel_url_get_param (new_url, "use_ssl");

		if ((old_poa_address && strcmp (old_poa_address, new_poa_address))
		   ||  (old_soap_port && strcmp (old_soap_port, new_soap_port)) 
		   ||  strcmp (old_url->user, new_url->user)
	           || (!old_use_ssl) 
		   || strcmp (old_use_ssl, new_use_ssl)) {
			
			account_removed (account_listener, account);
			account_added (account_listener, account);
		} else if (strcmp (existing_account_info->name, account->name)) {
			
			modify_esource ("/apps/evolution/calendar/sources", existing_account_info, account->name, new_url);
			modify_esource ("/apps/evolution/tasks/sources", existing_account_info, account->name,  new_url);
			modify_esource ("/apps/evolution/memos/sources", existing_account_info, account->name,  new_url);
			modify_addressbook_sources (account, existing_account_info);
			
		}
		
		g_free (existing_account_info->name);
		g_free (existing_account_info->source_url);
		existing_account_info->name = g_strdup (account->name);
		existing_account_info->source_url = g_strdup (account->source->url);
		camel_url_free (old_url);
		camel_url_free (new_url);
	}	
} 
#endif

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

	g_signal_connect (config_listener->priv->account_list, "account_added", G_CALLBACK (account_added), NULL);
//	g_signal_connect (config_listener->priv->account_list, "account_changed", G_CALLBACK (account_changed), NULL);
//	g_signal_connect (config_listener->priv->account_list, "account_removed", G_CALLBACK (account_removed), NULL);    
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
