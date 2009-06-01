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
#include <libedataserver/e-url.h>
#include <e-folder.h>
#include <exchange-account.h>
#include <libecal/e-cal.h>

#include "calendar/gui/dialogs/calendar-setup.h"
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "libedataserver/e-account.h"
#include "e-util/e-error.h"

#include "exchange-operations.h"
#include "exchange-folder-size-display.h"

enum {
	CALENDARNAME_COL,
	CALENDARRURI_COL,
	NUM_COLS
};

gboolean calendar_src_exists = FALSE;
gchar *calendar_old_source_uri = NULL;

static GPtrArray *e_exchange_calendar_get_calendars (ECalSourceType ftype);
void e_exchange_calendar_pcalendar_on_change (GtkTreeView *treeview, ESource *source);
GtkWidget *e_exchange_calendar_pcalendar (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean e_exchange_calendar_check (EPlugin *epl, EConfigHookPageCheckData *data);
void e_exchange_calendar_commit (EPlugin *epl, EConfigTarget *target);

/* FIXME: Reconsider the prototype of this function */
static GPtrArray *
e_exchange_calendar_get_calendars (ECalSourceType ftype)
{
	ExchangeAccount *account;
	GPtrArray *folder_array;
	GPtrArray *calendar_list;
	EFolder *folder;
	gint i, prefix_len;
	gchar *type;
	gchar *uri_prefix;
	gchar *tmp, *ruri;
	gchar *tstring;

	/* FIXME: Compiler warns here; review needed */
	if (ftype == E_CAL_SOURCE_TYPE_EVENT) { /* Calendars */
		tstring = g_strdup ("calendar");
	}
	else if (ftype == E_CAL_SOURCE_TYPE_TODO) { /* Tasks */
		tstring = g_strdup ("tasks");
	}
	else {
		/* FIXME: Would this ever happen? If so, handle it wisely */
		tstring = NULL;
	}

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return NULL;

	/* FIXME: Reconsider this hardcoding */
	uri_prefix = g_strconcat ("exchange://", account->account_filename, "/;", NULL);
	prefix_len = strlen (uri_prefix);

	calendar_list = g_ptr_array_new ();

	exchange_account_rescan_tree (account);
	folder_array = exchange_account_get_folders (account);

	for (i=0; i<folder_array->len; ++i) {
		folder = g_ptr_array_index (folder_array, i);
		type = (gchar *)e_folder_get_type_string (folder);

		if (!strcmp (type, tstring)) {
			tmp = (gchar *)e_folder_get_physical_uri (folder);
			if (g_str_has_prefix (tmp, uri_prefix)) {
				ruri = g_strdup (tmp+prefix_len);
				g_ptr_array_add (calendar_list, ruri);
			}
		}
	}

	if (folder_array)
		g_ptr_array_free (folder_array, TRUE);
	g_free (uri_prefix);
	g_free (tstring);
	return calendar_list;
}

void
e_exchange_calendar_pcalendar_on_change (GtkTreeView *treeview, ESource *source)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	ExchangeAccount *account;
	gchar *es_ruri, *ruri;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_get_selected(selection, &model, &iter);

	gtk_tree_model_get (model, &iter, CALENDARRURI_COL, &ruri, -1);
	es_ruri = g_strconcat (account->account_filename, "/;", ruri, NULL);
	e_source_set_relative_uri (source, es_ruri);
	g_free (ruri);
	g_free (es_ruri);
}

