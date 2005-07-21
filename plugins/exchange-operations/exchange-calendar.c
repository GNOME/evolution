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
#include <libecal/e-cal.h>

#include "calendar/gui/dialogs/calendar-setup.h"
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "e-util/e-account.h"
#include "e-util/e-error.h"

#include "exchange-operations.h"

enum {
	CALENDARNAME_COL,
	CALENDARRURI_COL,
	NUM_COLS
};

GPtrArray *e_exchange_calendar_get_calendars (ECalSourceType *ftype);
void e_exchange_calendar_pcalendar_on_change (GtkTreeView *treeview, ESource *source);
GtkWidget *e_exchange_calendar_pcalendar (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean e_exchange_calendar_check (EPlugin *epl, EConfigHookPageCheckData *data);
void e_exchange_calendar_commit (EPlugin *epl, EConfigTarget *target);

/* FIXME: Reconsider the prototype of this function */
GPtrArray *
e_exchange_calendar_get_calendars (ECalSourceType *ftype) 
{
	ExchangeAccount *account;
	GPtrArray *folder_array;
	GPtrArray *calendar_list;

	EFolder *folder;
	int i, prefix_len;
	gchar *type;
	gchar *uri_prefix;
	gchar *tmp, *ruri;
	gchar *tstring;

	/* FIXME: Compiler warns here; review needed */
	if (GPOINTER_TO_INT (ftype) == E_CAL_SOURCE_TYPE_EVENT) { /* Calendars */
		tstring = g_strdup ("calendar");
	}
	else if (GPOINTER_TO_INT (ftype) == E_CAL_SOURCE_TYPE_TODO) { /* Tasks */
		tstring = g_strdup ("tasks");
	}
	else { 
		/* FIXME: Would this ever happen? If so, handle it wisely */
		tstring = NULL;
	}

	account = exchange_operations_get_exchange_account ();

	/* FIXME: Reconsider this hardcoding */
	uri_prefix = g_strconcat ("exchange://", account->account_filename, "/", NULL);
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
				ruri = g_strdup (tmp+prefix_len); /* ATTN: Shouldn't free this explictly */
				g_ptr_array_add (calendar_list, (gpointer)ruri);
			}
		}
	}

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

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_get_selected(selection, &model, &iter);

	gtk_tree_model_get (model, &iter, CALENDARRURI_COL, &ruri, -1);
	es_ruri = g_strconcat (account->account_filename, "/", ruri, NULL);
	e_source_set_relative_uri (source, es_ruri);
	g_free (ruri);
	g_free (es_ruri);
} 

GtkWidget *
e_exchange_calendar_pcalendar (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *lbl_pcalendar, *scrw_pcalendar, *tv_pcalendar;
	static GtkWidget *hidden = NULL;
	GtkWidget *parent;
	GtkTreeStore *ts_pcalendar;
	GtkCellRenderer *cr_calendar;
	GtkTreeViewColumn *tvc_calendar;
	GPtrArray *callist;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	EUri *uri;
	ExchangeAccount *account;
	gchar *ruri;
	gchar *account_name;
        gchar *uri_text;
	gboolean src_exists = TRUE;
	int row, i;

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

	if (strcmp (uri->protocol, "exchange")) {
		e_uri_free (uri);
                g_free (uri_text);
		return hidden;
	}

	e_uri_free (uri);
	g_free (uri_text);

	if (!strlen (e_source_peek_relative_uri (t->source))) {
		src_exists = FALSE;
	}
	
	parent = data->parent;
	row = ((GtkTable*)parent)->nrows;

	/* REVIEW: Should this handle be freed? - Attn: surf */
	account = exchange_operations_get_exchange_account ();
	account_name = account->account_name;

	/* FIXME: Take care of i18n */
	lbl_pcalendar = gtk_label_new_with_mnemonic (_("_Location:"));
	gtk_widget_show (lbl_pcalendar);
	gtk_misc_set_alignment (GTK_MISC (lbl_pcalendar), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), lbl_pcalendar, 0, 2, row, row+1, GTK_FILL|GTK_EXPAND, 0, 0, 0);
  
	ts_pcalendar = gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING);

	callist = e_exchange_calendar_get_calendars (t->source_type);

	for (i=0; i<callist->len; ++i) {
		ruri = g_ptr_array_index (callist, i);
		exchange_operations_cta_add_node_to_tree (ts_pcalendar, NULL, ruri);		
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

	gtk_table_attach (GTK_TABLE (parent), scrw_pcalendar, 0, 2, row+1, row+2, GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_widget_show_all (scrw_pcalendar);
  
	if (src_exists) {
		gchar *uri_prefix, *sruri, *tmpruri;
		int prefix_len;
		GtkTreeSelection *selection;

		uri_prefix = g_strconcat (account->account_filename, "/", NULL);
		prefix_len = strlen (uri_prefix);
		
		tmpruri = (gchar*) e_source_peek_relative_uri (t->source);

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
	
	g_ptr_array_free (callist, TRUE);
	return tv_pcalendar;
}

gboolean
e_exchange_calendar_check (EPlugin *epl, EConfigHookPageCheckData *data)
{
	/* FIXME - check pageid */
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESourceGroup *group = e_source_peek_group (t->source);

	if (!strncmp (e_source_group_peek_base_uri (group), "exchange", 8)) {
		if (!strlen (e_source_peek_relative_uri (t->source))) {
			return FALSE;
		}
	}

	return TRUE;
}

void 
e_exchange_calendar_commit (EPlugin *epl, EConfigTarget *target)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) target;
	ESource *source = t->source;
	gchar *uri_text, *gruri, *gname, *ruri, *ftype, *path, *path_prefix;
	int prefix_len;

	ExchangeAccount *account;
	ExchangeAccountFolderResult result;
		
	uri_text = e_source_get_uri (source);
	if (strncmp (uri_text, "exchange", 8)) {
		g_free (uri_text);
		return ;
	}	
	
	account = exchange_operations_get_exchange_account ();
	path_prefix = g_strconcat (account->account_filename, "/", NULL);
	prefix_len = strlen (path_prefix);
	g_free (path_prefix);

	/* FIXME: Compiler gives a warning here; review needed */
	if (GPOINTER_TO_INT (t->source_type) == E_CAL_SOURCE_TYPE_EVENT) {
		ftype = g_strdup ("calendar");
	}
	else if (GPOINTER_TO_INT (t->source_type) == E_CAL_SOURCE_TYPE_TODO) {
		ftype = g_strdup ("tasks");
	}
	else {
		/* FIXME: This one would ever occur? */
		ftype = g_strdup ("mail");
	}

	gname = (gchar*) e_source_peek_name (source);
	gruri = (gchar*) e_source_peek_relative_uri (source);
	ruri = g_strconcat (gruri, "/", gname, NULL);
	e_source_set_relative_uri (source, ruri);
	path = g_strdup_printf ("/%s", ruri+prefix_len);

	result = exchange_account_create_folder (account, path, ftype);

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
	
	g_free (uri_text);
	g_free (ruri);
	g_free (path);
	g_free (ftype);
}
