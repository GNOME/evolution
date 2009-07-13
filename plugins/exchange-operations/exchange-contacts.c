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
#include <gtk/gtk.h>
#include <e-util/e-config.h>
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-url.h>
#include <e-folder.h>
#include <exchange-account.h>

#include <libebook/e-book.h>
#include <libecal/e-cal.h>
#include <addressbook/gui/widgets/eab-config.h>

#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "libedataserver/e-account.h"
#include "e-util/e-error.h"

#include "exchange-operations.h"
#include "exchange-folder-size-display.h"

enum {
	CONTACTSNAME_COL,
	CONTACTSRURI_COL,
	NUM_COLS
};

gboolean contacts_src_exists = FALSE;
gchar *contacts_old_src_uri = NULL;

static GPtrArray *e_exchange_contacts_get_contacts (void);
void e_exchange_contacts_pcontacts_on_change (GtkTreeView *treeview, ESource *source);
GtkWidget *e_exchange_contacts_pcontacts (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean e_exchange_contacts_check (EPlugin *epl, EConfigHookPageCheckData *data);
void e_exchange_contacts_commit (EPlugin *epl, EConfigTarget *target);

/* FIXME: Reconsider the prototype of this function */
static GPtrArray *
e_exchange_contacts_get_contacts (void)
{
	ExchangeAccount *account;
	GPtrArray *folder_array;
	GPtrArray *contacts_list;
	EFolder *folder;

	gint i, prefix_len;
	gchar *uri_prefix, *ruri;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return NULL;

	uri_prefix = g_strconcat ("exchange://", account->account_filename, "/;", NULL);
	prefix_len = strlen (uri_prefix);

	contacts_list = g_ptr_array_new ();
	exchange_account_rescan_tree (account);
	folder_array = exchange_account_get_folders (account);

	for (i=0; i<folder_array->len; ++i) {
		gchar *type, *tmp;
		folder = g_ptr_array_index (folder_array, i);
		type = (gchar *) e_folder_get_type_string (folder);
		if (!strcmp (type, "contacts")) {
			tmp = (gchar *) e_folder_get_physical_uri (folder);
			if (g_str_has_prefix (tmp, uri_prefix)) {
				ruri = g_strdup (tmp+prefix_len);
				g_ptr_array_add (contacts_list, ruri);
			}
		}
	}

	g_free (uri_prefix);
	if (folder_array)
		g_ptr_array_free (folder_array, TRUE);
	return contacts_list;
}

void
e_exchange_contacts_pcontacts_on_change (GtkTreeView *treeview, ESource *source)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	ExchangeAccount *account;
	gchar *es_ruri;
	gchar *ruri;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_get_selected(selection, &model, &iter);

	gtk_tree_model_get (model, &iter, CONTACTSRURI_COL, &ruri, -1);
	es_ruri = g_strconcat (account->account_filename, "/;", ruri, NULL);
	e_source_set_relative_uri (source, es_ruri);

	g_free (ruri);
	g_free (es_ruri);
}