GtkWidget *
e_exchange_calendar_pcalendar (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *lbl_pcalendar, *scrw_pcalendar, *tv_pcalendar, *lbl_size, *lbl_size_val;
	static GtkWidget *hidden = NULL;
	GtkWidget *parent;
	GtkTreeStore *ts_pcalendar;
	GtkCellRenderer *cr_calendar;
	GtkTreeViewColumn *tvc_calendar;
	GtkListStore *model;
	GPtrArray *callist;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	EUri *uri;
	ExchangeAccount *account;
	gchar *ruri;
	gchar *account_name;
        gchar *uri_text;
	gchar *cal_name;
	gchar *folder_size;
	const gchar *rel_uri;
	gint row, i;
	gint offline_status;
	gchar *offline_msg;
	GtkWidget *lbl_offline_msg;
	gboolean is_personal;

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old) {
		/* FIXME: Review this */
		gtk_widget_destroy (lbl_pcalendar);
		gtk_widget_destroy (scrw_pcalendar);
		gtk_widget_destroy (tv_pcalendar);
	}

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);

	if (uri && strcmp (uri->protocol, "exchange")) {
		e_uri_free (uri);
                g_free (uri_text);
		return hidden;
	}

	e_uri_free (uri);

	parent = data->parent;
	row = ((GtkTable*)parent)->nrows;

	exchange_config_listener_get_offline_status (exchange_global_config_listener,
								    &offline_status);
	if (offline_status == OFFLINE_MODE) {
		/* Evolution is in offline mode; we will not be able to create
		   new folders or modify existing folders. */
		offline_msg = g_markup_printf_escaped ("<b>%s</b>",
						       _("Evolution is in offline mode. You cannot create or modify folders now.\nPlease switch to online mode for such operations."));
		lbl_offline_msg = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (lbl_offline_msg), offline_msg);
		g_free (offline_msg);
		gtk_widget_show (lbl_offline_msg);
		gtk_table_attach (GTK_TABLE (parent), lbl_offline_msg, 0, 2, row, row+1, GTK_FILL|GTK_EXPAND, 0, 0, 0);
		g_free (uri_text);
		return lbl_offline_msg;
	}

	rel_uri = e_source_peek_relative_uri (t->source);
	if (rel_uri && strlen (rel_uri)) {
		calendar_src_exists = TRUE;
		g_free (calendar_old_source_uri);
		calendar_old_source_uri = g_strdup (rel_uri);
	}
	else {
		calendar_src_exists = FALSE;
	}

	/* REVIEW: Should this handle be freed? - Attn: surf */
	account = exchange_operations_get_exchange_account ();
	if (!account) {
		g_free (calendar_old_source_uri);
		g_free (uri_text);
		return NULL;
	}
	account_name = account->account_name;
	is_personal = is_exchange_personal_folder (account, uri_text);
	g_free (uri_text);

	if (calendar_src_exists && is_personal) {
		cal_name = (gchar *) e_source_peek_name (source);
		model = exchange_account_folder_size_get_model (account);
		if (model)
			folder_size = g_strdup_printf ("%s KB", exchange_folder_size_get_val (model, cal_name));
		else
			folder_size = g_strdup ("0 KB");

		/* FIXME: Take care of i18n */
		lbl_size = gtk_label_new_with_mnemonic (_("Size:"));
		lbl_size_val = gtk_label_new_with_mnemonic (_(folder_size));
		gtk_widget_show (lbl_size);
		gtk_widget_show (lbl_size_val);
		gtk_misc_set_alignment (GTK_MISC (lbl_size), 0.0, 0.5);
		gtk_misc_set_alignment (GTK_MISC (lbl_size_val), 0.0, 0.5);
		gtk_table_attach (GTK_TABLE (parent), lbl_size, 0, 2, row, row+1, GTK_FILL|GTK_EXPAND, 0, 0, 0);
		gtk_table_attach (GTK_TABLE (parent), lbl_size_val, 1, 3, row, row+1, GTK_FILL|GTK_EXPAND, 0, 0, 0);
		g_free (folder_size);
	}

	lbl_pcalendar = gtk_label_new_with_mnemonic (_("_Location:"));
	gtk_widget_show (lbl_pcalendar);
	gtk_misc_set_alignment (GTK_MISC (lbl_pcalendar), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), lbl_pcalendar, 0, 2, row+1, row+2, GTK_FILL|GTK_EXPAND, 0, 0, 0);

	ts_pcalendar = gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING);

	callist = e_exchange_calendar_get_calendars (t->source_type);

	if (callist) {
		for (i = 0; i < callist->len; i++) {
			ruri = g_ptr_array_index (callist, i);
			exchange_operations_cta_add_node_to_tree (ts_pcalendar, NULL, ruri);
		}
		g_ptr_array_free (callist, TRUE);
	}

	cr_calendar = gtk_cell_renderer_text_new ();
	tvc_calendar = gtk_tree_view_column_new_with_attributes (account_name, cr_calendar, "text", CALENDARNAME_COL, NULL);
	tv_pcalendar = gtk_tree_view_new_with_model (GTK_TREE_MODEL (ts_pcalendar));
	gtk_tree_view_append_column (GTK_TREE_VIEW (tv_pcalendar), tvc_calendar);
	g_object_set (tv_pcalendar, "expander-column", tvc_calendar, "headers-visible", TRUE, NULL);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (tv_pcalendar));

	scrw_pcalendar = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrw_pcalendar), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrw_pcalendar), GTK_SHADOW_IN);
	g_object_set (scrw_pcalendar, "height-request", 150, NULL);
	gtk_container_add (GTK_CONTAINER (scrw_pcalendar), tv_pcalendar);
	gtk_label_set_mnemonic_widget (GTK_LABEL (lbl_pcalendar), tv_pcalendar);
	g_signal_connect (G_OBJECT (tv_pcalendar), "cursor-changed", G_CALLBACK (e_exchange_calendar_pcalendar_on_change), t->source);

	gtk_table_attach (GTK_TABLE (parent), scrw_pcalendar, 0, 2, row+2, row+3, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_widget_show_all (scrw_pcalendar);

	if (calendar_src_exists) {
		gchar *uri_prefix, *sruri, *tmpruri;
		gint prefix_len;
		GtkTreeSelection *selection;

		uri_prefix = g_strconcat (account->account_filename, "/;", NULL);
		prefix_len = strlen (uri_prefix);

		tmpruri = (gchar *) rel_uri;

		if (g_str_has_prefix (tmpruri, uri_prefix)) {
			sruri = g_strdup (tmpruri+prefix_len);
		}
		else {
			sruri = NULL;
		}

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv_pcalendar));
		exchange_operations_cta_select_node_from_tree (ts_pcalendar, NULL, sruri, sruri, selection);
		gtk_widget_set_sensitive (tv_pcalendar, FALSE);
		g_free (uri_prefix);
		g_free (sruri);
	}

	g_object_unref (ts_pcalendar);
	return tv_pcalendar;
}

