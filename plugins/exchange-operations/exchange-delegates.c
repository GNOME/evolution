/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2002-2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* ExchangeDelegates: Exchange delegate handling.
 *
 * FIXME: make this instant-apply
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "exchange-delegates.h"
#include "exchange-delegates-user.h"
#include "exchange-user-dialog.h"
#include "exchange-operations.h"

#include <exchange-account.h>
#include <e2k-propnames.h>
#include <e2k-security-descriptor.h>
#include <e2k-sid.h>
#include <e2k-uri.h>
#include <e2k-utils.h>

#include <e-util/e-dialog-utils.h>
#include <e-util/e-error.h>
#include <glade/glade-xml.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>

typedef struct {
	const char *uri;
	E2kSecurityDescriptor *sd;
	gboolean changed;
} ExchangeDelegatesFolder;

typedef struct {
	ExchangeAccount *account;
	char *self_dn;

	GladeXML *xml;
	GtkWidget *dialog, *parent;

	GtkListStore *model;
	GtkWidget *table;

	GByteArray *creator_entryid;
	GPtrArray *users, *added_users, *removed_users;
	gboolean loaded_folders;
	ExchangeDelegatesFolder folder[EXCHANGE_DELEGATES_LAST];
	ExchangeDelegatesFolder freebusy_folder;
} ExchangeDelegates;

extern const char *exchange_delegates_user_folder_names[];

const char *exchange_localfreebusy_path = "NON_IPM_SUBTREE/Freebusy%20Data/LocalFreebusy.EML";

static void set_perms_for_user (ExchangeDelegatesUser *user, gpointer user_data);

static void
set_sd_for_href (ExchangeDelegates *delegates,
		 const char *href,
		 E2kSecurityDescriptor *sd)
{
	int i;

	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		if (!delegates->folder[i].uri)
			continue;

		if (!strcmp (href, delegates->folder[i].uri)) {
			delegates->folder[i].sd = sd;
			return;
		}
	}

	/* else, it's the freebusy folder */
	delegates->freebusy_folder.uri = g_strdup (href);
	delegates->freebusy_folder.sd = sd;
}

/* Given an array of ExchangeDelegatesUser containing display names
 * and entryids, and an array of E2kSecurityDescriptors containing
 * SIDs (which contain display names), add SIDs to the delegates. In
 * the easy case, we can just match the SIDs up with their
 * corresponding user by display name. However, there are two things
 * that can go wrong:
 *
 *   1. Some users may have been removed from the SDs
 *   2. Two users may have the same display name
 *
 * In both cases, we fall back to using the GC.
 */
static gboolean
fill_in_sids (ExchangeDelegates *delegates)
{
	int u, u2, sd, needed_sids;
	ExchangeDelegatesUser *user, *user2;
	GList *sids, *s;
	E2kSid *sid;
	E2kGlobalCatalog *gc;
	E2kGlobalCatalogStatus status;
	E2kGlobalCatalogEntry *entry;
	gboolean ok = TRUE;

	needed_sids = 0;

	/* Mark users with duplicate names and count the number of
	 * non-duplicate names.
	 */
	for (u = 0; u < delegates->users->len; u++) {
		user = delegates->users->pdata[u];
		if (user->sid == (E2kSid *)-1)
			continue;
		for (u2 = u + 1; u2 < delegates->users->len; u2++) {
			user2 = delegates->users->pdata[u2];
			if (!strcmp (user->display_name, user2->display_name))
				user->sid = user2->sid = (E2kSid *)-1;
		}
		if (!user->sid)
			needed_sids++;
	}

	/* Scan security descriptors trying to match SIDs until we're
	 * not expecting to find any more.
	 */
	for (sd = 0; sd < EXCHANGE_DELEGATES_LAST && needed_sids; sd++) {
		sids = e2k_security_descriptor_get_sids (delegates->folder[sd].sd);
		for (s = sids; s && needed_sids; s = s->next) {
			sid = s->data;
			for (u = 0; u < delegates->users->len; u++) {
				user = delegates->users->pdata[u];
				if (user->sid)
					continue;
				if (!strcmp (user->display_name,
					     e2k_sid_get_display_name (sid))) {
					user->sid = sid;
					g_object_ref (sid);
					needed_sids--;
				}
			}
		}
		g_list_free (sids);
	}

	/* Now for each user whose SID hasn't yet been found, look it up. */
	gc = exchange_account_get_global_catalog (delegates->account);
	for (u = 0; u < delegates->users->len; u++) {
		user = delegates->users->pdata[u];
		if (user->sid && user->sid != (E2kSid *)-1)
			continue;

		status = e2k_global_catalog_lookup (
			gc, NULL, /* FIXME: cancellable */
			E2K_GLOBAL_CATALOG_LOOKUP_BY_LEGACY_EXCHANGE_DN,
			e2k_entryid_to_dn (user->entryid),
			E2K_GLOBAL_CATALOG_LOOKUP_SID, &entry);
		if (status != E2K_GLOBAL_CATALOG_OK) {
			user->sid = NULL;
			ok = FALSE;
			continue;
		}
		user->sid = entry->sid;
		g_object_ref (user->sid);
		e2k_global_catalog_entry_free (gc, entry);
	}

	return ok;
}

