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
 *		Sankar P <psankar@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include <libemail-utils/e-account-utils.h>

#include <mail/em-config.h>

typedef struct _epif_data EPImapFeaturesData;
struct _epif_data {
	GtkWidget *all_headers;
	GtkWidget *basic_headers;
	GtkWidget *mailing_list_headers;
	GtkWidget *custom_headers_box;

	GtkEntry *entry_header;

	GtkButton *add_header;
	GtkButton *remove_header;

	GtkTreeView *custom_headers_tree;
	GtkTreeStore *store;

	gchar **custom_headers_array;
};

static EPImapFeaturesData *ui = NULL;

void imap_headers_abort (EPlugin *efp, EConfigHookItemFactoryData *data);
void imap_headers_commit (EPlugin *efp, EConfigHookItemFactoryData *data);
GtkWidget * org_gnome_imap_headers (EPlugin *epl, EConfigHookItemFactoryData *data);
gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

void
imap_headers_abort (EPlugin *efp,
                    EConfigHookItemFactoryData *data)
{
	/* Nothing to do here */
}

void
imap_headers_commit (EPlugin *efp,
                     EConfigHookItemFactoryData *data)
{
	EMConfigTargetSettings *target;
	CamelFetchHeadersType fetch_headers;
	gboolean use_imap = g_getenv ("USE_IMAP") != NULL;
	const gchar *protocol;

	target = (EMConfigTargetSettings *) data->config->target;
	protocol = target->storage_protocol;

	if (g_strcmp0 (protocol, "imap") == 0 ||
	    (use_imap && g_strcmp0 (protocol, "groupwise") == 0)) {

		GtkTreeModel *model;
		GtkTreeIter iter;
		gint n_children;
		gchar **strv = NULL;
		gboolean valid;
		gint ii = 0;

		model = gtk_tree_view_get_model (ui->custom_headers_tree);
		n_children = gtk_tree_model_iter_n_children (model, NULL);

		if (n_children > 0)
			strv = g_new0 (gchar *, n_children + 1);

		valid = gtk_tree_model_get_iter_first (model, &iter);

		while (valid) {
			gchar *header;

			g_warn_if_fail (ii < n_children);
			gtk_tree_model_get (model, &iter, 0, &header, -1);
			strv[ii++] = g_strstrip (header);

			valid = gtk_tree_model_iter_next (model, &iter);
		}

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui->all_headers)))
			fetch_headers = CAMEL_FETCH_HEADERS_ALL;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ui->basic_headers)))
			fetch_headers = CAMEL_FETCH_HEADERS_BASIC;
		else
			fetch_headers = CAMEL_FETCH_HEADERS_BASIC_AND_MAILING_LIST;

		g_object_set (
			target->storage_settings,
			"fetch-headers", fetch_headers,
			"fetch-headers-extra", strv, NULL);

		g_strfreev (strv);
	}
}

/* return true is the header is considered valid */
static gboolean
epif_header_is_valid (const gchar *header)
{
	gint len = g_utf8_strlen (header, -1);

	if (header[0] == 0
	    || g_utf8_strchr (header, len, ':') != NULL
	    || g_utf8_strchr (header, len, ' ') != NULL)
		return FALSE;

	return TRUE;
}

static void
epif_add_sensitivity (EPImapFeaturesData *ui)
{
	const gchar *entry_contents;
	GtkTreeIter iter;
	gboolean valid;

	/* the add header button should be sensitive if the text box contains
	 * a valid header string, that is not a duplicate with something already
	 * in the list view */
	entry_contents = gtk_entry_get_text (GTK_ENTRY (ui->entry_header));
	if (!epif_header_is_valid (entry_contents)) {
		gtk_widget_set_sensitive (GTK_WIDGET (ui->add_header), FALSE);
		return;
	}

	/* check if this is a duplicate */
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ui->store), &iter);
	while (valid) {
		gchar *header_name;

		gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &iter,
						    0, &header_name,
						    -1);
		if (g_ascii_strcasecmp (header_name, entry_contents) == 0) {
			gtk_widget_set_sensitive (GTK_WIDGET (ui->add_header), FALSE);
			return;
		}

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (ui->store), &iter);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (ui->add_header), TRUE);
}

static void
epif_add_header (GtkButton *button,
                 EPImapFeaturesData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	model = gtk_tree_view_get_model (ui->custom_headers_tree);
	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0, gtk_entry_get_text (ui->entry_header), -1);

	selection = gtk_tree_view_get_selection (ui->custom_headers_tree);
	gtk_tree_selection_select_iter (selection, &iter);

	gtk_entry_set_text (ui->entry_header, "");
	epif_add_sensitivity (ui);
}