gboolean
e_exchange_calendar_check (EPlugin *epl, EConfigHookPageCheckData *data)
{
	/* FIXME - check pageid */
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESourceGroup *group;
	const gchar *base_uri;
	const gchar *rel_uri;
	gint offline_status;
	ExchangeAccount *account;
	EUri *euri;
	gint uri_len;
	gchar *uri_text, *uri_string, *path, *folder_name;
	gboolean is_personal;

	rel_uri = e_source_peek_relative_uri (t->source);
	group = e_source_peek_group (t->source);
	base_uri = e_source_group_peek_base_uri (group);
	exchange_config_listener_get_offline_status (exchange_global_config_listener,
						     &offline_status);
	if (base_uri && !strncmp (base_uri, "exchange", 8)) {
		if (offline_status == OFFLINE_MODE)
			return FALSE;
		if (rel_uri && !strlen (rel_uri))
			return FALSE;
	}
	else {
		return TRUE;
	}

	if (!calendar_src_exists) {
		/* new folder */
                return TRUE;
        }

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return FALSE;

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
	}
	g_free (folder_name);

	return TRUE;
}

void
e_exchange_calendar_commit (EPlugin *epl, EConfigTarget *target)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) target;
	ESource *source = t->source;
	gchar *uri_text, *gruri, *gname, *ruri, *ftype, *path, *path_prefix, *oldpath=NULL;
	gchar *username, *windows_domain, *authtype;
	gint prefix_len;
	ExchangeAccount *account;
	ExchangeAccountFolderResult result;
	ExchangeConfigListenerStatus status;
	gint offline_status;
	gboolean rename = FALSE;

	uri_text = e_source_get_uri (source);
	if (uri_text && strncmp (uri_text, "exchange", 8)) {
		g_free (uri_text);
		return;
	}

	status = exchange_is_offline (&offline_status);
	if (offline_status == OFFLINE_MODE || status != CONFIG_LISTENER_STATUS_OK) {
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

	/* FIXME: Compiler gives a warning here; review needed */
	if (t->source_type == E_CAL_SOURCE_TYPE_EVENT) {
		ftype = g_strdup ("calendar");
	}
	else if (t->source_type == E_CAL_SOURCE_TYPE_TODO) {
		ftype = g_strdup ("tasks");
	}
	else {
		/* FIXME: This one would ever occur? */
		ftype = g_strdup ("mail");
	}

	gname = (gchar *) e_source_peek_name (source);
	gruri = (gchar *) e_source_peek_relative_uri (source);

	if (calendar_src_exists) {
		gchar *tmpruri, *uri_string, *temp_path, *prefix;
		EUri *euri;
		gint uri_len;

		/* sample uri_string: exchange://user;auth=NTLM@host/ */
		/* sample uri_text: exchange://user;auth=NTLM@host/;personal/Calendar */

		euri = e_uri_new (uri_text);
		uri_string = e_uri_to_string (euri, FALSE);
		e_uri_free (euri);

		/* sample gruri: user;auth=NTLM@host/;personal/Calendar */
		/* sample ruri: user;auth=NTLM@host/personal/Calendar */
		/* sample path: /personal/Calendar */

		uri_len = strlen (uri_string) + 1;
		tmpruri = g_strdup (uri_string + strlen ("exchange://"));
		temp_path = g_build_filename ("/", uri_text + uri_len, NULL);
		prefix  = g_strndup (temp_path, strlen (temp_path) - strlen (g_strrstr (temp_path, "/")));
		g_free (temp_path);
		path = g_build_filename (prefix, "/", gname, NULL);
		ruri = g_strconcat (tmpruri, ";", path+1, NULL);
		oldpath = g_build_filename ("/", calendar_old_source_uri + prefix_len, NULL);
		g_free (prefix);
		g_free (uri_string);
		g_free (tmpruri);
	}
	else {
		/* new folder */
		ruri = g_strconcat (gruri, "/", gname, NULL);
		path = g_build_filename ("/", ruri+prefix_len, NULL);
	}

	if (!calendar_src_exists) {
		/* Create the new folder */
		result = exchange_account_create_folder (account, path, ftype);
	}
	else if (gruri && strcmp (path, oldpath)) {
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
		if (result == EXCHANGE_ACCOUNT_FOLDER_OK) {
			e_source_set_name (source, gname);
			e_source_set_relative_uri (source, ruri);
			e_source_set_property (source, "username", username);
			e_source_set_property (source, "auth-domain", "Exchange");
			if (authtype) {
				e_source_set_property (source, "auth-type", authtype);
				g_free (authtype);
				authtype=NULL;
			}
			e_source_set_property (source, "auth", "1");
			if (rename) {
				exchange_operations_update_child_esources (source,
							   calendar_old_source_uri,
							   ruri);
			}
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
	g_free (uri_text);
	g_free (username);
	if (authtype)
		g_free (authtype);
	g_free (ruri);
	g_free (path);
	g_free (ftype);
	g_free (oldpath);
	g_free (calendar_old_source_uri);
	calendar_old_source_uri = NULL;
}