static const char *sd_props[] = {
	E2K_PR_EXCHANGE_SD_BINARY,
	E2K_PR_EXCHANGE_SD_XML
};
static const int n_sd_props = sizeof (sd_props) / sizeof (sd_props[0]);

/* Read the folder security descriptors and match them up with the
 * list of delegates.
 */
static gboolean
get_folder_security (ExchangeDelegates *delegates)
{
	GPtrArray *hrefs;
	E2kContext *ctx;
	E2kHTTPStatus status;
	E2kResultIter *iter;
	E2kResult *result;
	xmlNode *xml_form;
	GByteArray *binary_form;
	ExchangeDelegatesUser *user;
	guint32 perms;
	int i, u;

	/* If we've been here before, just return the success or
	 * failure result from last time.
	 */
	if (delegates->freebusy_folder.uri)
		return delegates->loaded_folders;

	if (!exchange_account_get_global_catalog (delegates->account)) {
		e_error_run (GTK_WINDOW (delegates->table), ERROR_DOMAIN ":delegates-no-gcs-error", 
			     NULL);
		return FALSE;
	}

	ctx = exchange_account_get_context (delegates->account);

	hrefs = g_ptr_array_new ();
	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		delegates->folder[i].uri = exchange_account_get_standard_uri (
			delegates->account, exchange_delegates_user_folder_names[i]);
		if (delegates->folder[i].uri) {
			g_ptr_array_add (hrefs, (char *)e2k_uri_relative (
						 delegates->account->home_uri,
						 delegates->folder[i].uri));
		}
	}
	g_ptr_array_add (hrefs, (char *)exchange_localfreebusy_path);

	iter = e2k_context_bpropfind_start (
		ctx, NULL, delegates->account->home_uri,
		(const char **)hrefs->pdata, hrefs->len,
		sd_props, n_sd_props);
	g_ptr_array_free (hrefs, TRUE);

	while ((result = e2k_result_iter_next (iter))) {
		xml_form = e2k_properties_get_prop (result->props,
						    E2K_PR_EXCHANGE_SD_XML);
		binary_form = e2k_properties_get_prop (result->props,
						       E2K_PR_EXCHANGE_SD_BINARY);

		if (xml_form && binary_form) {
			set_sd_for_href (delegates, result->href,
					 e2k_security_descriptor_new (xml_form, binary_form));
		}
	}
	status = e2k_result_iter_free (iter);

	if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
		e_error_run (GTK_WINDOW (delegates->table), ERROR_DOMAIN ":delegates-perm-read-error", 
			     NULL);
		return FALSE;
	}

	if (!fill_in_sids (delegates)) {
		delegates->loaded_folders = FALSE;
		e_error_run (GTK_WINDOW (delegates->table), ERROR_DOMAIN ":perm-deter-error", NULL);
		return FALSE;
	}

	/* Fill in delegate structures from the security descriptors */
	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		for (u = 0; u < delegates->users->len; u++) {
			user = delegates->users->pdata[u];
			perms = e2k_security_descriptor_get_permissions (
				delegates->folder[i].sd, user->sid);
			user->role[i] = e2k_permissions_role_find (perms);
		}
	}

	delegates->loaded_folders = TRUE;
	return TRUE;
}


