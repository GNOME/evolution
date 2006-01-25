/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 2005 Novell, Inc. (www.novell.com)
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
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libedataserverui/e-passwords.h>
#include <calendar/gui/e-cal-popup.h>
#include <calendar/gui/e-cal-config.h>
#include <calendar/gui/e-cal-menu.h>
#include <shell/es-event.h>
#include <e-util/e-util-private.h>
#include "url-editor-dialog.h"
#include "publish-format-fb.h"
#include "publish-format-ical.h"

static GtkListStore *store = NULL;
static GHashTable *uri_timeouts = NULL;
static GSList *publish_uris = NULL;
static GSList *queued_publishes = NULL;
static gint online = 0;

int          e_plugin_lib_enable (EPlugin *ep, int enable);
void         action_publish (EPlugin *ep, ECalMenuTargetSelect *t);
void         online_state_changed (EPlugin *ep, ESEventTargetState *target);
void         publish_calendar_context_activate (EPlugin *ep, ECalPopupTargetSource *target);
GtkWidget   *publish_calendar_locations (EPlugin *epl, EConfigHookItemFactoryData *data);
static void  update_timestamp (EPublishUri *uri);
static void publish (EPublishUri *uri);

static void
publish_uri_async (EPublishUri *uri) 
{
	GThread *thread = NULL;
	GError *error = NULL;

	thread = g_thread_create ((GThreadFunc) publish, uri, FALSE, &error);
	if (!thread) {
		g_warning (G_STRLOC ": %s", error->message);
		g_error_free (error);
	}
}

static void
publish (EPublishUri *uri)
{
	if (online) {
		GnomeVFSURI *vfs_uri = NULL;
		GnomeVFSResult result;
		GnomeVFSHandle *handle;
		gchar *password;

		if (g_slist_find (queued_publishes, uri))
			queued_publishes = g_slist_remove (queued_publishes, uri);

		if (!uri->enabled)
			return;

		vfs_uri = gnome_vfs_uri_new (uri->location);

		password = e_passwords_get_password ("Calendar", uri->location);
		gnome_vfs_uri_set_password (vfs_uri, password);

		if (vfs_uri == NULL) {
			fprintf (stderr, "Couldn't create uri %s\n", uri->location);
			/* FIXME: EError */
			g_free (password);
			return;
		}

		result = gnome_vfs_create_uri (&handle, vfs_uri, GNOME_VFS_OPEN_WRITE, FALSE, 0666);
		if (result != GNOME_VFS_OK) {
			/* FIXME: EError */
			fprintf (stderr, "Couldn't open %s: %s\n", uri->location, gnome_vfs_result_to_string (result));
			g_free (password);
			return;
		}

		switch (uri->publish_format) {
		case URI_PUBLISH_AS_ICAL:
			publish_calendar_as_ical (handle, uri);
			break;
		case URI_PUBLISH_AS_FB:
			publish_calendar_as_fb (handle, uri);
			break;
/*
		case URI_PUBLISH_AS_HTML:
			publish_calendar_as_html (handle, uri);
			break;
*/
		}

		update_timestamp (uri);

		result = gnome_vfs_close (handle);
		gnome_vfs_uri_unref (vfs_uri);
		g_free (password);
	} else {
		if (g_slist_find (queued_publishes, uri) == NULL)
			queued_publishes = g_slist_prepend (queued_publishes, uri);
	}
}

typedef struct {
	GConfClient *gconf;
	GtkWidget   *treeview;
	GtkWidget   *url_add;
	GtkWidget   *url_edit;
	GtkWidget   *url_remove;
	GtkWidget   *url_enable;
} PublishUIData;

static void
add_timeout (EPublishUri *uri)
{
	guint id;

	/* Set the timeout for now+frequency */
	switch (uri->publish_frequency) {
	case URI_PUBLISH_DAILY:
		id = g_timeout_add (24 * 60 * 60 * 1000, (GSourceFunc) publish, uri);
		g_hash_table_insert (uri_timeouts, uri, GUINT_TO_POINTER (id));
		break;
	case URI_PUBLISH_WEEKLY:
		id = g_timeout_add (7 * 24 * 60 * 60 * 1000, (GSourceFunc) publish, uri);
		g_hash_table_insert (uri_timeouts, uri, GUINT_TO_POINTER (id));
		break;
	}
}

