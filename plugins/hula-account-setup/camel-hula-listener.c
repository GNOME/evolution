/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *  
 *  Harish Krishnaswamy <kharish@novell.com>
 *
 * Copyright 2005, Novell, Inc.
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

#include <string.h>
#include <camel/camel-i18n.h>
#include <libedataserverui/e-passwords.h>
#include <e-util/e-error.h>
#include <e-util/e-account.h>
#include "camel-hula-listener.h"


static 	GList *hula_accounts = NULL;

struct _CamelHulaListenerPrivate {
	GConfClient *gconf_client;
	/* we get notification about mail account changes form this object */
	EAccountList *account_list;                  
};

struct _HulaAccountInfo {
	char *uid;
	char *name;
	char *source_url;
};

typedef struct _HulaAccountInfo HulaAccountInfo;

#define HULA_CALDAV_URI_PREFIX "caldav://"
#define HULA_PREFIX_LENGTH 9
#define HULA_URI_PREFIX "hula://"
#define HULA_PREFIX_LENGTH 7
#define PARENT_TYPE G_TYPE_OBJECT

static GObjectClass *parent_class = NULL;

static void dispose (GObject *object);
static void finalize (GObject *object);


static void 
camel_hula_listener_class_init (CamelHulaListenerClass *class)
{
	GObjectClass *object_class;
	
	parent_class =  g_type_class_ref (PARENT_TYPE);
	object_class = G_OBJECT_CLASS (class);
	
	/* virtual method override */
	object_class->dispose = dispose;
	object_class->finalize = finalize;
}

static void 
camel_hula_listener_init (CamelHulaListener *config_listener,  CamelHulaListenerClass *class)
{
	config_listener->priv = g_new0 (CamelHulaListenerPrivate, 1);	
}

