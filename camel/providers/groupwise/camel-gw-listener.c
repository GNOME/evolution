/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *  
 *  Sivaiah Nallagatla <snallagatla@novell.com>
 *
 * Copyright 2003, Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-gw-listener.h"
#include <string.h>
#include  "camel-i18n.h"

/*stores some info about all currently existing groupwise accounts 
  list of GwAccountInfo structures */

static 	GList *groupwise_accounts = NULL;

struct _CamelGwListenerPrivate {
	GConfClient *gconf_client;
	/* we get notification about mail account changes form this object */
	EAccountList *account_list;                  
};

struct _GwAccountInfo {
	char *uid;
	char *name;
	char *source_url;
};

typedef struct _GwAccountInfo GwAccountInfo;

#define GROUPWISE_URI_PREFIX   "groupwise://" 
#define GROUPWISE_PREFIX_LENGTH 12

#define LDAP_URI_PREFIX "ldap://"

#define PARENT_TYPE G_TYPE_OBJECT

static GObjectClass *parent_class = NULL;

static void dispose (GObject *object);
static void finalize (GObject *object);


static void 
camel_gw_listener_class_init (CamelGwListenerClass *class)
{
	GObjectClass *object_class;
	
	parent_class =  g_type_class_ref (PARENT_TYPE);
	object_class = G_OBJECT_CLASS (class);
	
	/* virtual method override */
	object_class->dispose = dispose;
	object_class->finalize = finalize;
}

static void 
camel_gw_listener_init (CamelGwListener *config_listener,  CamelGwListenerClass *class)
{
	config_listener->priv = g_new0 (CamelGwListenerPrivate, 1);	
}