static void
update_timestamp (EPublishUri *uri)
{
	GConfClient *client;
	GSList *uris, *l;
	gchar *xml;
	guint id;

	/* Remove timeout if we have one */
	id = GPOINTER_TO_UINT (g_hash_table_lookup (uri_timeouts, uri));
	if (id) {
		g_source_remove (id);
		add_timeout (uri);
	}

	/* Update timestamp in gconf */
	xml = e_publish_uri_to_xml (uri);

	client = gconf_client_get_default ();
	uris = gconf_client_get_list (client, "/apps/evolution/calendar/publish/uris", GCONF_VALUE_STRING, NULL);
	for (l = uris; l; l = g_slist_next (l)) {
		gchar *d = l->data;
		if (strcmp (d, xml) == 0) {
			uris = g_slist_remove (uris, d);
			g_free (d);
			break;
		}
	}
	g_free (xml);

	if (uri->last_pub_time)
		g_free (uri->last_pub_time);
	uri->last_pub_time = g_strdup_printf ("%d", (int) time (NULL));

	uris = g_slist_prepend (uris, e_publish_uri_to_xml (uri));

	gconf_client_set_list (client, "/apps/evolution/calendar/publish/uris", GCONF_VALUE_STRING, uris, NULL);

	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);
	g_object_unref (client);
}

static void
add_offset_timeout (EPublishUri *uri)
{
	guint id;
	time_t offset = atoi (uri->last_pub_time);
	time_t current = time (NULL);
	gint elapsed = current - offset;

	switch (uri->publish_frequency) {
	case URI_PUBLISH_DAILY:
		if (elapsed > 24 * 60 * 60) {
			publish (uri);
			add_timeout (uri);
		} else {
			id = g_timeout_add (((24 * 60 * 60) - elapsed) * 1000, (GSourceFunc) publish, uri);
			g_hash_table_insert (uri_timeouts, uri, GUINT_TO_POINTER (id));
			break;
		}
		break;
	case URI_PUBLISH_WEEKLY:
		if (elapsed > 7 * 24 * 60 * 60) {
			publish (uri);
			add_timeout (uri);
		} else {
			id = g_timeout_add (((7 * 24 * 60 * 60) - elapsed) * 1000, (GSourceFunc) publish, uri);
			g_hash_table_insert (uri_timeouts, uri, GUINT_TO_POINTER (id));
			break;
		}
		break;
	}
}

static void
url_list_changed (PublishUIData *ui)
{
	GtkTreeModel *model = NULL;
	GSList *url_list = NULL;
	GtkTreeIter iter;
	gboolean valid;
	GConfClient *client;

	url_list = NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
		EPublishUri *url;
		char *xml;

		gtk_tree_model_get (model, &iter, URL_LIST_URL_COLUMN, &url, -1);

		if ((xml = e_publish_uri_to_xml (url)))
			url_list = g_slist_append (url_list, xml);

		valid = gtk_tree_model_iter_next (model, &iter);
	}
	client = gconf_client_get_default ();
	gconf_client_set_list (client, "/apps/evolution/calendar/publish/uris", GCONF_VALUE_STRING, url_list, NULL);
	g_slist_foreach (url_list, (GFunc) g_free, NULL);
	g_slist_free (url_list);
}

static void
url_list_enable_toggled (GtkCellRendererToggle *renderer,
                         const char            *path_string,
			 PublishUIData         *ui)
{
	GtkTreeSelection *selection;
	EPublishUri *url = NULL;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	path = gtk_tree_path_new_from_string (path_string);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_model_get (model, &iter, URL_LIST_URL_COLUMN, &url, -1);

		url->enabled = !url->enabled;
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, URL_LIST_ENABLED_COLUMN, url->enabled, -1);
	}

	gtk_tree_path_free (path);
}

static void
selection_changed (GtkTreeSelection *selection, PublishUIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_widget_set_sensitive (ui->url_edit, TRUE);
		gtk_widget_set_sensitive (ui->url_remove, TRUE);
		gtk_widget_set_sensitive (ui->url_enable, TRUE);
	} else {
		gtk_widget_set_sensitive (ui->url_edit, FALSE);
		gtk_widget_set_sensitive (ui->url_remove, FALSE);
		gtk_widget_set_sensitive (ui->url_enable, FALSE);
	}
}