static const char *delegation_props[] = {
	PR_DELEGATES_DISPLAY_NAMES,
	PR_DELEGATES_ENTRYIDS,
	PR_DELEGATES_SEE_PRIVATE,
	PR_CREATOR_ENTRYID
};
static const int n_delegation_props = sizeof (delegation_props) / sizeof (delegation_props[0]);

/* Fetch the list of delegates from the freebusy message. */
static gboolean
get_user_list (ExchangeDelegates *delegates)
{
	E2kContext *ctx;
	E2kResultIter *iter;
	E2kResult *result;
	GPtrArray *display_names, *entryids, *privflags;
	GByteArray *entryid;
	ExchangeDelegatesUser *user;
	int i;

	ctx = exchange_account_get_context (delegates->account);
	iter = e2k_context_bpropfind_start (ctx, NULL,
					    delegates->account->home_uri,
					    &exchange_localfreebusy_path, 1,
					    delegation_props, n_delegation_props);
	result = e2k_result_iter_next (iter);
	if (!result || !E2K_HTTP_STATUS_IS_SUCCESSFUL (result->status)) {
		e2k_result_iter_free (iter);
		return FALSE;
	}

	delegates->users = g_ptr_array_new ();
	delegates->added_users = g_ptr_array_new ();
	delegates->removed_users = g_ptr_array_new ();

	display_names = e2k_properties_get_prop (result->props, PR_DELEGATES_DISPLAY_NAMES);
	entryids      = e2k_properties_get_prop (result->props, PR_DELEGATES_ENTRYIDS);
	privflags     = e2k_properties_get_prop (result->props, PR_DELEGATES_SEE_PRIVATE);

	entryid       = e2k_properties_get_prop (result->props, PR_CREATOR_ENTRYID);
	delegates->creator_entryid = g_byte_array_new ();
	g_byte_array_append (delegates->creator_entryid, entryid->data, entryid->len);

	if (!display_names || !entryids || !privflags) {
		e2k_result_iter_free (iter);
		return TRUE;
	}

	for (i = 0; i < display_names->len && i < entryids->len && i < privflags->len; i++) {
		user = exchange_delegates_user_new (display_names->pdata[i]);
		user->see_private  = privflags->pdata[i] && atoi (privflags->pdata[i]);
		entryid            = entryids->pdata[i];
		user->entryid      = g_byte_array_new ();
		g_byte_array_append (user->entryid, entryid->data, entryid->len);

		g_signal_connect (user, "edited", G_CALLBACK (set_perms_for_user), delegates);

		g_ptr_array_add (delegates->users, user);
	}

	e2k_result_iter_free (iter);
	return TRUE;
}

/* Add or remove a delegate. Everyone must be in one of three states:
 *   1. only in users (because they started and ended there)
 *   2. in users and added_users (because they weren't in
 *      users to begin with, but got added)
 *   3. only in removed_users (because they were in users to
 *      begin with and got removed).
 * If you're added and then removed, or removed and then added, you have
 * to end up in state 1. That's what this is for.
 */