static void 
dispose (GObject *object)
{
	CamelGwListener *config_listener = CAMEL_GW_LISTENER (object);
	
	g_object_unref (config_listener->priv->gconf_client);
	g_object_unref (config_listener->priv->account_list);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void 
finalize (GObject *object)
{
	CamelGwListener *config_listener = CAMEL_GW_LISTENER (object);
	GList *list;
	GwAccountInfo *info;

	if (config_listener->priv) {
		g_free (config_listener->priv);
	}

	for ( list = g_list_first (groupwise_accounts); list ; list = g_list_next (list) ) {
	       
		info = (GwAccountInfo *) (list->data);

		if (info) {
			
			g_free (info->uid);
			g_free (info->name);
			g_free (info->source_url);
			g_free (info);
		}
	}
	
	g_list_free (groupwise_accounts);
}

/*determines whehter the passed in account is groupwise or not by looking at source url */

static gboolean
is_groupwise_account (EAccount *account)
{
	if (account->source->url != NULL) {
		return (strncmp (account->source->url,  GROUPWISE_URI_PREFIX, GROUPWISE_PREFIX_LENGTH ) == 0);
	} else {
		return FALSE;
	}
}

/* looks up for an existing groupwise account info in the groupwise_accounts list based on uid */

static GwAccountInfo* 
lookup_account_info (const char *key)
{
	GList *list;
        GwAccountInfo *info ;
	int found = 0;
                                                                      
        if (!key)
                return NULL;

	info = NULL;

        for (list = g_list_first (groupwise_accounts);  list;  list = g_list_next (list)) {
                info = (GwAccountInfo *) (list->data);
                found = strcmp (info->uid, key) == 0;
		if (found)
			break;
	}

	return info;
}


static void
add_esource (const char *conf_key, const char *group_name,  const char* source_name, const char *username, const char* relative_uri)
{
	ESourceList *source_list;
	ESourceGroup *group;
	ESource *source;
	
	source_list = e_source_list_new_for_gconf (gconf_client_get_default (), conf_key);

	group = e_source_group_new (group_name,  GROUPWISE_URI_PREFIX);
	if ( !e_source_list_add_group (source_list, group, -1))
		return;

	source = e_source_new (source_name, relative_uri);
	e_source_set_property (source, "auth", "1");
	e_source_set_property (source, "username", username);
	e_source_group_add_source (group, source, -1);

	e_source_list_sync (source_list, NULL);

	g_object_unref (source);
	g_object_unref (group);
	g_object_unref (source_list);
}


static void 
remove_esource (const char *conf_key, const char *group_name, char* source_name, const char* relative_uri)
{
	ESourceList *list;
        ESourceGroup *group;
        ESource *source;
        GSList *groups;
        GSList *sources;
	gboolean found_group;

        list = e_source_list_new_for_gconf (gconf_client_get_default (), conf_key);
	groups = e_source_list_peek_groups (list); 
	
	found_group = FALSE;
	
	for ( ; groups != NULL && !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		
		if (strcmp (e_source_group_peek_name (group), group_name) == 0 && 
		   strcmp (e_source_group_peek_base_uri (group), GROUPWISE_URI_PREFIX ) == 0) {

			sources = e_source_group_peek_sources (group);
			
			for( ; sources != NULL; sources = g_slist_next (sources)) {
				
				source = E_SOURCE (sources->data);
				
				if (strcmp (e_source_peek_relative_uri (source), relative_uri) == 0) {
					
					e_source_list_remove_group (list, group);
					e_source_list_sync (list, NULL);	
					found_group = TRUE;
					break;
					
				}
			}

		}
		
	      
	}

	g_object_unref (list);
			
	
}

/* looks up for e-source with having same info as old_account_info and changes its values passed in new values */

static void 
modify_esource (const char* conf_key, GwAccountInfo *old_account_info, const char* new_group_name, const char* new_relative_uri)
{
	ESourceList *list;
        ESourceGroup *group;
        ESource *source;
        GSList *groups;
	GSList *sources;
	char *old_relative_uri;
	CamelURL *url;
	gboolean found_group;

	url = camel_url_new (old_account_info->source_url, NULL);
	old_relative_uri =  g_strdup_printf ("%s@%s", url->user, url->host);
	
        list = e_source_list_new_for_gconf (gconf_client_get_default (), conf_key);
	groups = e_source_list_peek_groups (list); 

	found_group = FALSE;

	for ( ; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		
		if (strcmp (e_source_group_peek_name (group), old_account_info->name) == 0 && 
		   strcmp (e_source_group_peek_base_uri (group), GROUPWISE_URI_PREFIX) == 0) {
			
			sources = e_source_group_peek_sources (group);
			
			for ( ; sources != NULL; sources = g_slist_next (sources)) {
				
				source = E_SOURCE (sources->data);
				
				if (strcmp (e_source_peek_relative_uri (source), old_relative_uri) == 0) {
					
					e_source_group_set_name (group, new_group_name);
					e_source_set_relative_uri (source, new_relative_uri);
					e_source_list_sync (list, NULL);
					found_group = TRUE;
					break;
				}
			}
		}
	}

	g_object_unref (list);
	camel_url_free (url);
	g_free (old_relative_uri);
	
}
/* add sources for calendar and tasks if the account added is groupwise account
   adds the new account info to  groupwise_accounts list */

static void 
add_calendar_tasks_sources (GwAccountInfo *info)
{
	CamelURL *url;
	char *relative_uri;
	
	url = camel_url_new (info->source_url, NULL);
	/* FIXME: don't hard-code the port number */
	relative_uri =  g_strdup_printf ("%s:7181/soap", url->host);
	add_esource ("/apps/evolution/calendar/sources", info->name, N_("Default"), url->user, relative_uri);
	add_esource ("/apps/evolution/tasks/sources", info->name, N_("Default"), url->user,  relative_uri);
	
	groupwise_accounts = g_list_append (groupwise_accounts, info);
	
	camel_url_free (url);
	g_free (relative_uri);

}

/* removes calendar and tasks sources if the account removed is groupwise account 
   removes the the account info from groupwise_account list */

static void 
remove_calendar_tasks_sources (GwAccountInfo *info)
{
	CamelURL *url;
	char *relative_uri;
	
	url = camel_url_new (info->source_url, NULL);
	relative_uri =  g_strdup_printf ("%s@%s", url->user, url->host);

	remove_esource ("/apps/evolution/calendar/sources", info->name,  "Default", relative_uri);
	remove_esource ("/apps/evolution/tasks/sources", info->name,  "Default", relative_uri);
	
	g_free (info->uid);
	g_free (info->name);
	g_free (info->source_url);
	groupwise_accounts = g_list_remove (groupwise_accounts, info);
	g_free (info);
	camel_url_free (url);
	g_free (relative_uri);

}
static void 
add_ldap_addressbook_source (EAccount *account)
{
	CamelURL *url;
	const char *ldap_server_name;
	const char *search_scope;
	ESourceList *list;
        ESourceGroup *group;
        ESource *source;
        GSList *groups;
       	gboolean found_group;
	char * relative_uri;

	url = camel_url_new (account->source->url, NULL);

	if (url == NULL) {
		return;
	}
	
	ldap_server_name = camel_url_get_param (url, "ldap_server");
	search_scope = camel_url_get_param (url, "search_base");

	if (ldap_server_name == NULL) {
	
		return;
	}

	list = e_source_list_new_for_gconf (gconf_client_get_default (), "/apps/evolution/addressbook/sources" );
	groups = e_source_list_peek_groups (list); 

	relative_uri = g_strdup_printf ("%s:%s/%s%s%s", ldap_server_name, "389",
                                      search_scope, "??", "sub");
		
	found_group = FALSE;

	for ( ; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		if ( strcmp ( e_source_group_peek_base_uri (group), LDAP_URI_PREFIX) == 0) {
			source = e_source_new (account->name, relative_uri);
			e_source_set_property ( source, "limit", "100");
			e_source_set_property ( source, "ssl", "never");
			e_source_set_property (source, "auth", "none");
			e_source_group_add_source (group, source, -1);
			e_source_list_sync (list, NULL);
			found_group = TRUE;
		}
	}

	g_free (relative_uri);
	g_object_unref (list);
	camel_url_free (url);
	
}

static void 
modify_ldap_addressbook_source ( EAccount *account)
{
	CamelURL *url;
	const char *ldap_server_name;
	const char *search_scope;
	ESourceList *list;
        ESourceGroup *group;
        ESource *source;
        GSList *groups;
       	gboolean found_group;
	char * relative_uri;

	url = camel_url_new (account->source->url, NULL);

	if (url == NULL) {
		return;
	}
	
	ldap_server_name = camel_url_get_param (url, "ldap_server");

	if (ldap_server_name == NULL) {
		return;
	}
		
	search_scope = camel_url_get_param (url, "search_base");

	list = e_source_list_new_for_gconf (gconf_client_get_default (), "/apps/evolution/addressbook/sources" );
	groups = e_source_list_peek_groups (list); 

	relative_uri = g_strdup_printf ("%s:%s/%s%s%s", ldap_server_name, "389",
                                      search_scope, "??", "sub");
		
	found_group = FALSE;

	for ( ; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		if ( strcmp ( e_source_group_peek_base_uri (group), LDAP_URI_PREFIX) == 0) {
			source = e_source_group_peek_source_by_name (group, account->name);
			e_source_set_relative_uri (source, relative_uri);
			e_source_list_sync (list, NULL);
			found_group = TRUE;
		}
	}

	g_free (relative_uri);
	g_object_unref (list);
	camel_url_free (url);

}
static void 
remove_ldap_addressbook_source ( EAccount *account )
{
	ESourceList *list;
        ESourceGroup *group;
        ESource *source;
        GSList *groups;
       	gboolean found_group;

	list = e_source_list_new_for_gconf (gconf_client_get_default (), "/apps/evolution/addressbook/sources" );
	groups = e_source_list_peek_groups (list); 

		found_group = FALSE;

	for ( ; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		if ( strcmp ( e_source_group_peek_base_uri (group), LDAP_URI_PREFIX) == 0) {

			source = e_source_group_peek_source_by_name (group, account->name);
			e_source_group_remove_source (group, source);
			e_source_list_sync (list, NULL);
			found_group = TRUE;
						
		}
	}
	g_object_unref (list);
	

}


static void 
account_added (EAccountList *account_listener, EAccount *account)
{

	GwAccountInfo *info;

	if (!is_groupwise_account (account))
		return;
	
	info = g_new0 (GwAccountInfo, 1);
	info->uid = g_strdup (account->uid);
	info->name = g_strdup (account->name);
	info->source_url = g_strdup (account->source->url);
	
	add_calendar_tasks_sources (info);
	add_ldap_addressbook_source(account);
}



static void
account_changed (EAccountList *account_listener, EAccount *account)
{
	gboolean is_gw_account;
	CamelURL *url;
	char *relative_uri;

	GwAccountInfo *existing_account_info;
	is_gw_account = is_groupwise_account (account);
	existing_account_info = lookup_account_info (account->uid);
		
	if (existing_account_info == NULL && is_gw_account) {
		/* some account of other type is changed to Groupwise */
		account_added (account_listener, account);

	} else if ( existing_account_info != NULL && !is_gw_account) {

		/*Groupwise account is changed to some other type */

		remove_calendar_tasks_sources ( existing_account_info);

	} else if ( existing_account_info != NULL && is_gw_account ) {

		/* some info of groupwise account is changed . update the sources with new info if required */
 
		if (strcmp (existing_account_info->name, account->name) != 0 || strcmp (existing_account_info->source_url, account->source->url) != 0) {
			
			url = camel_url_new (account->source->url, NULL);
			relative_uri =  g_strdup_printf ("%s@%s", url->user, url->host);
			modify_esource ("/apps/evolution/calendar/sources", existing_account_info, account->name, relative_uri);
			modify_esource ("/apps/evolution/tasks/sources", existing_account_info, account->name, relative_uri);
			g_free (existing_account_info->name);
			g_free (existing_account_info->source_url);
			existing_account_info->name = g_strdup (account->name);
			existing_account_info->source_url = g_strdup (account->source->url);
			camel_url_free (url);
			modify_ldap_addressbook_source (account);
		}
		
	}
		
	
} 

static void 
account_removed (EAccountList *account_listener, EAccount *account)
{
       	GwAccountInfo *info;
	
	if (!is_groupwise_account (account))
		return;
	
	info = lookup_account_info (account->uid);
	if (info == NULL) {
		return;
	}

	remove_calendar_tasks_sources (info);
	remove_ldap_addressbook_source (account);

}

static void
camel_gw_listener_construct (CamelGwListener *config_listener)
{
	EIterator *iter;
	EAccount *account;
	GwAccountInfo *info ;
	
       	config_listener->priv->account_list = e_account_list_new (config_listener->priv->gconf_client);

	for ( iter = e_list_get_iterator (E_LIST ( config_listener->priv->account_list) ) ; e_iterator_is_valid (iter); e_iterator_next (iter) ) {
		
		account = E_ACCOUNT (e_iterator_get (iter));
		if ( is_groupwise_account (account)) {
			
		        info = g_new0 (GwAccountInfo, 1);
			info->uid = g_strdup (account->uid);
			info->name = g_strdup (account->name);
			info->source_url = g_strdup (account->source->url);
			groupwise_accounts = g_list_append (groupwise_accounts, info);
			
		}
			
	}
	g_signal_connect (config_listener->priv->account_list, "account_added", G_CALLBACK (account_added), NULL);
	g_signal_connect (config_listener->priv->account_list, "account_changed", G_CALLBACK (account_changed), NULL);
	g_signal_connect (config_listener->priv->account_list, "account_removed", G_CALLBACK (account_removed), NULL);
	
       
}

GType
camel_gw_listener_get_type (void)
{
	static GType camel_gw_listener_type  = 0;

	if (!camel_gw_listener_type) {
		static GTypeInfo info = {
                        sizeof (CamelGwListenerClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) camel_gw_listener_class_init,
                        NULL, NULL,
                        sizeof (CamelGwListener),
                        0,
                        (GInstanceInitFunc) camel_gw_listener_init
                };
		camel_gw_listener_type = g_type_register_static (PARENT_TYPE, "CamelGwListener", &info, 0);
	}

	return camel_gw_listener_type;
}

CamelGwListener*
camel_gw_listener_new ()
{
	CamelGwListener *config_listener;
       
	config_listener = g_object_new (CAMEL_TYPE_GW_LISTENER, NULL);
	config_listener->priv->gconf_client = gconf_client_get_default();
	
	camel_gw_listener_construct (config_listener);

	return config_listener;

}