GtkWidget *
e_exchange_contacts_pcontacts (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *lbl_pcontacts, *scrw_pcontacts, *tv_pcontacts, *vb_pcontacts, *lbl_size, *lbl_size_val, *hbx_size;
	GtkTreeStore *ts_pcontacts;
	GtkCellRenderer *cr_contacts;
	GtkTreeViewColumn *tvc_contacts;
	GtkListStore *model;
	GPtrArray *conlist;
	gchar *ruri, *account_name, *uri_text;
	ExchangeAccount *account;
	gint i;
	gchar *folder_size, *abook_name;
	const gchar *rel_uri;
	const gchar *uid;
	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESource *source = t->source;
	GtkWidget *lbl_offline_msg, *vb_offline_msg;
	gchar *offline_msg;
	gint offline_status;
	gboolean gal_folder = FALSE, is_personal;

	if (data->old) {
		gtk_widget_destroy (vb_pcontacts);
	}

	uri_text = e_source_get_uri (source);
	if (uri_text && g_ascii_strncasecmp (uri_text, "exchange", 8)) {
		if (g_ascii_strncasecmp (uri_text, "gal", 3)) {
			g_free (uri_text);
			return NULL;
		}
		else {
			gal_folder = TRUE;
		}
	}

	exchange_config_listener_get_offline_status (exchange_global_config_listener,
								    &offline_status);
	if (offline_status == OFFLINE_MODE) {
		/* Evolution is in offline mode; we will not be able to create
		   new folders or modify existing folders. */
		offline_msg = g_markup_printf_escaped ("<b>%s</b>",
						       _("Evolution is in offline mode. You cannot create or modify folders now.\nPlease switch to online mode for such operations."));
		vb_offline_msg = gtk_vbox_new (FALSE, 6);
		gtk_container_add (GTK_CONTAINER (data->parent), vb_offline_msg);
		lbl_offline_msg = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (lbl_offline_msg), offline_msg);
		g_free (offline_msg);
		gtk_box_pack_start (GTK_BOX (vb_offline_msg), lbl_offline_msg, FALSE, FALSE, 0);
		gtk_widget_show_all (vb_offline_msg);
		g_free (uri_text);
		return vb_offline_msg;
	}

	if (gal_folder) {
		contacts_src_exists = TRUE;
		g_free (uri_text);
		return NULL;
	}

	rel_uri = e_source_peek_relative_uri (source);
	uid = e_source_peek_uid (source);
	if (rel_uri && uid && (strcmp (rel_uri, uid))) {
		contacts_src_exists = TRUE;
		g_free (contacts_old_src_uri);
		contacts_old_src_uri = g_strdup (rel_uri);
	}
	else {
		/* new folder */
		contacts_src_exists = FALSE;
		e_source_set_relative_uri (source, ""); /* FIXME: Nasty hack */
	}

	account = exchange_operations_get_exchange_account ();
	if (!account) {
		g_free (contacts_old_src_uri);
		g_free (uri_text);
		return NULL;
	}
	account_name = account->account_name;
	hbx_size = NULL;

	is_personal = is_exchange_personal_folder (account, uri_text);
	g_free (uri_text);

	if (contacts_src_exists && is_personal ) {
		abook_name = (gchar *)e_source_peek_name (source);
		model = exchange_account_folder_size_get_model (account);
		if (model)
			folder_size = g_strdup_printf (_("%s KB"), exchange_folder_size_get_val (model, abook_name));
		else
			folder_size = g_strdup_printf (_("0 KB"));

		/* FIXME: Take care of i18n */
		lbl_size = gtk_label_new_with_mnemonic (_("Size:"));
		lbl_size_val = gtk_label_new_with_mnemonic (_(folder_size));
		hbx_size = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbx_size), lbl_size, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (hbx_size), lbl_size_val, FALSE, TRUE, 10);
		gtk_widget_show (lbl_size);
		gtk_widget_show (lbl_size_val);
		gtk_misc_set_alignment (GTK_MISC (lbl_size), 0.0, 0.5);
		gtk_misc_set_alignment (GTK_MISC (lbl_size_val), 0.0, 0.5);
		g_free (folder_size);
	}
	vb_pcontacts = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (data->parent), vb_pcontacts);

	if (hbx_size)
		gtk_box_pack_start (GTK_BOX (vb_pcontacts), hbx_size, FALSE, FALSE, 0);

	lbl_pcontacts = gtk_label_new_with_mnemonic (_("_Location:"));
	gtk_widget_show (lbl_pcontacts);
	gtk_misc_set_alignment (GTK_MISC (lbl_pcontacts), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vb_pcontacts), lbl_pcontacts, FALSE, FALSE, 0);

	ts_pcontacts = gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING);

	conlist = e_exchange_contacts_get_contacts ();

	if (conlist) {
		for (i = 0; i < conlist->len; i++) {
			ruri = g_ptr_array_index (conlist, i);
			exchange_operations_cta_add_node_to_tree (ts_pcontacts, NULL, ruri);
		}
		g_ptr_array_free (conlist, TRUE);
	}

	cr_contacts = gtk_cell_renderer_text_new ();
	tvc_contacts = gtk_tree_view_column_new_with_attributes (account_name, cr_contacts, "text", CONTACTSNAME_COL, NULL);
	tv_pcontacts = gtk_tree_view_new_with_model (GTK_TREE_MODEL (ts_pcontacts));
	gtk_tree_view_append_column (GTK_TREE_VIEW (tv_pcontacts), tvc_contacts);
	g_object_set (tv_pcontacts,"expander-column", tvc_contacts, "headers-visible", TRUE, NULL);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (tv_pcontacts));

	scrw_pcontacts = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrw_pcontacts), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrw_pcontacts), GTK_SHADOW_IN);
	g_object_set (scrw_pcontacts, "height-request", 150, NULL);
	gtk_container_add (GTK_CONTAINER (scrw_pcontacts), tv_pcontacts);
	gtk_label_set_mnemonic_widget (GTK_LABEL (lbl_pcontacts), tv_pcontacts);
	g_signal_connect (G_OBJECT (tv_pcontacts), "cursor-changed", G_CALLBACK (e_exchange_contacts_pcontacts_on_change), t->source);
	gtk_widget_show_all (scrw_pcontacts);

	gtk_box_pack_start (GTK_BOX (vb_pcontacts), scrw_pcontacts, FALSE, FALSE, 0);
	gtk_widget_show_all (vb_pcontacts);

	if (contacts_src_exists) {
		gchar *uri_prefix, *sruri, *tmpruri;
		gint prefix_len;
		GtkTreeSelection *selection;

		tmpruri = (gchar *) rel_uri;
		uri_prefix = g_strconcat (account->account_filename, "/;", NULL);
		prefix_len = strlen (uri_prefix);

		if (g_str_has_prefix (tmpruri, uri_prefix)) {
			sruri = g_strdup (tmpruri+prefix_len);
		}
		else {
			sruri = NULL;
		}

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv_pcontacts));
		exchange_operations_cta_select_node_from_tree (ts_pcontacts,
							       NULL,
							       sruri,
							       sruri,
							       selection);
		gtk_widget_set_sensitive (tv_pcontacts, FALSE);

		g_free (uri_prefix);
		g_free (sruri);
	}

	g_object_unref (ts_pcontacts);
	return vb_pcontacts;
}

