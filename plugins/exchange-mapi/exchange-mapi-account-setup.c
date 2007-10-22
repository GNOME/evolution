/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Srinivasa Ragavan <sragavan@novell.com>
 *  Johnny Jacob  <jjohnny@novell.com>
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
#include "config.h"
#endif

#include <string.h>
#include <unistd.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <camel/camel-provider.h>
#include <camel/camel-url.h>
#include <camel/camel-service.h>
#include <camel/camel-folder.h>
#include <libedataserver/e-xml-hash-utils.h>
#include <libedataserverui/e-passwords.h>
#include <libedataserver/e-account.h>
#include <e-util/e-dialog-utils.h>
#include <libmapi/libmapi.h>
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "exchange-account-listener.h"


GtkWidget *org_gnome_exchange_mapi_account_setup (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean org_gnome_exchange_mapi_check_options(EPlugin *epl, EConfigHookPageCheckData *data);

#define DEFAULT_PROF_PATH ".evolution/mapi-profiles.ldb"
#define E_PASSWORD_COMPONENT "ExchangeMAPI"

static void  validate_credentials (GtkWidget *widget, EConfig *config);

static ExchangeAccountListener *config_listener = NULL;

static void 
free_mapi_listener ( void )
{
	g_object_unref (config_listener);
}

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	printf("Loading Exchange MAPI Plugin\n");
	if (!config_listener) {
		config_listener = exchange_account_listener_new ();	
	 	g_atexit ( free_mapi_listener );
	}

	return 0;
}

static gboolean 
create_profile(const char *username, const char *password, const char *domain, const char *server)
{
	enum MAPISTATUS	retval;
	enum MAPISTATUS status;
	gchar *workstation;
	gchar *profname = NULL, *profpath = NULL;
	struct mapi_session *session = NULL;

	printf("Create profile with %s %s (****) %s %s\n", username, password, domain, server);
	
	workstation = "localhost";

	profpath = g_build_filename (g_getenv("HOME"), DEFAULT_PROF_PATH, NULL);
	if (!g_file_test (profpath, G_FILE_TEST_EXISTS)) {
		/* Create MAPI Profile */
		//FIXME: Get the PATH from Makefile
		if (CreateProfileStore (profpath, "/usr/local/samba/share/setup") != MAPI_E_SUCCESS) {
			g_warning ("Profile Database creation failed\n");
			g_free (profpath);
			return FALSE;
		}
	}

	if (MAPIInitialize(profpath) != MAPI_E_SUCCESS){
		status = GetLastError();
		if (status == MAPI_E_SESSION_LIMIT){
			printf("%s(%d):%s:%s \n", __FILE__, __LINE__, __PRETTY_FUNCTION__, "[exchange_mapi_plugin] Still connected");
			mapi_errstr("MAPIInitialize", GetLastError());
		} else {
 			g_free(profpath);
  			return FALSE;
 		}
	}

	printf("[exchange_mapi_plugin] Profile creation");

	profname = g_strdup_printf("%s@%s", username, domain);
	while(CreateProfile(profname, username, password, 0) == -1){
		retval = GetLastError();
		if(retval ==  MAPI_E_NO_ACCESS){
			printf("[exchange_mapi_plugin] The profile alderly exist !. Deleting it and will recreate");
			if (DeleteProfile(profname) == -1) {
				retval = GetLastError();
				mapi_errstr("[exchange_mapi_plugin] DeleteProfile() ", retval);
				return FALSE;
			}
		}
	}
	
	mapi_profile_add_string_attr(profname, "binding", server);
	mapi_profile_add_string_attr(profname, "workstation", workstation);
	mapi_profile_add_string_attr(profname, "domain", domain);
	
	/* This is only convenient here and should be replaced at some point */
	mapi_profile_add_string_attr(profname, "codepage", "0x4e4");
	mapi_profile_add_string_attr(profname, "language", "0x40c");
	mapi_profile_add_string_attr(profname, "method", "0x409");
	
	
	/* Login now */
	printf("Logging into the server\n");
	if (MapiLogonProvider(&session, profname, NULL, PROVIDER_ID_NSPI) == -1){
		retval = GetLastError();
		mapi_errstr("[exchange_mapi_plugin] Error ", retval);
		if (retval == MAPI_E_NETWORK_ERROR){
			printf("Network error\n");
			return FALSE;
		}
		if (retval == MAPI_E_LOGON_FAILED){
			printf("LOGIN Failed\n");
			return FALSE;
		}
		printf("Generic error\n");
		return FALSE;
	}


	
	printf("Login succeeded: Yeh\n");
	printf("[exchange_mapi_plugin] Ambigous name and process filling");
	if (ProcessNetworkProfile(session, username, NULL, NULL) == -1){
		retval = GetLastError();
		mapi_errstr("[exchange_mapi_plugin] : ProcessNetworkProfile", retval);
		if (retval != MAPI_E_SUCCESS && retval != 0x1){
			mapi_errstr("[exchange_mapi_plugin] ProcessNetworkProfile() ", retval);
			if (retval == MAPI_E_NOT_FOUND){
				printf("Bad user\n");
			}
			if (DeleteProfile(profname) == -1){
				retval = GetLastError();
				mapi_errstr("[exchange_mapi_plugin] DeleteProfile() ", retval);
			}
			return FALSE;
		}
	}
	
	if ((retval = SetDefaultProfile(profname)) != MAPI_E_SUCCESS){
		mapi_errstr("[exchange_mapi_plugin] SetDefaultProfile() ", GetLastError());
		return FALSE;
	}

	MAPIUninitialize ();
/*
	if (MAPIInitialize(profpath) != MAPI_E_SUCCESS){
		g_free(profpath);
		status = GetLastError();
		if (status == MAPI_E_SESSION_LIMIT){
			puts("[Openchange_plugin] openchange-lwmapi.c - Still connected");
			mapi_errstr("MAPIInitialize", GetLastError());

			
		}
		else 
			return NULL;
	}

	if (MapiLogonEx(&session, profname, NULL) == -1){
		retval = GetLastError();
		mapi_errstr("[Openchange_plugin] Error ", retval);
		if (retval == MAPI_E_NETWORK_ERROR){
			printf("Network error\n");
			return NULL;
		}
		if (retval == MAPI_E_LOGON_FAILED){
			printf("LOGIN Failed\n");
			return NULL;
		}
		printf("Generic error\n");
		return NULL;
	}
*/	
	g_free (profpath);

	/*Initialize a global connection */
	//FIXME: Dont get the password from profile
	if (!exchange_mapi_connection_new(profname, NULL))
			return FALSE;

	/* Fetch the folders into a global list for future use.*/
	exchange_account_fetch_folders ();
	
	g_free (profname);
	
	return TRUE;
  
}

