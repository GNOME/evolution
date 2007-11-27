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
#include <addressbook/gui/widgets/eab-config.h>
#include <calendar/gui/e-cal-config.h>
#include <mapi/exchange-mapi-folder.h>
#include <mapi/exchange-mapi-connection.h>

/* Account Setup */
GtkWidget *org_gnome_exchange_mapi_account_setup (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean org_gnome_exchange_mapi_check_options(EPlugin *epl, EConfigHookPageCheckData *data);

/* New Addressbook/CAL */
GtkWidget *exchange_mapi_create (EPlugin *epl, EConfigHookItemFactoryData *data);

/* New Addressbook */
gboolean exchange_mapi_book_check (EPlugin *epl, EConfigHookPageCheckData *data);
void exchange_mapi_book_commit (EPlugin *epl, EConfigTarget *target);

/* New calendar/task list/memo list */
gboolean exchange_mapi_cal_check (EPlugin *epl, EConfigHookPageCheckData *data);
void exchange_mapi_cal_commit (EPlugin *epl, EConfigTarget *target);


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
	MAPIUninitialize ();
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
	printf("profpath %s\n", profpath);
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
	printf("[exchange_mapi_plugin] Ambigous name and process filling\n");
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

	g_free (profname);

	exchange_account_listener_get_folder_list ();

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

enum {
	CONTACTSNAME_COL,
	CONTACTSFID_COL,
	CONTACTSFOLDER_COL,
	NUM_COLS
};


static gboolean
check_node (GtkTreeStore *ts, ExchangeMAPIFolder *folder, GtkTreeIter *iter)
{
	mapi_id_t fid;
	gboolean status = FALSE;
	GtkTreeIter parent;
	
	gtk_tree_model_get (GTK_TREE_MODEL (ts), iter, 1, &fid, -1);
	if (fid && folder->parent_folder_id == fid) {
		/* Do something */
		GtkTreeIter node;
		gtk_tree_store_append (ts, &node, iter);		
		gtk_tree_store_set (ts, &node, 0, folder->folder_name, 1, folder->folder_id, 2, folder,-1);		
		return TRUE;
	}

	if (gtk_tree_model_iter_has_child (ts, iter)) {
		GtkTreeIter child;
		gtk_tree_model_iter_children (ts, &child, iter);
		status = check_node (ts, folder, &child);
	}

	while (gtk_tree_model_iter_next (ts, iter) && !status) {
		status = check_node (ts, folder, iter);
	}
	
	return status;
}

static void
add_to_store (GtkTreeStore *ts, ExchangeMAPIFolder *folder)
{
	GtkTreeIter iter;
	
	gtk_tree_model_get_iter_first (ts, &iter);
	if (!check_node (ts, folder, &iter)) {
		GtkTreeIter node;
		gtk_tree_store_append (ts, &node, &iter);		
		gtk_tree_store_set (ts, &node, 0, folder->folder_name, 1, folder->folder_id, -1);
		
	}
}

static void
add_folders (GSList *folders, GtkTreeStore *ts)
{
	GSList *tmp = folders;
	GtkTreeIter iter;
	char *node = _("Personal Folders");
	mapi_id_t last = 0;
	
	gtk_tree_store_append (ts, &iter, NULL);
	gtk_tree_store_set (ts, &iter, 0, node, -1);
	while (tmp) {
		ExchangeMAPIFolder *folder = tmp->data;
		printf("%s\n", folder->folder_name);
		add_to_store (ts, folder);
		tmp = tmp->next;
	}
}

static void
exchange_mapi_cursor_change (GtkTreeView *treeview, ESource *source)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	mapi_id_t pfid;
	gchar *sfid=NULL;
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_get_selected(selection, &model, &iter);

	gtk_tree_model_get (model, &iter, CONTACTSFID_COL, &pfid, -1);
	sfid = g_strdup_printf ("%016llx", pfid);
	e_source_set_property (source, "parent-fid", sfid); 
	g_free (sfid);
}