gboolean
e_exchange_contacts_check (EPlugin *epl, EConfigHookPageCheckData *data)
{
	/* FIXME - check pageid */
	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESourceGroup *group;
	const gchar *base_uri;
	const gchar *rel_uri;
	gint offline_status;
	ExchangeAccount *account;

	rel_uri = e_source_peek_relative_uri (t->source);
	group = e_source_peek_group (t->source);
	base_uri = e_source_group_peek_base_uri (group);
	exchange_config_listener_get_offline_status (exchange_global_config_listener,
								    &offline_status);
	if (base_uri && !g_ascii_strncasecmp (base_uri, "exchange", 8)) {
		if (offline_status == OFFLINE_MODE)
			return FALSE;
		if (rel_uri && !strlen (rel_uri))
			return FALSE;
	}
	else {
		return TRUE;
	}

	if (!contacts_src_exists) {
		return TRUE;
	}

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return FALSE;

	if (!rel_uri) {
		GConfClient *client;
		ESourceList *source_list = NULL;
		ESourceGroup *source_group = NULL;
		ESource *source;
		EAccount *eaccount;

		/* GAL folder */
		client = gconf_client_get_default ();
		source_list = e_source_list_new_for_gconf ( client, CONF_KEY_CONTACTS);
		g_object_unref (client);

		eaccount = exchange_account_fetch (account);
		g_return_val_if_fail (eaccount != NULL, FALSE);
		g_return_val_if_fail (eaccount->uid != NULL, FALSE);

		if ((source_group = e_source_list_peek_group_by_properties (source_list, "account-uid", eaccount->uid, NULL))) {
			source =  e_source_group_peek_source_by_name (source_group, e_source_peek_name (t->source));
			if (e_source_group_peek_source_by_name (source_group,
							e_source_peek_name (t->source))) {
				/* not a rename of GAL */
				g_object_unref (source_list);
				return TRUE;
			}
			else {
				g_object_unref (source_list);
				return FALSE;
			}
		}
		else {
			g_object_unref (source_list);
			return FALSE;
		}
	}
	else {
		EUri *euri;
		gint uri_len;
		gchar *uri_text, *uri_string, *path, *folder_name;
		gboolean is_personal;

		uri_text = e_source_get_uri (t->source);
		euri = e_uri_new (uri_text);
		uri_string = e_uri_to_string (euri, FALSE);
		e_uri_free (euri);

		is_personal = is_exchange_personal_folder (account, uri_text);

		uri_len = strlen (uri_string) + 1;
		g_free (uri_string);
		path = g_build_filename ("/", uri_text + uri_len, NULL);
		g_free (uri_text);
		folder_name = g_strdup (g_strrstr (path, "/") +1);
		g_free (path);

		if (strcmp (folder_name, e_source_peek_name (t->source))) {
			/* rename */
			if (exchange_account_get_standard_uri (account, folder_name) ||
			    !is_personal) {
				/* rename of standard/non-personal folder */
				g_free (folder_name);
				return FALSE;
			}
			g_free (folder_name);
		}
	}
	return TRUE;
}