static void
url_add_clicked (GtkButton *button, PublishUIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *url_editor;
	EPublishUri *uri;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ui->treeview));
	url_editor = url_editor_dialog_new (model, NULL);
	url_editor_dialog_run ((UrlEditorDialog *) url_editor);

	uri = URL_EDITOR_DIALOG (url_editor)->uri;
	if (uri->location) {
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    URL_LIST_ENABLED_COLUMN, uri->enabled,
				    URL_LIST_LOCATION_COLUMN, uri->location,
				    URL_LIST_URL_COLUMN, uri, -1);
		url_list_changed (ui);
		publish_uris = g_slist_prepend (publish_uris, uri);
		add_timeout (uri);
		publish_uri_async (uri);
	} else {
		g_free (uri);
	}
	gtk_widget_destroy (url_editor);
}

static void
url_edit_clicked (GtkButton *button, PublishUIData *ui)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EPublishUri *uri;
		GtkWidget *url_editor;
		guint id;

		gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 2, &uri, -1);
		url_editor = url_editor_dialog_new (model, uri);
		url_editor_dialog_run ((UrlEditorDialog *) url_editor);

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    URL_LIST_ENABLED_COLUMN, uri->enabled,
				    URL_LIST_LOCATION_COLUMN, uri->location,
				    URL_LIST_URL_COLUMN, uri, -1);

		id = GPOINTER_TO_UINT (g_hash_table_lookup (uri_timeouts, uri));
		if (id)
			g_source_remove (id);
		add_timeout (uri);
		url_list_changed (ui);
		publish_uri_async (uri);
	}
}

static void
url_list_double_click (GtkTreeView       *treeview,
		       GtkTreePath       *path,
		       GtkTreeViewColumn *column,
		       PublishUIData     *ui)
{
	url_edit_clicked (NULL, ui);
}

static void
url_remove_clicked (GtkButton *button, PublishUIData *ui)
{
	EPublishUri *url = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *confirm;
	gint response;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, URL_LIST_URL_COLUMN, &url, -1);

	confirm = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			                  GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					  _("Are you sure you want to remove this URL?"));
	gtk_dialog_add_button (GTK_DIALOG (confirm), GTK_STOCK_CANCEL, GTK_RESPONSE_NO);
	gtk_dialog_add_button (GTK_DIALOG (confirm), GTK_STOCK_REMOVE, GTK_RESPONSE_YES);
	gtk_dialog_set_default_response (GTK_DIALOG (confirm), GTK_RESPONSE_CANCEL);

	response = gtk_dialog_run (GTK_DIALOG (confirm));
	gtk_widget_destroy (confirm);

	if (response == GTK_RESPONSE_YES) {
		int len;
		guint id;
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

		len = gtk_tree_model_iter_n_children (model, NULL);
		if (len > 0) {
			gtk_tree_selection_select_iter (selection, &iter);
		} else {
			gtk_widget_set_sensitive (ui->url_edit, FALSE);
			gtk_widget_set_sensitive (ui->url_remove, FALSE);
			gtk_widget_set_sensitive (ui->url_enable, FALSE);
		}

		publish_uris = g_slist_remove (publish_uris, url);
		id = GPOINTER_TO_UINT (g_hash_table_lookup (uri_timeouts, url));
		if (id)
			g_source_remove (id);

		g_free (url);
		url_list_changed (ui);
	}
}

static void
url_enable_clicked (GtkButton *button, PublishUIData *ui)
{
	EPublishUri *url = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, URL_LIST_URL_COLUMN, &url, -1);
		url->enabled = !url->enabled;
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, URL_LIST_URL_COLUMN, url->enabled, -1);
		gtk_tree_selection_select_iter (selection, &iter);
		url_list_changed (ui);
	}
}

void
online_state_changed (EPlugin *ep, ESEventTargetState *target)
{
	online = target->state;
	if (online)
		while (queued_publishes)
			publish (queued_publishes->data);
}