GtkWidget *
exchange_mapi_create (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	GtkWidget *vbox, *label, *scroll, *tv;
	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESource *source = t->source;
	char *uri_text;
	GtkCellRenderer *rcell;
	GtkTreeStore *ts;
	GtkTreeViewColumn *tvc;
	GtkListStore *model;
	char *acc;
	GSList *folders = exchange_account_listener_peek_folder_list ();
	int type;
	GtkWidget *parent;

	uri_text = e_source_get_uri (source);
	if (uri_text && g_ascii_strncasecmp (uri_text, "mapi", 4)) {
		return NULL;
	}

	acc = e_source_group_peek_name (e_source_peek_group (source));
	ts = gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_INT64, G_TYPE_POINTER);

	add_folders (folders, ts);
	
	vbox = gtk_vbox_new (FALSE, 6);

	if (!strcmp (data->config->id, "org.gnome.evolution.calendar.calendarProperties")) {
		int row = ((GtkTable*) data->parent)->nrows;
		gtk_table_attach (GTK_TABLE (data->parent), vbox, 0, 2, row+1, row+2, GTK_FILL|GTK_EXPAND, 0, 0, 0);
	} else if (!strcmp (data->config->id, "com.novell.evolution.addressbook.config.accountEditor")) {
		gtk_container_add (GTK_CONTAINER (data->parent), vbox);
	}

	label = gtk_label_new_with_mnemonic (_("_Location:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	
	rcell = gtk_cell_renderer_text_new ();
	tvc = gtk_tree_view_column_new_with_attributes (acc, rcell, "text", CONTACTSNAME_COL, NULL);
	tv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (ts));
	gtk_tree_view_append_column (GTK_TREE_VIEW (tv), tvc);
	g_object_set (tv,"expander-column", tvc, "headers-visible", TRUE, NULL);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (tv));
	
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	g_object_set (scroll, "height-request", 150, NULL);
	gtk_container_add (GTK_CONTAINER (scroll), tv);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), tv);
	g_signal_connect (G_OBJECT (tv), "cursor-changed", G_CALLBACK (exchange_mapi_cursor_change), t->source);
	gtk_widget_show_all (scroll);

	gtk_box_pack_start (GTK_BOX (vbox), scroll, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);
	return vbox;
}

gboolean
exchange_mapi_book_check (EPlugin *epl, EConfigHookPageCheckData *data)
{
	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESource *source = t->source;
	char *uri_text = e_source_get_uri (source);

	if (!uri_text)
		return TRUE;

	/* FIXME: Offline handling */

	/* not a MAPI account */
	if (g_ascii_strncasecmp (uri_text, "mapi", 4)) {
		g_free (uri_text);
		return TRUE;
	}

	/* does not have a parent-fid which is needed for folder creation on server */
	if (!e_source_get_property (source, "parent-fid")) {
		g_free (uri_text);
		return FALSE;
	}

	g_free (uri_text);
	return TRUE;
}

void 
exchange_mapi_book_commit (EPlugin *epl, EConfigTarget *target)
{
	EABConfigTargetSource *t = (EABConfigTargetSource *) target;
	ESource *source = t->source;
	char *uri_text, *sfid, *tmp;
	mapi_id_t fid, pfid;
	ESourceGroup *grp;
	
	uri_text = e_source_get_uri (source);
	if (uri_text && g_ascii_strncasecmp (uri_text, "mapi", 4))
		return;
	
	//FIXME: Offline handling
	sfid = e_source_get_property (source, "parent-fid");
	sscanf (sfid, "%016llx", &pfid);

	fid = exchange_mapi_create_folder (olFolderContacts, pfid, e_source_peek_name (source));
	printf("Created %016llx\n", fid);
	grp = e_source_peek_group (source);
	e_source_set_property (source, "auth", "plain/password");
	e_source_set_property (source, "auth-domain", "MAPI");
	e_source_set_property(source, "user", e_source_group_get_property (grp, "user"));
	e_source_set_property(source, "host", e_source_group_get_property (grp, "host"));
	e_source_set_property(source, "profile", e_source_group_get_property (grp, "profile"));
	e_source_set_property(source, "domain", e_source_group_get_property (grp, "domain"));
	e_source_set_relative_uri (source, g_strconcat (";",e_source_peek_name (source), NULL));

	tmp = g_strdup_printf ("%016llx", fid);
	e_source_set_property(source, "folder-id", tmp);
	g_free (tmp);
	e_source_set_property (source, "completion", "true");

	return;
}