void
e_exchange_contacts_commit (EPlugin *epl, EConfigTarget *target)
{
	EABConfigTargetSource *t = (EABConfigTargetSource *) target;
	ESource *source = t->source;
	gchar *uri_text, *gname, *gruri, *ruri = NULL, *path = NULL, *path_prefix, *oldpath=NULL;
	gchar *username, *windows_domain, *authtype;
	gint prefix_len;
	ExchangeAccount *account;
	ExchangeAccountFolderResult result;
	gint offline_status;
	gboolean rename = FALSE;

	uri_text = e_source_get_uri (source);
	if (uri_text && strncmp (uri_text, "exchange", 8)) {
		/* here no need of checking for gal */
		g_free (uri_text);
		return;
	}

	exchange_config_listener_get_offline_status (exchange_global_config_listener,
								    &offline_status);
	if (offline_status == OFFLINE_MODE) {
		g_free (uri_text);
		return;
	}

	account = exchange_operations_get_exchange_account ();
	if (!account || !is_exchange_personal_folder (account, uri_text))
		return;

	windows_domain = exchange_account_get_windows_domain (account);
	if (windows_domain)
		username = g_strdup_printf ("%s\\%s", windows_domain,
					    exchange_account_get_username (account));
	else
		username = g_strdup (exchange_account_get_username (account));

	authtype = exchange_account_get_authtype (account);

	path_prefix = g_strconcat (account->account_filename, "/;", NULL);
	prefix_len = strlen (path_prefix);
	g_free (path_prefix);

	gname = (gchar *) e_source_peek_name (source);
	gruri = (gchar *) e_source_peek_relative_uri (source);

	if (contacts_src_exists) {
		gchar *tmpruri, *uri_string, *temp_path, *prefix;
		EUri *euri;
		gint uri_len;

		euri = e_uri_new (uri_text);
		uri_string = e_uri_to_string (euri, FALSE);
		e_uri_free (euri);

		uri_len = strlen (uri_string) + 1;
		tmpruri = g_strdup (uri_string + strlen ("exchange://"));
		temp_path = g_build_filename ("/", uri_text + uri_len, NULL);
		prefix	= g_strndup (temp_path, strlen (temp_path) - strlen (g_strrstr (temp_path, "/")));
		g_free (temp_path);
		path = g_build_filename (prefix, "/", gname, NULL);
		ruri = g_strconcat (tmpruri, ";", path+1, NULL);
		oldpath = g_build_filename ("/", contacts_old_src_uri + prefix_len, NULL);
		g_free (prefix);
		g_free (uri_string);
		g_free (tmpruri);
	}
	else {
		/* new folder */
		ruri = g_strconcat (gruri, "/", gname, NULL);
		path = g_build_filename ("/", ruri+prefix_len, NULL);
	}

	if (!contacts_src_exists) {
		/* Create the new folder */
		result = exchange_account_create_folder (account, path, "contacts");
	}
	else if (gruri && strcmp (path, oldpath )) {
		/* Rename the folder */
		rename = TRUE;
		result = exchange_account_xfer_folder (account, oldpath, path, TRUE);
	}
	else {
		/* Nothing happened specific to exchange; just return */
		goto done;
	}
	switch (result) {
	case EXCHANGE_ACCOUNT_FOLDER_OK:
		e_source_set_name (source, gname);
		e_source_set_relative_uri (source, ruri);
		e_source_set_property (source, "username", username);
		e_source_set_property (source, "auth-domain", "Exchange");
		if (authtype) {
			e_source_set_property (source, "auth-type", authtype);
			g_free (authtype);
			authtype=NULL;
		}
		e_source_set_property (source, "auth", "plain/password");
		if (rename) {
			exchange_operations_update_child_esources (source,
							contacts_old_src_uri,
							ruri);
		}
		break;
	case EXCHANGE_ACCOUNT_FOLDER_ALREADY_EXISTS:
		e_error_run (NULL, ERROR_DOMAIN ":folder-exists-error", NULL);
		break;
	case EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST:
		e_error_run (NULL, ERROR_DOMAIN ":folder-doesnt-exist-error", NULL);
		break;
	case EXCHANGE_ACCOUNT_FOLDER_UNKNOWN_TYPE:
		e_error_run (NULL, ERROR_DOMAIN ":folder-unknown-type", NULL);
		break;
	case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED:
		e_error_run (NULL, ERROR_DOMAIN ":folder-perm-error", NULL);
		break;
	case EXCHANGE_ACCOUNT_FOLDER_OFFLINE:
		e_error_run (NULL, ERROR_DOMAIN ":folder-offline-error", NULL);
		break;
	case EXCHANGE_ACCOUNT_FOLDER_UNSUPPORTED_OPERATION:
		e_error_run (NULL, ERROR_DOMAIN ":folder-unsupported-error", NULL);
		break;
	case EXCHANGE_ACCOUNT_FOLDER_GENERIC_ERROR:
		e_error_run (NULL, ERROR_DOMAIN ":folder-generic-error", NULL);
		break;
	default:
		break;
	}
done:
	g_free (ruri);
	g_free (username);
	if (authtype)
		g_free (authtype);
	g_free (path);
	g_free (oldpath);
	g_free (contacts_old_src_uri);
	g_free (uri_text);
	contacts_old_src_uri = NULL;
}
