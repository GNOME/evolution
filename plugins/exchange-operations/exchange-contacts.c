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
	static GtkWidget *lbl_pcontacts, *scrw_pcontacts, *tv_pcontacts, *vb_pcontacts;
	GtkTreeStore *ts_pcontacts;
	GtkCellRenderer *cr_contacts;
	GtkTreeViewColumn *tvc_contacts;
	GPtrArray *conlist;
	gchar *ruri, *account_name, *uri_text;
	ExchangeAccount *account;

	int i;

	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESource *source = t->source;

	if (data->old) {
		gtk_widget_destroy (vb_pcontacts);
	}

        uri_text = e_source_get_uri (source);
	if (strncmp (uri_text, "exchange", 8)) {
		g_free (uri_text);		
		return NULL;
	}

	g_free (uri_text);

	if (strcmp (e_source_peek_relative_uri (source), e_source_peek_uid (source))) {
		contacts_src_exists = TRUE;
		g_free (contacts_old_src_uri);
		contacts_old_src_uri = g_strdup (e_source_peek_relative_uri (source));
	}
	else {
		contacts_src_exists = FALSE;
		e_source_set_relative_uri (source, ""); /* FIXME: Nasty hack */
	}

	account = exchange_operations_get_exchange_account ();
	account_name = account->account_name;

	vb_pcontacts = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (data->parent), vb_pcontacts);

	/* FIXME: Take care of i18n */
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

		tmpruri = (gchar*)e_source_peek_relative_uri (t->source);
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
	ESourceGroup *group = e_source_peek_group (t->source);

	if (!strncmp (e_source_group_peek_base_uri (group), "exchange", 8)) {
		if (!strlen (e_source_peek_relative_uri (t->source))) {
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
		
	uri_text = e_source_get_uri (source);
	if (strncmp (uri_text, "exchange", 8)) {
		g_free (uri_text);
		return ;
	}	
	g_free (uri_text);

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
	else if (strcmp (e_source_peek_relative_uri (source), contacts_old_src_uri)) {
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
		/* TODO: Modify all these error messages using e_error */
	case EXCHANGE_ACCOUNT_FOLDER_OK:
		g_print ("Folder created\n");
		break;
	case EXCHANGE_ACCOUNT_FOLDER_ALREADY_EXISTS:
		g_print ("Already exists\n");
		break;
	case EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST:
		g_print ("Doesn't exists\n");
		break;
	case EXCHANGE_ACCOUNT_FOLDER_UNKNOWN_TYPE:
		g_print ("Unknown type\n");
		break;
	case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED:
		g_print ("Permission denied\n");
		break;
	case EXCHANGE_ACCOUNT_FOLDER_OFFLINE:
		g_print ("Folder offline\n");
		break;
	case EXCHANGE_ACCOUNT_FOLDER_UNSUPPORTED_OPERATION:
		g_print ("Unsupported operation\n");
		break;
	case EXCHANGE_ACCOUNT_FOLDER_GENERIC_ERROR:		
		g_print ("Generic error\n");
		break;
	}
	g_free (ruri);
	g_free (path);
	g_free (oldpath);
	g_free (contacts_old_src_uri);
	contacts_old_src_uri = NULL;
}