static void
add_remove_user (ExchangeDelegatesUser *user, 
		 GPtrArray *to_array, GPtrArray *from_array)
{
	ExchangeDelegatesUser *match;
	int i;

	for (i = 0; i < from_array->len; i++) {
		match = from_array->pdata[i];
		if (e2k_sid_binary_sid_equal (e2k_sid_get_binary_sid (match->sid),
					      e2k_sid_get_binary_sid (user->sid))) {
			g_ptr_array_remove_index_fast (from_array, i);
			g_object_unref (match);
			return;
		}
	}

	g_ptr_array_add (to_array, user);
	g_object_ref (user);
}

static void
set_perms_for_user (ExchangeDelegatesUser *user, gpointer user_data)
{
	ExchangeDelegates *delegates = user_data;
	int i, role;
	guint32 perms;

	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		perms = e2k_permissions_role_get_perms (user->role[i]);
		e2k_security_descriptor_set_permissions (delegates->folder[i].sd,
							 user->sid, perms);
	}
	role = user->role[EXCHANGE_DELEGATES_CALENDAR];
	if (role == E2K_PERMISSIONS_ROLE_AUTHOR)
		role = E2K_PERMISSIONS_ROLE_EDITOR;
	perms = e2k_permissions_role_get_perms (role);
	e2k_security_descriptor_set_permissions (delegates->freebusy_folder.sd,
						 user->sid, perms);
}

static void
add_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	ExchangeDelegates *delegates = data;
	E2kGlobalCatalog *gc;
	GtkWidget *dialog, *parent_window;
	const char *delegate_exchange_dn;
	char *email;
	ExchangeDelegatesUser *user, *match;
	int response, u;
	GtkTreeIter iter;

	if (!get_folder_security (delegates))
		return;

	gc = exchange_account_get_global_catalog (delegates->account);

	parent_window = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);
	dialog = e2k_user_dialog_new (parent_window,
				      _("Delegate To:"), _("Delegate To"));
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}
	email = e2k_user_dialog_get_user (E2K_USER_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (email == NULL)
		return;

	user = exchange_delegates_user_new_from_gc (gc, email,
						    delegates->creator_entryid);
	if (!user) {
		e_error_run (GTK_WINDOW (parent_window), ERROR_DOMAIN ":delegate-error", email, NULL);
		g_free (email);
		return;
	}
	g_free (email);

	delegate_exchange_dn = e2k_entryid_to_dn (user->entryid);
	if (delegate_exchange_dn && !g_ascii_strcasecmp (delegate_exchange_dn, delegates->account->legacy_exchange_dn)) {
		g_object_unref (user);
		e_error_run (GTK_WINDOW (parent_window), ERROR_DOMAIN ":delegate-own-error", NULL);
		return;
	}

	for (u = 0; u < delegates->users->len; u++) {
		match = delegates->users->pdata[u];
		if (e2k_sid_binary_sid_equal (e2k_sid_get_binary_sid (user->sid),
					      e2k_sid_get_binary_sid (match->sid))) {
			e_error_run (GTK_WINDOW (parent_window), ERROR_DOMAIN ":delegate-existing", 
				     user->display_name, NULL);
			g_object_unref (user);
			exchange_delegates_user_edit (match, parent_window);
			return;
		}
	}

	if (!exchange_delegates_user_edit (user, parent_window)) {
		g_object_unref (user);
		return;
	}
	set_perms_for_user (user, delegates);
	g_signal_connect (user, "edited",
			  G_CALLBACK (set_perms_for_user), delegates);

	add_remove_user (user, delegates->added_users, delegates->removed_users);
	g_ptr_array_add (delegates->users, user);

	/* Add the user to the table */
	gtk_list_store_append (delegates->model, &iter);
	gtk_list_store_set (delegates->model, &iter,
			    0, user->display_name,
			    -1);
}

static int
get_selected_row (GtkWidget *tree_view, GtkTreeIter *iter)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	int *indices, row;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, iter))
		return -1;

	path = gtk_tree_model_get_path (model, iter);
	indices = gtk_tree_path_get_indices (path);
	row = indices[0];
	gtk_tree_path_free (path);

	return row;
}