static void
epif_tv_selection_changed (GtkTreeSelection *selection,
                           GtkWidget *button)
{
	g_return_if_fail (selection != NULL);
	g_return_if_fail (button != NULL);

	gtk_widget_set_sensitive (button, gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static void
epif_remove_header_clicked (GtkButton *button,
                            EPImapFeaturesData *ui)
{
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean valid = TRUE;

	select = gtk_tree_view_get_selection (ui->custom_headers_tree);

	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		path = gtk_tree_model_get_path (model, &iter);
		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

		if (gtk_tree_path_prev (path)) {
			gtk_tree_model_get_iter (model, &iter, path);
		} else {
			valid = gtk_tree_model_get_iter_first (model, &iter);
		}

		if (valid)
			gtk_tree_selection_select_iter (select, &iter);
	}

	epif_add_sensitivity (ui);
}

static void
epif_entry_changed (GtkWidget *entry,
                    EPImapFeaturesData *ui)
{
	epif_add_sensitivity (ui);
}

GtkWidget *
org_gnome_imap_headers (EPlugin *epl,
                        EConfigHookItemFactoryData *data)
{
	EMConfigTargetSettings *target;
	GtkWidget *vbox;
	GtkBuilder *builder;
	GtkWidget *button;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	CamelFetchHeadersType fetch_headers = 0;
	gchar **extra_headers = NULL;
	gboolean use_imap = g_getenv ("USE_IMAP") != NULL;
	const gchar *protocol;
	guint ii, length = 0;

	ui = g_new0 (EPImapFeaturesData, 1);

	target = (EMConfigTargetSettings *) data->config->target;
	protocol = target->storage_protocol;

	if (g_strcmp0 (protocol, "imap") != 0 &&
	    !(use_imap && g_strcmp0 (protocol, "groupwise") == 0))
		return NULL;

	g_object_get (
		target->storage_settings,
		"fetch-headers", &fetch_headers,
		"fetch-headers-extra", &extra_headers, NULL);

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "imap-headers.ui");

	vbox = e_builder_get_widget (builder, "vbox2");
	ui->all_headers = e_builder_get_widget (builder, "allHeaders");
	ui->basic_headers = e_builder_get_widget (builder, "basicHeaders");
	ui->mailing_list_headers = e_builder_get_widget (builder, "mailingListHeaders");
	ui->custom_headers_box = e_builder_get_widget (builder, "custHeaderHbox");
	ui->custom_headers_tree = GTK_TREE_VIEW(e_builder_get_widget (builder, "custHeaderTree"));
	ui->add_header = GTK_BUTTON(e_builder_get_widget (builder, "addHeader"));
	ui->remove_header = GTK_BUTTON(e_builder_get_widget (builder, "removeHeader"));
	ui->entry_header = GTK_ENTRY (e_builder_get_widget (builder, "customHeaderEntry"));

	g_object_bind_property (
		ui->all_headers, "active",
		ui->custom_headers_box, "sensitive",
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	ui->store = gtk_tree_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model (ui->custom_headers_tree, GTK_TREE_MODEL (ui->store));

	selection = gtk_tree_view_get_selection (ui->custom_headers_tree);

	if (extra_headers != NULL)
		length = g_strv_length (extra_headers);

	for (ii = 0; ii < length; ii++) {

		/* Skip empty strings. */
		g_strstrip (extra_headers[ii]);
		if (*extra_headers[ii] == '\0')
			continue;

		gtk_tree_store_append (ui->store, &iter, NULL);
		gtk_tree_store_set (ui->store, &iter, 0, extra_headers[ii], -1);
	}

	switch (fetch_headers) {
		case CAMEL_FETCH_HEADERS_ALL:
			button = ui->all_headers;
			break;
		case CAMEL_FETCH_HEADERS_BASIC:
			button = ui->basic_headers;
			break;
		default:
			button = ui->mailing_list_headers;
			break;
	}

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (button), TRUE);

	g_strfreev (extra_headers);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Custom Headers"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column (ui->custom_headers_tree , column);

	gtk_widget_set_sensitive (GTK_WIDGET (ui->add_header), FALSE);
	epif_tv_selection_changed (selection, GTK_WIDGET (ui->remove_header));

	g_signal_connect (
		ui->add_header, "clicked",
		G_CALLBACK (epif_add_header), ui);
	g_signal_connect (
		ui->remove_header, "clicked",
		G_CALLBACK (epif_remove_header_clicked), ui);
	g_signal_connect (
		ui->entry_header, "changed",
		G_CALLBACK (epif_entry_changed), ui);
	g_signal_connect (
		ui->entry_header, "activate",
		G_CALLBACK (epif_add_header), ui);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (epif_tv_selection_changed), ui->remove_header);

	gtk_notebook_append_page ((GtkNotebook *)(data->parent), vbox, gtk_label_new(_("IMAP Headers")));
	gtk_container_child_set (GTK_CONTAINER (data->parent), vbox, "tab-fill", FALSE, "tab-expand", FALSE, NULL);
	gtk_widget_show_all (vbox);

	return GTK_WIDGET (vbox);
}