/* New calendar/task list/memo list */
gboolean
exchange_mapi_cal_check (EPlugin *epl, EConfigHookPageCheckData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *)(data->target);
	ESource *source = t->source;
	char *uri_text = e_source_get_uri (source);

	if (!uri_text)
		return TRUE;

	/* FIXME: Offline handling */

	/* not a MAPI account */
	if (g_ascii_strncasecmp (uri_text, "mapi", 4)) {
		g_free (uri_text);
		return TRUE;
	}

	/* does not have a parent-fid which is needed for folder creation on server */
	if (!e_source_get_property (source, "parent-fid")) {
		g_free (uri_text);
		return FALSE;
	}

	g_free (uri_text);
	return TRUE;
}

void 
exchange_mapi_cal_commit (EPlugin *epl, EConfigTarget *target)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) target;
	ESource *source = t->source;
	gchar *uri_text, *tmp;
	mapi_id_t fid, pfid;
	ESourceGroup *grp;
	int type;
	const gchar *source_selection_key = NULL, *sfid;

	uri_text = e_source_get_uri (source);
	if (uri_text && g_ascii_strncasecmp (uri_text, "mapi://", 7))
		return;

	g_free (uri_text);

	switch (t->source_type) {
		case E_CAL_SOURCE_TYPE_EVENT: 
			type = olFolderCalendar; 
			source_selection_key = "/apps/evolution/calendar/display/selected_calendars";
			break;
		case E_CAL_SOURCE_TYPE_TODO: 
			type = olFolderTasks; 
			source_selection_key = "/apps/evolution/calendar/tasks/selected_tasks";
			break;
		case E_CAL_SOURCE_TYPE_JOURNAL: 
			type = olFolderNotes; 
			source_selection_key = "/apps/evolution/calendar/memos/selected_memos";
			break;
		default: 
			type = olFolderCalendar; 
			source_selection_key = "/apps/evolution/calendar/display/selected_calendars";
			break;
	}

	//FIXME: Offline handling

	sfid = e_source_get_property (source, "parent-fid");
	sscanf (sfid, "%016llx", &pfid);

	fid = exchange_mapi_create_folder (type, pfid, e_source_peek_name (source));
	printf("Created %016llx\n", fid);

	grp = e_source_peek_group (source);

	e_source_set_property (source, "auth", "1");
	e_source_set_property (source, "auth-domain", "MAPI");

	tmp = e_source_group_get_property (grp, "use_ssl");
	e_source_set_property (source, "use_ssl", tmp);
	g_free (tmp);

	tmp = e_source_group_get_property (grp, "username");
	e_source_set_property (source, "username", tmp);
	g_free (tmp);
	
	tmp = e_source_group_get_property (grp, "host");
	e_source_set_property (source, "host", tmp);
	g_free (tmp);

//	e_source_set_property (source, "offline_sync", 
//				       camel_url_get_param (url, "offline_sync") ? "1" : "0");

	tmp = e_source_group_get_property (grp, "profile");
	e_source_set_property (source, "profile", tmp);
	g_free (tmp);

	tmp = e_source_group_get_property (grp, "domain");
	e_source_set_property (source, "domain", tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("%016llx", fid);
	e_source_set_property (source, "folder-id", tmp);
	g_free (tmp);

	/* make sure we set relative uri after we set the props needed to create the relative uri */
	tmp = g_strdup_printf ("%s@%s/%s/", e_source_get_property (source, "username"), e_source_get_property (source, "host"), e_source_get_property (source, "folder-id"));
	e_source_set_relative_uri (source, tmp);
	g_free (tmp);

	return;
}


