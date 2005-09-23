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
#include <gtk/gtk.h>
#include <e-util/e-config.h>
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-url.h>
#include <e-folder.h>
#include <exchange-account.h>

#include <libebook/e-book.h>
#include <libecal/e-cal.h>
#include <addressbook/gui/widgets/eab-config.h>

#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "e-util/e-account.h"
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


GPtrArray *e_exchange_contacts_get_contacts (void);
void e_exchange_contacts_pcontacts_on_change (GtkTreeView *treeview, ESource *source);
GtkWidget *e_exchange_contacts_pcontacts (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean e_exchange_contacts_check (EPlugin *epl, EConfigHookPageCheckData *data);
void e_exchange_contacts_commit (EPlugin *epl, EConfigTarget *target);

/* FIXME: Reconsider the prototype of this function */
GPtrArray *
e_exchange_contacts_get_contacts (void) 
{
	ExchangeAccount *account;
	GPtrArray *folder_array;
	GPtrArray *contacts_list;
	EFolder *folder;

	int i, prefix_len;
	gchar *uri_prefix, *ruri;

	account = exchange_operations_get_exchange_account ();

	uri_prefix = g_strconcat ("exchange://", account->account_filename, "/", NULL);
	prefix_len = strlen (uri_prefix);

	contacts_list = g_ptr_array_new ();
	exchange_account_rescan_tree (account);
	folder_array = exchange_account_get_folders (account);

	for (i=0; i<folder_array->len; ++i) {
		gchar *type, *tmp;
		folder = g_ptr_array_index (folder_array, i);
		type = (gchar*) e_folder_get_type_string (folder);
		if (!strcmp (type, "contacts")) {
			tmp = (gchar*) e_folder_get_physical_uri (folder);
			if (g_str_has_prefix (tmp, uri_prefix)) {
				ruri = g_strdup (tmp+prefix_len); /* ATTN: Should not free this explicitly */
				g_ptr_array_add (contacts_list, (gpointer)ruri);
			}
		}
	}

	g_free (uri_prefix);
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
	
	account = exchange_operations_get_exchange_account ();

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_get_selected(selection, &model, &iter);
	gchar *ruri;
	
	gtk_tree_model_get (model, &iter, CONTACTSRURI_COL, &ruri, -1);
	es_ruri = g_strconcat (account->account_filename, "/", ruri, NULL);
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

	int i;
	char *folder_size, *abook_name;
	const char *rel_uri;
	const char *uid;

	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESource *source = t->source;
	GtkWidget *lbl_offline_msg, *vb_offline_msg;
	char *offline_msg;
	gint offline_status;
	

	if (data->old) {
		gtk_widget_destroy (vb_pcontacts);
	}

        uri_text = e_source_get_uri (source);
	if (uri_text && strncmp (uri_text, "exchange", 8)) {
		g_free (uri_text);		
		return NULL;
	}

	g_free (uri_text);

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
		return vb_offline_msg;		
	}

	rel_uri = e_source_peek_relative_uri (source);
	uid = e_source_peek_uid (source);
	if (rel_uri && uid && (strcmp (rel_uri, uid))) {
		contacts_src_exists = TRUE;
		g_free (contacts_old_src_uri);
		contacts_old_src_uri = g_strdup (rel_uri);
	}
	else {
		contacts_src_exists = FALSE;
		e_source_set_relative_uri (source, ""); /* FIXME: Nasty hack */
	}

	account = exchange_operations_get_exchange_account ();
	account_name = account->account_name;
	hbx_size = NULL;
	if (contacts_src_exists) {
		abook_name = (char*)e_source_peek_name (source);
		model = exchange_account_folder_size_get_model (account);
		if (model)
			folder_size = g_strdup_printf ("%s KB", exchange_folder_size_get_val (model, abook_name));
		else
			folder_size = g_strdup_printf ("0 KB");

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

	for (i=0; i<conlist->len; ++i) {
		ruri = g_ptr_array_index (conlist, i);
		exchange_operations_cta_add_node_to_tree (ts_pcontacts, NULL, ruri);		
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
		int prefix_len;
		GtkTreeSelection *selection;

		tmpruri = (gchar*) rel_uri;
		uri_prefix = g_strconcat (account->account_filename, "/", NULL);
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
	
	g_ptr_array_free (conlist, TRUE);  
	return vb_pcontacts;
}

gboolean 
e_exchange_contacts_check (EPlugin *epl, EConfigHookPageCheckData *data) 
{
	/* FIXME - check pageid */
	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESourceGroup *group;
	const char *base_uri;
	const char *rel_uri;
	gint offline_status;

	rel_uri = e_source_peek_relative_uri (t->source);
	group = e_source_peek_group (t->source);
	base_uri = e_source_group_peek_base_uri (group);
	exchange_config_listener_get_offline_status (exchange_global_config_listener, 
								    &offline_status);
	if (base_uri && !strncmp (base_uri, "exchange", 8)) {
		if (offline_status == OFFLINE_MODE)
			return FALSE;
		if (rel_uri && !strlen (rel_uri)) {
			return FALSE;
		}		
	}

	return TRUE;
}

void 
e_exchange_contacts_commit (EPlugin *epl, EConfigTarget *target)
{
	EABConfigTargetSource *t = (EABConfigTargetSource *) target;
	ESource *source = t->source;
	gchar *uri_text, *gname, *gruri, *ruri, *path, *path_prefix, *oldpath=NULL;
	int prefix_len;
	ExchangeAccount *account;
	ExchangeAccountFolderResult result;
	gint offline_status;
		
	uri_text = e_source_get_uri (source);
	if (uri_text && strncmp (uri_text, "exchange", 8)) {
		g_free (uri_text);
		return ;
	}	
	g_free (uri_text);

	exchange_config_listener_get_offline_status (exchange_global_config_listener, 
								    &offline_status);
	if (offline_status == OFFLINE_MODE)
		return;

	account = exchange_operations_get_exchange_account ();
	path_prefix = g_strconcat (account->account_filename, "/", NULL);
	prefix_len = strlen (path_prefix);
	g_free (path_prefix);

	gname = (gchar*) e_source_peek_name (source);
	gruri = (gchar*) e_source_peek_relative_uri (source);
	if (contacts_src_exists) {
		gchar *tmpruri, *tmpdelimit;
		tmpruri = g_strdup (gruri);
		tmpdelimit = g_strrstr (tmpruri, "/");
		tmpdelimit[0] = '\0';
		ruri = g_strconcat (tmpruri, "/", gname, NULL);
		g_free (tmpruri);
	}
	else {
		ruri = g_strconcat (gruri, "/", gname, NULL);
	}
	e_source_set_relative_uri (source, ruri);

	path = g_strdup_printf ("/%s", ruri+prefix_len);
	
	if (!contacts_src_exists) {
		/* Create the new folder */
		result = exchange_account_create_folder (account, path, "contacts");
	}
	else if (strcmp (gruri, contacts_old_src_uri)) {
		/* Rename the folder */
		oldpath = g_strdup_printf ("/%s", contacts_old_src_uri+prefix_len);
		result = exchange_account_xfer_folder (account, oldpath, path, TRUE);
		exchange_operations_update_child_esources (source, 
							   contacts_old_src_uri, 
							   ruri);
	}
	else {
		/* Nothing happened specific to exchange; just return */
		return;
	}

	switch (result) {
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
	g_free (ruri);
	g_free (path);
	g_free (oldpath);
	g_free (contacts_old_src_uri);
	contacts_old_src_uri = NULL;
}