static void
edit_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	ExchangeDelegates *delegates = data;
	GtkWidget *parent_window;
	GtkTreeIter iter;
	int row;

	if (!get_folder_security (delegates))
		return;

	row = get_selected_row (delegates->table, &iter);
	g_return_if_fail (row >= 0 && row < delegates->users->len);

	parent_window = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);
	exchange_delegates_user_edit (delegates->users->pdata[row],
				      parent_window);
}

static gboolean
table_click_cb (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	ExchangeDelegates *delegates = data;
	GtkWidget *parent_window;
	GtkTreeIter iter;
	int row;

	if (event->type != GDK_2BUTTON_PRESS)
		return FALSE;

	row = get_selected_row (delegates->table, &iter);
	if (row < 0 || row >= delegates->users->len)
		return FALSE;

	if (!get_folder_security (delegates))
		return FALSE;

	parent_window = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);
	exchange_delegates_user_edit (delegates->users->pdata[row],
				      parent_window);
	return TRUE;
}

static void
remove_button_clicked_cb (GtkWidget *widget, gpointer data)
{
	ExchangeDelegates *delegates = data;
	ExchangeDelegatesUser *user;
	GtkWidget *dialog;
	int row, btn, i;
	GtkTreeIter iter;

	if (!get_folder_security (delegates))
		return;

	row = get_selected_row (delegates->table, &iter);
	g_return_if_fail (row >= 0 && row < delegates->users->len);

	user = delegates->users->pdata[row];

	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO,
					 _("Remove the delegate %s?"),
					 user->display_name);
	e_dialog_set_transient_for (GTK_WINDOW (dialog), widget); 

	btn = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	if (btn != GTK_RESPONSE_YES)
		return;

	add_remove_user (user, delegates->removed_users, delegates->added_users);

	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		e2k_security_descriptor_remove_sid (delegates->folder[i].sd,
						    user->sid);
	}
	e2k_security_descriptor_remove_sid (delegates->freebusy_folder.sd,
					    user->sid);

	/* Remove the user from the table */
	gtk_list_store_remove (delegates->model, &iter);
	g_ptr_array_remove_index (delegates->users, row);
	g_object_unref (user);
}


static gboolean
proppatch_sd (E2kContext *ctx, ExchangeDelegatesFolder *folder)
{
	GByteArray *binsd;
	E2kProperties *props;
	const char *href = "";
	E2kResultIter *iter;
	E2kResult *result;
	E2kHTTPStatus status;

	binsd = e2k_security_descriptor_to_binary (folder->sd);
	if (!binsd)
		return FALSE;

	props = e2k_properties_new ();
	e2k_properties_set_binary (props, E2K_PR_EXCHANGE_SD_BINARY, binsd);

	iter = e2k_context_bproppatch_start (ctx, NULL, folder->uri,
					     &href, 1, props, FALSE);
	e2k_properties_free (props);

	result = e2k_result_iter_next (iter);
	if (result) {
		status = result->status;
		e2k_result_iter_free (iter);
	} else
		status = e2k_result_iter_free (iter);

	return E2K_HTTP_STATUS_IS_SUCCESSFUL (status);
}

static gboolean
get_user_dn (E2kGlobalCatalog *gc, ExchangeDelegatesUser *user)
{
	E2kGlobalCatalogEntry *entry;
	E2kGlobalCatalogStatus status;
	const char *exchange_dn;

	exchange_dn = e2k_entryid_to_dn (user->entryid);
	status = e2k_global_catalog_lookup (
		gc, NULL, /* FIXME: cancellable */
		E2K_GLOBAL_CATALOG_LOOKUP_BY_LEGACY_EXCHANGE_DN,
		exchange_dn, 0, &entry);
	if (status != E2K_GLOBAL_CATALOG_OK)
		return FALSE;

	user->dn = g_strdup (entry->dn);
	e2k_global_catalog_entry_free (gc, entry);
	return TRUE;
}

