/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Praveen Kumar <kpraveen@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <string.h>
#include <glib/gi18n.h>

#include "exchange-operations.h"
#include <e-folder-exchange.h>
#include <exchange-hierarchy.h>
#include <e-util/e-error.h>

ExchangeConfigListener *exchange_global_config_listener=NULL;

static const gchar *error_ids[] = {
	"config-error",
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
	"account-quota-warn"
};

static void
free_exchange_listener (void)
{
	g_object_unref (exchange_global_config_listener);
}

gint
e_plugin_lib_enable (EPlugin *eplib, gint enable)
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
exchange_operations_tokenize_string (gchar **string, gchar *token, gchar delimit, guint maxsize)
{
	guint i=0;
	gchar *str=*string;
	while (*str!=delimit && *str!='\0' && i<maxsize-1) {
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
exchange_operations_cta_add_node_to_tree (GtkTreeStore *store, GtkTreeIter *parent, const gchar *ruri)
{
	GtkTreeIter iter;
	gchar *luri=(gchar *)ruri;
	gchar nodename[80];
	gchar *uri;
	gboolean status, found;

	exchange_operations_tokenize_string (&luri, nodename, '/', sizeof(nodename));

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
exchange_operations_cta_select_node_from_tree (GtkTreeStore *store, GtkTreeIter *parent, const gchar *nuri, const gchar *ruri, GtkTreeSelection *selection)
{
	gchar *luri=(gchar *)nuri;
	gchar nodename[80];
	GtkTreeIter iter;
	gboolean status;

	if (!luri)
		return;

	exchange_operations_tokenize_string (&luri, nodename, '/', sizeof(nodename));
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
	ExchangeAccountResult result;
	GSList *acclist;
	gint mode;

	acclist = exchange_config_listener_get_accounts (exchange_global_config_listener);
	/* FIXME: Need to be changed for handling multiple accounts */
	if (acclist) {
		account = acclist->data;

		exchange_config_listener_get_offline_status (exchange_global_config_listener,
							     &mode);

		if (mode == OFFLINE_MODE) {
			return account;
		}
		else if (exchange_account_get_context (account)) {
			return account;
		} else {
			/* Try authenticating */
			result = exchange_config_listener_authenticate(exchange_global_config_listener, account);
			if (result != EXCHANGE_ACCOUNT_CONNECT_SUCCESS) {
				exchange_operations_report_error (account, result);
				return NULL;
			}
			if (exchange_account_get_context (account))
				return account;
		}
	}

	return NULL;
}

void
exchange_operations_report_error (ExchangeAccount *account, ExchangeAccountResult result)
{
	gchar *error_string;
	gchar *quota_value;
	GtkWidget *widget;

	g_return_if_fail (account != NULL);

	if (result == EXCHANGE_ACCOUNT_CONNECT_SUCCESS)
		return;

	error_string = g_strconcat ("org-gnome-exchange-operations:", error_ids[result], NULL);

	switch (result) {
		case EXCHANGE_ACCOUNT_MAILBOX_NA:
			widget = e_error_new (NULL, error_string, exchange_account_get_username (account), NULL);
			break;
		case EXCHANGE_ACCOUNT_NO_MAILBOX:
			widget = e_error_new (NULL, error_string, exchange_account_get_username (account),
					      account->exchange_server, NULL);
			break;
		case EXCHANGE_ACCOUNT_RESOLVE_ERROR:
		case EXCHANGE_ACCOUNT_CONNECT_ERROR:
		case EXCHANGE_ACCOUNT_UNKNOWN_ERROR:
			widget = e_error_new (NULL, error_string, account->exchange_server, NULL);
			break;
		case EXCHANGE_ACCOUNT_QUOTA_RECIEVE_ERROR:
		case EXCHANGE_ACCOUNT_QUOTA_SEND_ERROR:
		case EXCHANGE_ACCOUNT_QUOTA_WARN:
			quota_value = g_strdup_printf ("%.2f", account->mbox_size);
			widget = e_error_new (NULL, error_string, quota_value, NULL);
			g_free (quota_value);
			break;
		default:
			widget = e_error_new (NULL, error_string, NULL);
	}
	g_signal_connect ((GtkDialog *)widget, "response", G_CALLBACK (gtk_widget_destroy), widget);
	gtk_widget_show (widget);
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
		ruri = (gchar *) e_source_peek_relative_uri (tsource->data);
		if (ruri && g_strrstr (ruri, old_path)) {
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

gboolean
is_exchange_personal_folder (ExchangeAccount *account, gchar *uri)
{
	ExchangeHierarchy *hier;
	EFolder *folder;

	folder = exchange_account_get_folder (account, uri);
	if (folder) {
		hier = e_folder_exchange_get_hierarchy (folder);
		if (hier->type != EXCHANGE_HIERARCHY_PERSONAL)
			return FALSE;
		else
			return TRUE;
	}
	return FALSE;
}