static void
validate_credentials (GtkWidget *widget, EConfig *config)
{
	EMConfigTargetAccount *target_account = (EMConfigTargetAccount *)config->target;
	const gchar *source_url = NULL;
	CamelURL *url = NULL;
 	gchar *key = NULL;
 	gchar *password = NULL;
 	const gchar *domain_name = NULL;
 	const gchar *id_name = NULL;
 	gchar *at = NULL;
 	gchar *user = NULL;
	gboolean status;

	source_url = e_account_get_string (target_account->account, E_ACCOUNT_SOURCE_URL);

	url = camel_url_new(source_url, NULL);
	if (url && url->user == NULL) {
		id_name = e_account_get_string (target_account->account, E_ACCOUNT_ID_ADDRESS);
		if (id_name && *id_name) {
			at = strchr(id_name, '@');
			user = g_alloca(at-id_name+1);
			memcpy(user, id_name, at-id_name);
			user[at-id_name] = 0; 
			camel_url_set_user (url, user);
			printf("%s(%d):%s:user : %s \n", __FILE__, __LINE__, __PRETTY_FUNCTION__, user);
		}
	}

	domain_name = camel_url_get_param (url, "domain");
	
	key = camel_url_to_string (url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	password = e_passwords_get_password ("Exchange", key);
	if (!password) {
		gboolean remember = FALSE;
		gchar *title;
		
		title = g_strdup_printf (_("Enter Password for %s"), url->user);
		password = e_passwords_ask_password (title, E_PASSWORD_COMPONENT, key, title,
						     E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET,
						     &remember, NULL);
		g_free (title);

		if (!password) return;
	} 

	/* Yah, we have the username, password, domain and server. Lets create everything. */

	status = create_profile (url->user, password, domain_name, url->host);
	//FIXME: Is there a way to keep the session persistent than having a global variable?

	if (status) {
		/* Things are successful.*/
		char *profname = g_strdup_printf("%s@%s", url->user, domain_name);
		char *uri;
		
		camel_url_set_param(url, "profile", profname);
		uri = camel_url_to_string(url, 0);
		e_account_set_string(target_account->account, E_ACCOUNT_SOURCE_URL, uri);

		g_free (uri);
		g_free (profname);
	} else {
		e_passwords_forget_password (E_PASSWORD_COMPONENT, key);
	}

	g_free (key);
	g_free (password);
}

static void
domain_entry_changed(GtkWidget *entry, EConfig *config)
{
	const char *domain = NULL;
	CamelURL *url = NULL;
	char *url_string;
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)config->target;

	url = camel_url_new(e_account_get_string(target->account, E_ACCOUNT_SOURCE_URL), NULL);

	domain = gtk_entry_get_text((GtkEntry *)entry);
	if (domain && domain[0])
		camel_url_set_param(url, "domain", domain);
	else
		camel_url_set_param(url, "domain", NULL);

	url_string = camel_url_to_string(url, 0);
	e_account_set_string(target->account, E_ACCOUNT_SOURCE_URL, url_string);

	g_free(url_string);
	camel_url_free (url);
}