static void
delegates_apply (ExchangeDelegates *delegates)
{
	ExchangeDelegatesUser *user;
	E2kGlobalCatalog *gc;
	E2kContext *ctx;
	GPtrArray *display_names, *entryids, *privflags;
	GByteArray *entryid_dup;
	char *error = NULL;
	E2kProperties *props;
	int i, status;

	if (!delegates->loaded_folders)
		return;

	/* We can't do this atomically/transactionally, so we need to
	 * make sure that if we fail at any step, things are still in
	 * a semi-consistent state. So we do:
	 *
	 *   1. Remove old delegates from AD
	 *   2. Update LocalFreebusy.EML (the canonical list of delegates)
	 *   3. Add new delegates to AD
	 *   4. Update security descriptors
	 *
	 * If step 1 fails, nothing is changed.
	 *
	 * If step 2 fails, delegates who should have been removed
	 * will have been removed from AD but nothing else, so they
	 * will still show up as being delegates and the user can try
	 * to remove them again later.
	 *
	 * If step 3 fails, delegates who should have been added will
	 * not be in AD, but will be listed as delegates, so the user
	 * can remove them and try adding them again later.
	 *
	 * If step 4 fails, the user can still correct the folder
	 * permissions by hand.
	 */

	gc = exchange_account_get_global_catalog (delegates->account);
	if (!gc) {
		error = g_strdup (_("Could not access Active Directory"));
		goto done;
	}

	if ((delegates->removed_users || delegates->added_users) && !delegates->self_dn) {
		E2kGlobalCatalog *gc;
		E2kGlobalCatalogStatus status;
		E2kGlobalCatalogEntry *entry;

		gc = exchange_account_get_global_catalog (delegates->account);
		status = e2k_global_catalog_lookup (
			gc, NULL, /* FIXME: cancellable */
			E2K_GLOBAL_CATALOG_LOOKUP_BY_LEGACY_EXCHANGE_DN,
			delegates->account->legacy_exchange_dn, 0, &entry);
		if (status != E2K_GLOBAL_CATALOG_OK) {
			error = g_strdup (_("Could not find self in Active Directory"));
			goto done;
		}

		delegates->self_dn = g_strdup (entry->dn);
		e2k_global_catalog_entry_free (gc, entry);
	}

	/* 1. Remove old delegates from AD */
	while (delegates->removed_users && delegates->removed_users->len) {
		user = delegates->removed_users->pdata[0];
		if (!user->dn && !get_user_dn (gc, user)) {
			error = g_strdup_printf (
				_("Could not find delegate %s in Active Directory"),
				user->display_name);
			goto done;
		}

		/* FIXME: cancellable */
		status = e2k_global_catalog_remove_delegate (gc, NULL,
							     delegates->self_dn,
							     user->dn);
		if (status != E2K_GLOBAL_CATALOG_OK &&
		    status != E2K_GLOBAL_CATALOG_NO_DATA) {
			error = g_strdup_printf (
				_("Could not remove delegate %s"),
				user->display_name);
			goto done;
		}

		g_object_unref (user);
		g_ptr_array_remove_index_fast (delegates->removed_users, 0);
	}

	/* 2. Update LocalFreebusy.EML */
	ctx = exchange_account_get_context (delegates->account);

	if (delegates->users->len) {
		display_names = g_ptr_array_new ();
		entryids = g_ptr_array_new ();
		privflags = g_ptr_array_new ();

		for (i = 0; i < delegates->users->len; i++) {
			user = delegates->users->pdata[i];
			g_ptr_array_add (display_names, g_strdup (user->display_name)); 
			entryid_dup = g_byte_array_new ();
			g_byte_array_append (entryid_dup, user->entryid->data,
					     user->entryid->len);
			g_ptr_array_add (entryids, entryid_dup);
			g_ptr_array_add (privflags, g_strdup_printf ("%d", user->see_private));
		}

		props = e2k_properties_new (); 
		e2k_properties_set_string_array (
			props, PR_DELEGATES_DISPLAY_NAMES, display_names);
		e2k_properties_set_binary_array (
			props, PR_DELEGATES_ENTRYIDS, entryids);
		e2k_properties_set_int_array (
			props, PR_DELEGATES_SEE_PRIVATE, privflags);
	} else if (delegates->removed_users) {
		props = e2k_properties_new (); 
		e2k_properties_remove (props, PR_DELEGATES_DISPLAY_NAMES);
		e2k_properties_remove (props, PR_DELEGATES_ENTRYIDS);
		e2k_properties_remove (props, PR_DELEGATES_SEE_PRIVATE);
	} else
		props = NULL;

	if (props) {
		E2kResultIter *iter;
		E2kResult *result;

		iter = e2k_context_bproppatch_start (
			ctx, NULL, delegates->account->home_uri,
			&exchange_localfreebusy_path, 1,
			props, FALSE);
		e2k_properties_free (props);

		result = e2k_result_iter_next (iter);
		if (result) {
			status = result->status;
			e2k_result_iter_free (iter);
		} else
			status = e2k_result_iter_free (iter);

		if (!E2K_HTTP_STATUS_IS_SUCCESSFUL (status)) {
			error = g_strdup (_("Could not update list of delegates."));
			goto done;
		}
	}

	/* 3. Add new delegates to AD */
	while (delegates->added_users && delegates->added_users->len) {
		user = delegates->added_users->pdata[0];
		/* An added user must have come from the GC so
		 * we know user->dn is set.
		 */
		/* FIXME: cancellable */
		status = e2k_global_catalog_add_delegate (gc, NULL,
							  delegates->self_dn,
							  user->dn);
		if (status != E2K_GLOBAL_CATALOG_OK &&
		    status != E2K_GLOBAL_CATALOG_EXISTS) {
			error = g_strdup_printf (
				_("Could not add delegate %s"),
				user->display_name);
			goto done;
		}
		g_ptr_array_remove_index_fast (delegates->added_users, 0);
		g_object_unref (user);
	}

	/* 4. Update security descriptors */
	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++)
		proppatch_sd (ctx, &delegates->folder[i]);
	proppatch_sd (ctx, &delegates->freebusy_folder);

 done:
	if (error) {
		e_error_run (GTK_WINDOW (delegates->table), ERROR_DOMAIN ":delegate-fail-error", error, NULL);
		g_free (error);
	}
}

