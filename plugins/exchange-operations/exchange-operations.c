/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Praveen Kumar <kpraveen@novell.com>
 * Copyright (C) 2005 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <string.h>

#include "exchange-operations.h"
#include <e-util/e-error.h>

ExchangeConfigListener *exchange_global_config_listener=NULL;

static char *error_ids[] = { "config-error",
			     "password-weak-error",
			     "password-change-error",
			     "password-change-success",
			     "account-offline",
			     "password-incorrect",
			     "account-domain-error",
			     "account-mailbox-na",
			     "account-version-error",
			     "account-wss-error",
			     "account-no-mailbox",
			     "account-resolve-error",
			     "account-connect-error",
			     "password-expired",
			     "account-unknown-error",
			     "account-quota-error",
			     "account-quota-send-error",
			     "account-quota-warn" };

static void
free_exchange_listener (void)
{
	g_object_unref (exchange_global_config_listener);
}

int
e_plugin_lib_enable (EPluginLib *eplib, int enable)
{
	if (!exchange_global_config_listener) {
		exchange_global_config_listener = exchange_config_listener_new ();
		g_atexit (free_exchange_listener);
	}
	return 0;
}

ExchangeConfigListenerStatus
exchange_is_offline (gint *mode)
{	
	return exchange_config_listener_get_offline_status (exchange_global_config_listener, mode);
}	

/* FIXME: See if a GLib variant of this function available */
gboolean
exchange_operations_tokenize_string (char **string, char *token, char delimit)
{
	int i=0;
	char *str=*string;
	while (*str!=delimit && *str!='\0') {
		token[i++]=*str++;
	}
	while (*str==delimit)
		str++;
	token[i]='\0';
	*string = str;
	if (i==0) 
		return FALSE;
	return TRUE;
}

gboolean
exchange_operations_cta_add_node_to_tree (GtkTreeStore *store, GtkTreeIter *parent, const char *ruri) 
{
	GtkTreeIter iter;
	char *luri=(char *)ruri;
	char nodename[80];
	gchar *uri;
	gboolean status, found;
	
	exchange_operations_tokenize_string (&luri, nodename, '/');

       	if (!nodename[0]) {
		return TRUE;
	}

	if (!parent) {
	  uri = g_strdup (nodename);
	}
	else {
	  gchar *tmpuri;
	  gtk_tree_model_get (GTK_TREE_MODEL (store), parent, 1, &tmpuri, -1);
	  uri = g_strconcat (tmpuri, "/", nodename, NULL);
	  g_free (tmpuri);
	}

	if (!strcmp (nodename, "personal") && !parent) {
		/* FIXME: Don't hardcode this */
		strcpy (nodename, _("Personal Folders"));
	}

	found = FALSE;
	status = gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &iter, parent);
	while (status) {
	  gchar *readname;
	  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &readname, -1);
	  if (!strcmp (nodename, readname)) {
	    found = TRUE;
	    exchange_operations_cta_add_node_to_tree (store, &iter, luri);
	    g_free (readname);
	    break;
	  }
	  status = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
	}

	if (!found) {
	  gtk_tree_store_append (store, &iter, parent);		
	  gtk_tree_store_set (store, &iter, 0, nodename, 1, uri, -1);		
	  exchange_operations_cta_add_node_to_tree (store, &iter, luri);				
	}

	g_free (uri);
	return TRUE;
}

void
exchange_operations_cta_select_node_from_tree (GtkTreeStore *store, GtkTreeIter *parent, const char *nuri, const char *ruri, GtkTreeSelection *selection) 
{
	char *luri=(char *)nuri;
	char nodename[80];
	GtkTreeIter iter;
	gboolean status;

	exchange_operations_tokenize_string (&luri, nodename, '/');

       	if (!nodename[0]) {
		return;
	}

	if (!strcmp (nodename, "personal") && !parent) {
		/* FIXME: Don't hardcode this */
		strcpy (nodename, _("Personal Folders"));
	}

	status = gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &iter, parent);
	while (status) {
		gchar *readname;
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &readname, -1);
		if (!strcmp (nodename, readname)) {
			gchar *readruri;
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 1, &readruri, -1);
			if (!strcmp (ruri, readruri)) {
				gtk_tree_selection_select_iter (selection, &iter);
				return;
			}
			g_free (readname);
			g_free (readruri);
			exchange_operations_cta_select_node_from_tree (store, &iter, luri, ruri, selection);
			break;
		}
		status = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
	}
	return;
}

ExchangeAccount *
exchange_operations_get_exchange_account (void) 
{
	ExchangeAccount *account = NULL;
	GSList *acclist;

	acclist = exchange_config_listener_get_accounts (exchange_global_config_listener);
	/* FIXME: Need to be changed for handling multiple accounts */
	if (acclist)
		account = acclist->data; 
	
	return account;
}

void
exchange_operations_report_error (ExchangeAccount *account, ExchangeAccountResult result)
{
	gchar *error_string;
	gchar *quota_value;

	g_return_if_fail (account != NULL);

	if (result == EXCHANGE_ACCOUNT_CONNECT_SUCCESS)
		return;
	
	error_string = g_strconcat ("org-gnome-exchange-operations:", error_ids[result], NULL);
	
	switch (result) {
		case EXCHANGE_ACCOUNT_MAILBOX_NA:
			e_error_run (NULL, error_string, exchange_account_get_username (account), NULL);
			break;
		case EXCHANGE_ACCOUNT_NO_MAILBOX:
			e_error_run (NULL, error_string, exchange_account_get_username (account),
				     account->exchange_server, NULL);
			break;
		case EXCHANGE_ACCOUNT_RESOLVE_ERROR:
		case EXCHANGE_ACCOUNT_CONNECT_ERROR:
		case EXCHANGE_ACCOUNT_UNKNOWN_ERROR:
			e_error_run (NULL, error_string, account->exchange_server, NULL);
			break;
		case EXCHANGE_ACCOUNT_QUOTA_RECIEVE_ERROR:
		case EXCHANGE_ACCOUNT_QUOTA_SEND_ERROR:
		case EXCHANGE_ACCOUNT_QUOTA_WARN:
			quota_value = g_strdup_printf ("%d", exchange_account_get_quota_limit (account));
			e_error_run (NULL, error_string, quota_value, NULL);
			g_free (quota_value);
			break;
		default:
			e_error_run (NULL, error_string, NULL);
	}
	g_free (error_string);
}

void exchange_operations_update_child_esources (ESource *source, const gchar *old_path, const gchar *new_path)
{
	ESourceGroup *group;
	GSList *sources, *tsource;
	group = e_source_peek_group (source);
	sources = e_source_group_peek_sources (group);
	for (tsource = sources; tsource != NULL; tsource = tsource->next) {
		gchar *ruri;
		ruri = e_source_peek_relative_uri (tsource->data);
		if (g_strrstr (ruri, old_path)) {
			/* This ESource points to one of the child folders */
			gchar **tmpv, *truri;
			/* A nasty search and replace */
		  	tmpv = g_strsplit (ruri, old_path, -1);
			truri = g_strjoinv (new_path, tmpv);
			e_source_set_relative_uri (tsource->data, truri);
			g_strfreev (tmpv);
			g_free (truri);	
		}
	}
}
