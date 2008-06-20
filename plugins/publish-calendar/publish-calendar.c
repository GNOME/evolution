/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libedataserver/e-url.h>
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
publish_online (EPublishUri *uri, GFile *file, GError **perror)
{
	GOutputStream *stream;
	GError *error = NULL;

	stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));

	if (!stream || error) {
		if (stream)
			g_object_unref (stream);

		if (perror) {
			*perror = error;
		} else if (error) {
			g_warning ("Couldn't open %s: %s", uri->location, error->message);
			g_error_free (error);
		} else {
			g_warning ("Couldn't open %s: Unknown error", uri->location);
		}
		return;
	}

	switch (uri->publish_format) {
	case URI_PUBLISH_AS_ICAL:
		publish_calendar_as_ical (stream, uri);
		break;
	case URI_PUBLISH_AS_FB:
		publish_calendar_as_fb (stream, uri);
		break;
	/*
	case URI_PUBLISH_AS_HTML:
		publish_calendar_as_html (handle, uri);
		break;
	*/
	}

	update_timestamp (uri);

	g_output_stream_close (stream, NULL, NULL);
	g_object_unref (stream);
}

static void
unmount_done_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;

	g_mount_unmount_finish (G_MOUNT (source_object), res, &error);

	if (error) {
		g_warning ("Unmount failed: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (source_object);
}

struct mnt_struct {
	EPublishUri *uri;
	GFile *file;
	GMountOperation *mount_op;
};

static void
mount_ready_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	struct mnt_struct *ms = (struct mnt_struct *) user_data;
	GError *error = NULL;
	GMount *mount;

	g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error);

	if (error) {
		if (error->code != G_IO_ERROR_CANCELLED)
			g_warning ("Mount of %s failed: %s", ms->uri->location, error->message);

		g_error_free (error);
		if (ms)
			g_object_unref (ms->mount_op);
		g_free (ms);

		g_object_unref (source_object);

		return;
	}

	g_return_if_fail (ms != NULL);

	publish_online (ms->uri, ms->file, NULL);

	g_object_unref (ms->mount_op);
	g_free (ms);

	mount = g_file_find_enclosing_mount (G_FILE (source_object), NULL, NULL);
	if (mount)
		g_mount_unmount (mount, G_MOUNT_UNMOUNT_NONE, NULL, unmount_done_cb, NULL);

	g_object_unref (source_object);
}

static void
ask_password (GMountOperation *op, const gchar *message, const gchar *default_user, const gchar *default_domain, GAskPasswordFlags flags, gpointer user_data)
{
	struct mnt_struct *ms = (struct mnt_struct *) user_data;
	gchar *username, *password;
	gboolean req_pass = FALSE;
	EUri *euri;

	g_return_if_fail (ms != NULL);

	/* we can ask only for a password */
	if ((flags & G_ASK_PASSWORD_NEED_PASSWORD) == 0)
		return;

	euri = e_uri_new (ms->uri->location);
	username = euri->user;
	password = e_passwords_get_password ("Calendar", ms->uri->location);
	req_pass = ((username && *username) && !(ms->uri->service_type == TYPE_ANON_FTP &&
			!strcmp (username, "anonymous"))) ? TRUE:FALSE;

	if (!password && req_pass) {
		gboolean remember = FALSE;

		password = e_passwords_ask_password (_("Enter password"), "", ms->uri->location, message,
					     E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET|E_PASSWORDS_ONLINE,
					     &remember,
					     NULL);

		if (!password) {
			/* user canceled password dialog */
			g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
			e_uri_free (euri);

			return;
		}
	}

	if (!req_pass)
		g_mount_operation_set_anonymous (op, TRUE);
	else {
		g_mount_operation_set_anonymous (op, FALSE);
		g_mount_operation_set_username  (op, username);
		g_mount_operation_set_password  (op, password);
	}

	g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);

	e_uri_free (euri);
}

static void
ask_question (GMountOperation *op, const char *message, const char *choices[])
{
	/* this has been stolen from file-chooser */
	GtkWidget *dialog;
	int cnt, len;
	char *primary;
	const char *secondary = NULL;
	int res;

	primary = strstr (message, "\n");
	if (primary) {
		secondary = primary + 1;
		primary = g_strndup (message, strlen (message) - strlen (primary));
	}

	gdk_threads_enter ();
	dialog = gtk_message_dialog_new (NULL,
					 0, GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE, "%s", primary);
	g_free (primary);

	if (secondary) {
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s", secondary);
	}
	
	if (choices) {
		/* First count the items in the list then 
		 * add the buttons in reverse order */
		for (len = 0; choices[len] != NULL; len++) {
			;
		}
		
		for (cnt = len - 1; cnt >= 0; cnt--) {
			gtk_dialog_add_button (GTK_DIALOG (dialog), choices[cnt], cnt);
		}
	}

	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res >= 0) {
		g_mount_operation_set_choice (op, res);
		g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
	} else {
		g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
	gdk_threads_leave ();
}

static void
mount_first (EPublishUri *uri, GFile *file)
{
	struct mnt_struct *ms = g_malloc (sizeof (struct mnt_struct));

	ms->uri = uri;
	ms->file = g_object_ref (file);
	ms->mount_op = g_mount_operation_new ();

	g_signal_connect (ms->mount_op, "ask-password", G_CALLBACK (ask_password), ms);
	g_signal_connect (ms->mount_op, "ask-question", G_CALLBACK (ask_question), ms);	

	g_file_mount_enclosing_volume (file, G_MOUNT_MOUNT_NONE, ms->mount_op, NULL, mount_ready_cb, ms);
}

static void
publish (EPublishUri *uri)
{
	if (online) {
		GError *error = NULL;
		GFile *file;

		if (g_slist_find (queued_publishes, uri))
			queued_publishes = g_slist_remove (queued_publishes, uri);

		if (!uri->enabled)
			return;

		file = g_file_new_for_uri (uri->location);

		g_return_if_fail (file != NULL);

		publish_online (uri, file, &error);

		if (error && error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_MOUNTED) {
			g_error_free (error);
			error = NULL;

			mount_first (uri, file);
		}

		if (error) {
			g_warning ("Couldn't open %s: %s", uri->location, error->message);
			g_error_free (error);
		}

		g_object_unref (file);
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

		if(url->enabled)
			gtk_widget_set_sensitive (ui->url_enable, FALSE);
		else
			gtk_widget_set_sensitive (ui->url_enable, TRUE);

		gtk_list_store_set (GTK_LIST_STORE (model), &iter, URL_LIST_ENABLED_COLUMN, url->enabled, -1);
	}

	gtk_tree_path_free (path);
}

static void
selection_changed (GtkTreeSelection *selection, PublishUIData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	EPublishUri *url = NULL;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, URL_LIST_URL_COLUMN, &url, -1);
		gtk_widget_set_sensitive (ui->url_edit, TRUE);
		gtk_widget_set_sensitive (ui->url_remove, TRUE);

		if(url->enabled)
			gtk_widget_set_sensitive (ui->url_enable, FALSE);
		else
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

		if(url->enabled)
			gtk_widget_set_sensitive (ui->url_enable, FALSE);
		else
			gtk_widget_set_sensitive (ui->url_enable, TRUE);

		gtk_list_store_set (GTK_LIST_STORE (model), &iter, URL_LIST_ENABLED_COLUMN, url->enabled, -1);
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

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
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
			l = g_slist_next (l);
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