static void parent_destroyed (gpointer user_data, GObject *ex_parent);

static void
delegates_destroy (ExchangeDelegates *delegates)
{
	int i;

	g_object_unref (delegates->account);

	if (delegates->parent) {
		g_object_weak_unref (G_OBJECT (delegates->parent),
				     parent_destroyed, delegates);
	}
	if (delegates->dialog)
		gtk_widget_destroy (delegates->dialog);

	if (delegates->model)
		g_object_unref (delegates->model);

	if (delegates->self_dn)
		g_free (delegates->self_dn);
	if (delegates->creator_entryid)
		g_byte_array_free (delegates->creator_entryid, TRUE);

	if (delegates->users) {
		for (i = 0; i < delegates->users->len; i++)
			g_object_unref (delegates->users->pdata[i]);
		g_ptr_array_free (delegates->users, TRUE);
	}
	if (delegates->added_users) {
		for (i = 0; i < delegates->added_users->len; i++)
			g_object_unref (delegates->added_users->pdata[i]);
		g_ptr_array_free (delegates->added_users, TRUE);
	}
	if (delegates->removed_users) {
		for (i = 0; i < delegates->removed_users->len; i++)
			g_object_unref (delegates->removed_users->pdata[i]);
		g_ptr_array_free (delegates->removed_users, TRUE);
	}

	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		if (delegates->folder[i].sd)
			g_object_unref (delegates->folder[i].sd);
	}
	if (delegates->freebusy_folder.sd)
		g_object_unref (delegates->freebusy_folder.sd);
	if (delegates->freebusy_folder.uri)
		g_free ((char *)delegates->freebusy_folder.uri);

	if (delegates->xml)
		g_object_unref (delegates->xml);

	g_free (delegates);
}