GtkWidget *
publish_calendar_locations (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	GladeXML *xml;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkWidget *toplevel;
	PublishUIData *ui = g_new0 (PublishUIData, 1);
	GSList *l;
	GtkTreeIter iter;
	GConfClient *client;
	char *gladefile;

	gladefile = g_build_filename (EVOLUTION_PLUGINDIR,
				      "publish-calendar.glade",
				      NULL);
	xml = glade_xml_new (gladefile, "toplevel", NULL);
	g_free (gladefile);

	ui->treeview = glade_xml_get_widget (xml, "url list");
	if (store == NULL)
		store = gtk_list_store_new (URL_LIST_N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
	else 
		gtk_list_store_clear (store);

	gtk_tree_view_set_model (GTK_TREE_VIEW (ui->treeview), GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer), "activatable", TRUE, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui->treeview), -1, _("Enabled"),
			                             renderer, "active", URL_LIST_ENABLED_COLUMN, NULL);
	g_signal_connect (G_OBJECT (renderer), "toggled", G_CALLBACK (url_list_enable_toggled), ui);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui->treeview), -1, _("Location"),
			                             renderer, "text", URL_LIST_LOCATION_COLUMN, NULL);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (selection_changed), ui);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (ui->treeview), TRUE);
	g_signal_connect (G_OBJECT (ui->treeview), "row-activated", G_CALLBACK (url_list_double_click), ui);

	ui->url_add = glade_xml_get_widget (xml, "url add");
	ui->url_edit = glade_xml_get_widget (xml, "url edit");
	ui->url_remove = glade_xml_get_widget (xml, "url remove");
	ui->url_enable = glade_xml_get_widget (xml, "url enable");
	g_signal_connect (G_OBJECT (ui->url_add), "clicked", G_CALLBACK (url_add_clicked), ui);
	g_signal_connect (G_OBJECT (ui->url_edit), "clicked", G_CALLBACK (url_edit_clicked), ui);
	g_signal_connect (G_OBJECT (ui->url_remove), "clicked", G_CALLBACK (url_remove_clicked), ui);
	g_signal_connect (G_OBJECT (ui->url_enable), "clicked", G_CALLBACK (url_enable_clicked), ui);
	gtk_widget_set_sensitive (GTK_WIDGET (ui->url_edit), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (ui->url_remove), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (ui->url_enable), FALSE);

	client = gconf_client_get_default ();
	l = publish_uris;
	while (l) {
		EPublishUri *url = (EPublishUri *) l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    URL_LIST_ENABLED_COLUMN, url->enabled,
				    URL_LIST_LOCATION_COLUMN, url->location,
				    URL_LIST_URL_COLUMN, url, -1);

		l = g_slist_next (l);
	}
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
		gtk_tree_selection_select_iter (selection, &iter);

	toplevel = glade_xml_get_widget (xml, "toplevel");
	gtk_widget_show_all (toplevel);
	gtk_box_pack_start (GTK_BOX (data->parent), toplevel, FALSE, TRUE, 0);

	g_object_unref (xml);

	return toplevel;
}

static gpointer 
publish_urls (gpointer data)
{
	GSList *l;

	for (l = publish_uris; l; l = g_slist_next (l)) {
		EPublishUri *uri = l->data;
		publish (uri);
	}

	return GINT_TO_POINTER (0);
}

void
action_publish (EPlugin *ep, ECalMenuTargetSelect *t)
{
	GThread *thread = NULL;
	GError *error = NULL;
	
	thread = g_thread_create ((GThreadFunc) publish_urls, NULL, FALSE, &error);
	if (!thread) {
		g_warning (G_STRLOC ": %s", error->message);
		g_error_free (error);
	}

}

static void
publish_uris_set_timeout (GSList *uris)
{
	GSList *l;

	uri_timeouts = g_hash_table_new (g_direct_hash, g_direct_equal);
	l = uris;

	while (l) {
		gchar *xml = l->data;

		EPublishUri *uri = e_publish_uri_from_xml (xml);

		if (!uri->location) {
			g_free (uri);
			continue;
		}

		publish_uris = g_slist_prepend (publish_uris, uri);

		/* Add a timeout based on the last publish time */
		add_offset_timeout (uri);

		l = g_slist_next (l);
	}
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);
}

int
e_plugin_lib_enable (EPlugin *ep, int enable)
{
	GSList *uris;
	GConfClient *client;

	if (enable) {
		GThread *thread = NULL;
		GError *error = NULL;


		client = gconf_client_get_default ();
		uris = gconf_client_get_list (client, "/apps/evolution/calendar/publish/uris", GCONF_VALUE_STRING, NULL);
		thread = g_thread_create ((GThreadFunc) publish_uris_set_timeout, uris, FALSE, &error);
		if (!thread) {
			g_warning ("Could create thread to set timeout for publishing uris : %s", error->message);
			g_error_free (error);
		}
		g_object_unref (client);
	}

	return 0;
}