static void 
dispose (GObject *object)
{
	CamelHulaListener *config_listener = CAMEL_HULA_LISTENER (object);
	
	g_object_unref (config_listener->priv->gconf_client);
	g_object_unref (config_listener->priv->account_list);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void 
finalize (GObject *object)
{
	CamelHulaListener *config_listener = CAMEL_HULA_LISTENER (object);
	GList *list;
	HulaAccountInfo *info;

	if (config_listener->priv) {
		g_free (config_listener->priv);
	}

	for ( list = g_list_first (hula_accounts); list ; list = g_list_next (list) ) {
	       
		info = (HulaAccountInfo *) (list->data);

		if (info) {
			
			g_free (info->uid);
			g_free (info->name);
			g_free (info->source_url);
			g_free (info);
		}
	}
	
	g_list_free (hula_accounts);
}

/*determines whehter the passed in account is hula or not by looking at source url */

static gboolean
is_hula_account (EAccount *account)
{
	if (account->source->url != NULL) {
		return (strncmp (account->source->url,  HULA_URI_PREFIX, HULA_PREFIX_LENGTH ) == 0);
	} else {
		return FALSE;
	}
}

/* looks up for an existing hula account info in the hula_accounts list based on uid */

static HulaAccountInfo* 
lookup_account_info (const char *key)
{
	GList *list;
        HulaAccountInfo *info ;
	int found = 0;
                                                                      
        if (!key)
                return NULL;

	info = NULL;

        for (list = g_list_first (hula_accounts);  list;  list = g_list_next (list)) {
                info = (HulaAccountInfo *) (list->data);
                found = (strcmp (info->uid, key) == 0);
		if (found)
			break;
	}
	if (found)
		return info;
	return NULL;
}

#define CALENDAR_SOURCES "/apps/evolution/calendar/sources"
#define SELECTED_CALENDARS "/apps/evolution/calendar/display/selected_calendars"

static void
add_esource (const char *conf_key, const char *group_name,  const char *source_name, CamelURL *url)
{
	ESourceList *source_list;
	ESourceGroup *group;
	ESource *source;
        GConfClient *client;
	GSList *ids, *temp ;
	gboolean result;
	char *source_selection_key;
	char *relative_uri;
	const char *cal_port = "8081";
	const char *use_ssl = "";
	/* offline_sync to come soon */ 
	
	/* TODO use_ssl = camel_url_get_param (url, "use_ssl"); */

	client = gconf_client_get_default();	
	if (client) {
		g_message ("could not get a valid gconf client\n");
		return;
	}
	source_list = e_source_list_new_for_gconf (client, conf_key);

	group = e_source_group_new (group_name, HULA_CALDAV_URI_PREFIX);
	result = e_source_list_add_group (source_list, group, -1);

	if (result == FALSE) {
		g_warning ("Could not add Hula source group!");	
	} else {
		e_source_list_sync (source_list, NULL);
	}

	/* caldav://localhost:8081/dav/kharish/calendar/Personal */

	relative_uri = g_strdup_printf ("%s@%s:%s/dav/%s/calendar/Personal", url->user, url->host, cal_port, url->user);
	g_message ("Relative uri is %s\n", relative_uri);
	
	source = e_source_new (source_name, relative_uri);
	/* e_source_set_property (source, "port", camel_url_get_param (url,
	 * "port")); 
	e_source_set_property (source, "auth-domain", "Hula");
	e_source_set_property (source, "use_ssl", use_ssl); */
	e_source_group_add_source (group, source, -1);
	e_source_list_sync (source_list, NULL);

	if (!strcmp (conf_key, CALENDAR_SOURCES)) 
		source_selection_key = SELECTED_CALENDARS;
	else source_selection_key = NULL;
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
        ESourceGroup *group;
        ESource *source;
        GSList *groups;
        GSList *sources;
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

		group = E_SOURCE_GROUP (groups->data);
		
		if (strcmp (e_source_group_peek_name (group), group_name) == 0 && 
		   strcmp (e_source_group_peek_base_uri (group), HULA_CALDAV_URI_PREFIX ) == 0) {

			sources = e_source_group_peek_sources (group);
			
			for( ; sources != NULL; sources = g_slist_next (sources)) {
				
				source = E_SOURCE (sources->data);
				
				if (strcmp (e_source_peek_relative_uri (source), relative_uri) == 0) {
				
					if (!strcmp (conf_key, CALENDAR_SOURCES)) 
						source_selection_key = SELECTED_CALENDARS;
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
modify_esource (const char* conf_key, HulaAccountInfo *old_account_info, const char* new_group_name, CamelURL *new_url)
{
	ESourceList *list;
        ESourceGroup *group;
        ESource *source;
        GSList *groups;
	GSList *sources;
	char *old_relative_uri;
	CamelURL *url;
	gboolean found_group;
      	GConfClient* client;
	const char *address;
	const char *cal_port;
	char *new_relative_uri;
	const char *new_address;
	
	url = camel_url_new (old_account_info->source_url, NULL);
	address = url->host; 
	if (!address || strlen (address) ==0)
		return;
	new_address = new_url->host;
	
	old_relative_uri = g_strdup_printf ("%s@%s:%s/dav/%s/calendar/Personal", url->user, url->host, cal_port, url->user);
	client = gconf_client_get_default ();
        list = e_source_list_new_for_gconf (client, conf_key);
	groups = e_source_list_peek_groups (list); 

	found_group = FALSE;

	for ( ; groups != NULL &&  !found_group; groups = g_slist_next (groups)) {

		group = E_SOURCE_GROUP (groups->data);
		
		if (strcmp (e_source_group_peek_name (group), old_account_info->name) == 0 && 
		    strcmp (e_source_group_peek_base_uri (group), HULA_CALDAV_URI_PREFIX) == 0) {

			sources = e_source_group_peek_sources (group);
			
			for ( ; sources != NULL; sources = g_slist_next (sources)) {
				
				source = E_SOURCE (sources->data);
				
				if (strcmp (e_source_peek_relative_uri (source), old_relative_uri) == 0) {
					
					new_relative_uri = g_strdup_printf ("%s@%s:%s/dav/%s/calendar/Personal", url->user, url->host, cal_port, url->user);
					e_source_group_set_name (group, new_group_name);
					e_source_set_relative_uri (source, new_relative_uri);
					e_source_set_property (source, "username", new_url->user);
					e_source_set_property (source, "port", camel_url_get_param (new_url,"port"));
					e_source_set_property (source, "use_ssl",  camel_url_get_param (url, "use_ssl"));
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
/* add sources for calendar if the account added is HULA account
   adds the new account info to  HULA accounts list */

static void 
add_calendar_sources (HulaAccountInfo *info)
{
	CamelURL *url;
	
	url = camel_url_new (info->source_url, NULL);
	add_esource ("/apps/evolution/calendar/sources", info->name, _("Calendar"), url);
	
	camel_url_free (url);


}

/* removes calendar  sources if the account removed is HULA account 
   removes the the account info from HULA_account list */

static void 
remove_calendar_sources (HulaAccountInfo *info)
{
	CamelURL *url;
	char *relative_uri;
	const char *address;
	const char *caldav_port;

	url = camel_url_new (info->source_url, NULL);

	address = url->host; 
	if (!address || strlen (address) ==0)
		return;

	caldav_port = camel_url_get_param (url, "caldav_port");
	if (!caldav_port || strlen (caldav_port) == 0)
		caldav_port = "8081";

	relative_uri = g_strdup_printf ("%s@%s:%s/dav/%s/calendar/Personal", url->user, url->host, caldav_port, url->user);
	remove_esource ("/apps/evolution/calendar/sources", info->name, _("Calendar"), relative_uri);
	camel_url_free (url);
	g_free (relative_uri);

}

static void 
account_added (EAccountList *account_listener, EAccount *account)
{

	HulaAccountInfo *info;
	EAccount *parent;
	CamelURL *parent_url;

	if (!is_hula_account (account))
		return;
	
	info = g_new0 (HulaAccountInfo, 1);
	info->uid = g_strdup (account->uid);
	info->name = g_strdup (account->name);
	info->source_url = g_strdup (account->source->url);
	if (account->parent_uid) {
		parent = (EAccount *)e_account_list_find (account_listener, E_ACCOUNT_FIND_UID, account->parent_uid);

		if (!parent) 
			return;

		parent_url = camel_url_new (e_account_get_string(parent, E_ACCOUNT_SOURCE_URL), NULL);	
	} else 
		add_calendar_sources (info);
	
	hula_accounts = g_list_append (hula_accounts, info);
}

static void 
account_removed (EAccountList *account_listener, EAccount *account)
{
       	HulaAccountInfo *info;
	
	if (!is_hula_account (account))
		return;
	
	info = lookup_account_info (account->uid);
	if (info == NULL) 
		return;

	remove_calendar_sources (info);
	hula_accounts = g_list_remove (hula_accounts, info);
	g_free (info->uid);
	g_free (info->name);
	g_free (info->source_url);
        g_free (info);
}


static void
account_changed (EAccountList *account_listener, EAccount *account)
{
	gboolean is_hula;
	CamelURL *old_url, *new_url;
	const char *old_caldav_port, *new_caldav_port;
	HulaAccountInfo *existing_account_info;
	const char *old_use_ssl, *new_use_ssl;
	const char *old_address, *new_address;
	
	is_hula = is_hula_account (account);
	
	existing_account_info = lookup_account_info (account->uid);
       
	if (existing_account_info == NULL && is_hula) {

		if (!account->enabled)
			return;

		/* some account of other type is changed to hula*/
		account_added (account_listener, account);

	} else if ( existing_account_info != NULL && !is_hula) {

		/*hula account is changed to some other type */
		remove_calendar_sources (existing_account_info);
		hula_accounts = g_list_remove (hula_accounts, existing_account_info);
		g_free (existing_account_info->uid);
		g_free (existing_account_info->name);
		g_free (existing_account_info->source_url);
		g_free (existing_account_info);
		
	} else if ( existing_account_info != NULL && is_hula ) {
		
		if (!account->enabled) {
			account_removed (account_listener, account);
			return;
		}
		
		/* some info of hula account is changed . update the sources with new info if required */
		old_url = camel_url_new (existing_account_info->source_url, NULL);
		old_address = old_url->host; 
		old_caldav_port = camel_url_get_param (old_url, "caldav_port");
		old_use_ssl = camel_url_get_param (old_url, "use_ssl");
		new_url = camel_url_new (account->source->url, NULL);
		new_address = new_url->host; 

		if (!new_address || strlen (new_address) ==0)
			return;

		new_caldav_port = camel_url_get_param (new_url, "caldav_port");

		if (!new_caldav_port || strlen (new_caldav_port) == 0)
			new_caldav_port = "8081";

		new_use_ssl = camel_url_get_param (new_url, "use_ssl");

		if ((old_address && strcmp (old_address, new_address))
		   ||  (old_caldav_port && strcmp (old_caldav_port, new_caldav_port)) 
		   ||  strcmp (old_url->user, new_url->user) 
		   || strcmp (old_use_ssl, new_use_ssl)) {
			
			account_removed (account_listener, account);
			account_added (account_listener, account);
		} else if (strcmp (existing_account_info->name, account->name)) {
			
			modify_esource ("/apps/evolution/calendar/sources", existing_account_info, account->name, new_url);
			
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
camel_hula_listener_construct (CamelHulaListener *config_listener)
{
	EIterator *iter;
	EAccount *account;
	HulaAccountInfo *info ;
	
       	config_listener->priv->account_list = e_account_list_new (config_listener->priv->gconf_client);

	for ( iter = e_list_get_iterator (E_LIST ( config_listener->priv->account_list) ) ; e_iterator_is_valid (iter); e_iterator_next (iter) ) {
		
		account = E_ACCOUNT (e_iterator_get (iter));

		if ( is_hula_account (account) && account->enabled) {
			
		        info = g_new0 (HulaAccountInfo, 1);
			info->uid = g_strdup (account->uid);
			info->name = g_strdup (account->name);
			info->source_url = g_strdup (account->source->url);
			hula_accounts = g_list_append (hula_accounts, info);
			
		}
			
	}
	g_signal_connect (config_listener->priv->account_list, "account_added", G_CALLBACK (account_added), NULL);
	g_signal_connect (config_listener->priv->account_list, "account_changed", G_CALLBACK (account_changed), NULL);
	g_signal_connect (config_listener->priv->account_list, "account_removed", G_CALLBACK (account_removed), NULL);    
}

GType
camel_hula_listener_get_type (void)
{
	static GType camel_hula_listener_type  = 0;

	if (!camel_hula_listener_type) {
		static GTypeInfo info = {
                        sizeof (CamelHulaListenerClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) camel_hula_listener_class_init,
                        NULL, NULL,
                        sizeof (CamelHulaListener),
                        0,
                        (GInstanceInitFunc) camel_hula_listener_init
                };
		camel_hula_listener_type = g_type_register_static (PARENT_TYPE, "CamelHulaListener", &info, 0);
	}

	return camel_hula_listener_type;
}

CamelHulaListener*
camel_hula_listener_new ()
{
	CamelHulaListener *config_listener;
       
	config_listener = g_object_new (CAMEL_TYPE_HULA_LISTENER, NULL);
	config_listener->priv->gconf_client = gconf_client_get_default();
	
	camel_hula_listener_construct (config_listener);

	return config_listener;
}