GtkWidget *
org_gnome_exchange_mapi_account_setup (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	CamelURL *url;
	const char *source_url;
	GtkWidget *hbox = NULL;

	GtkWidget *label;
	GtkWidget *domain_name;
	GtkWidget *auth_button;

	int row = 0;

	target_account = (EMConfigTargetAccount *)data->config->target;
	source_url = e_account_get_string(target_account->account, E_ACCOUNT_SOURCE_URL);
	url = camel_url_new(source_url, NULL);
	
	if (url == NULL)
		return NULL;
	
	if (!g_ascii_strcasecmp (url->protocol, "mapi")) {
		row = ((GtkTable *)data->parent)->nrows;		

		/* Domain name & Authenticate Button*/
		hbox = gtk_hbox_new (FALSE, 6);
		label = gtk_label_new_with_mnemonic (_("_Domain name:"));
		gtk_widget_show (label);

		domain_name = gtk_entry_new ();
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), domain_name);
		gtk_box_pack_start (GTK_BOX (hbox), domain_name, FALSE, FALSE, 0);
		g_signal_connect (domain_name, "changed", G_CALLBACK(domain_entry_changed), data->config);
		
		auth_button = gtk_button_new_with_mnemonic (_("_Authenticate"));
		gtk_box_pack_start (GTK_BOX (hbox), auth_button, FALSE, FALSE, 0);

		gtk_table_attach (GTK_TABLE (data->parent), label, 0, 1, row, row+1, 0, 0, 0, 0);
		gtk_widget_show_all (GTK_WIDGET (hbox));
		gtk_table_attach (GTK_TABLE (data->parent), GTK_WIDGET (hbox), 1, 2, row, row+1, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0); 

		gtk_signal_connect(GTK_OBJECT(auth_button), "clicked",  GTK_SIGNAL_FUNC(validate_credentials), data->config);
	}

	return GTK_WIDGET (hbox);
}

gboolean
org_gnome_exchange_mapi_check_options(EPlugin *epl, EConfigHookPageCheckData *data)
{
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)data->config->target;
	int status = FALSE;

	if (data->pageid != NULL && g_strcasecmp (data->pageid, "10.receive") == 0) {
		CamelURL *url = NULL;

		url = camel_url_new (e_account_get_string(target->account,  E_ACCOUNT_SOURCE_URL), NULL);
		if (url && url->protocol && !g_ascii_strcasecmp (url->protocol, "mapi")) {
			const gchar *prof = NULL;

			/* We assume that if the profile is set, then the setting is valid. */
 			prof = camel_url_get_param (url, "profile");

			if (prof && *prof)
				status = TRUE;
		} else
			status = TRUE;
		
		if (url)
			camel_url_free(url);
		
	} else
		return TRUE;
	
	return status;
}