static void
dialog_response (GtkDialog *dialog, int response, gpointer user_data)
{
	ExchangeDelegates *delegates = user_data;

	if (response == GTK_RESPONSE_OK)
		delegates_apply (delegates);
	delegates_destroy (delegates);
}

static void
parent_destroyed (gpointer user_data, GObject *ex_parent)
{
	ExchangeDelegates *delegates = user_data;

	gtk_widget_destroy (delegates->dialog);
	delegates_destroy (delegates);
}

void
exchange_delegates (ExchangeAccount *account, GtkWidget *parent)
{
	ExchangeDelegates *delegates;
	GtkWidget *button;
	ExchangeDelegatesUser *user;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	int i;
	
	g_return_if_fail (GTK_IS_WIDGET (parent));
	g_return_if_fail (EXCHANGE_IS_ACCOUNT (account));

	delegates = g_new0 (ExchangeDelegates, 1);
	delegates->account = g_object_ref (account);

	delegates->xml = glade_xml_new (CONNECTOR_GLADEDIR "/exchange-delegates.glade", NULL, NULL);
	g_return_if_fail (delegates->xml != NULL);

	delegates->dialog = glade_xml_get_widget (delegates->xml, "delegates");
	g_return_if_fail (delegates->dialog != NULL);

	g_signal_connect (delegates->dialog, "response",
			  G_CALLBACK (dialog_response), delegates);

	e_dialog_set_transient_for (GTK_WINDOW (delegates->dialog), parent);
	delegates->parent = parent;
	g_object_weak_ref (G_OBJECT (parent), parent_destroyed, delegates);

	/* Set up the buttons */
	button = glade_xml_get_widget (delegates->xml, "add_button");
	g_signal_connect (button, "clicked",
			  G_CALLBACK (add_button_clicked_cb), delegates);
	button = glade_xml_get_widget (delegates->xml, "edit_button");
	g_signal_connect (button, "clicked",
			  G_CALLBACK (edit_button_clicked_cb), delegates);
	button = glade_xml_get_widget (delegates->xml, "remove_button");
	g_signal_connect (button, "clicked",
			  G_CALLBACK (remove_button_clicked_cb), delegates);

	/* Set up the table */
	delegates->model = gtk_list_store_new (1, G_TYPE_STRING);
 	delegates->table = glade_xml_get_widget (delegates->xml, "delegates_table");
	column = gtk_tree_view_column_new_with_attributes (
		_("Name"), gtk_cell_renderer_text_new (), "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (delegates->table),
				     column);
	gtk_tree_view_set_model (GTK_TREE_VIEW (delegates->table),
				 GTK_TREE_MODEL (delegates->model));

	/* Get list of delegate users */
	if (get_user_list (delegates)) {
		for (i = 0; i < delegates->users->len; i++) {
			user = delegates->users->pdata[i];

			gtk_list_store_append (delegates->model, &iter);
			gtk_list_store_set (delegates->model, &iter,
					    0, user->display_name,
					    -1);
		}
		g_signal_connect (delegates->table,
				  "button_press_event",
				  G_CALLBACK (table_click_cb), delegates);
	} else {
		button = glade_xml_get_widget (delegates->xml, "add_button");
		gtk_widget_set_sensitive (button, FALSE);
		button = glade_xml_get_widget (delegates->xml, "edit_button");
		gtk_widget_set_sensitive (button, FALSE);
		button = glade_xml_get_widget (delegates->xml, "remove_button");
		gtk_widget_set_sensitive (button, FALSE);

		gtk_list_store_append (delegates->model, &iter);
		gtk_list_store_set (delegates->model, &iter,
				    0, _("Error reading delegates list."),
				    -1);
	}

	gtk_widget_show (delegates->dialog);
}
