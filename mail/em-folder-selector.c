/*
 *
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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <string.h>
#include <glib/gi18n.h>
#include <e-util/e-util.h>

#include "e-mail-session.h"
#include "em-folder-tree.h"
#include "em-folder-selector.h"
#include "em-folder-utils.h"
#include "em-utils.h"

#define d(x)

#define EM_FOLDER_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FOLDER_SELECTOR, EMFolderSelectorPrivate))

struct _EMFolderSelectorPrivate {
	EMailBackend *backend;
	EMFolderTree *folder_tree;  /* not referenced */
};

enum {
	PROP_0,
	PROP_BACKEND
};

/* XXX EMFolderSelector is an EAlertSink, but it just uses the default
 *     message dialog implementation.  We should do something nicer. */

G_DEFINE_TYPE_WITH_CODE (
	EMFolderSelector,
	em_folder_selector,
	GTK_TYPE_DIALOG,
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, NULL))

static void
folder_selector_set_backend (EMFolderSelector *emfs,
                             EMailBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (emfs->priv->backend == NULL);

	emfs->priv->backend = g_object_ref (backend);
}

static void
folder_selector_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			folder_selector_set_backend (
				EM_FOLDER_SELECTOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
folder_selector_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			g_value_set_object (
				value,
				em_folder_tree_get_backend (
				EM_FOLDER_TREE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
folder_selector_dispose (GObject *object)
{
	EMFolderSelector *emfs = EM_FOLDER_SELECTOR (object);
	GtkTreeModel *model;

	if (emfs->priv->backend != NULL) {
		g_object_unref (emfs->priv->backend);
		emfs->priv->backend = NULL;
	}

	if (emfs->created_id != 0) {
		GtkTreeView *tree_view;

		tree_view = GTK_TREE_VIEW (emfs->priv->folder_tree);
		model = gtk_tree_view_get_model (tree_view);
		g_signal_handler_disconnect (model, emfs->created_id);
		emfs->created_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_folder_selector_parent_class)->dispose (object);
}

static void
folder_selector_finalize (GObject *object)
{
	EMFolderSelector *emfs = EM_FOLDER_SELECTOR (object);

	g_free (emfs->selected_uri);
	g_free (emfs->created_uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_folder_selector_parent_class)->finalize (object);
}

static void
em_folder_selector_class_init (EMFolderSelectorClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMFolderSelectorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = folder_selector_set_property;
	object_class->get_property = folder_selector_get_property;
	object_class->dispose = folder_selector_dispose;
	object_class->finalize = folder_selector_finalize;

	g_object_class_install_property (
		object_class,
		PROP_BACKEND,
		g_param_spec_object (
			"backend",
			NULL,
			NULL,
			E_TYPE_MAIL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
em_folder_selector_init (EMFolderSelector *emfs)
{
	emfs->priv = EM_FOLDER_SELECTOR_GET_PRIVATE (emfs);
}

static void
emfs_response (GtkWidget *dialog, gint response, EMFolderSelector *emfs)
{
	EMFolderTree *folder_tree;
	EMailBackend *backend;

	if (response != EM_FOLDER_SELECTOR_RESPONSE_NEW)
		return;

	folder_tree = em_folder_selector_get_folder_tree (emfs);

	g_object_set_data (
		G_OBJECT (folder_tree), "select", GUINT_TO_POINTER (1));

	backend = em_folder_tree_get_backend (folder_tree);

	em_folder_utils_create_folder (
		GTK_WINDOW (dialog), backend, folder_tree, NULL);

	g_signal_stop_emission_by_name (emfs, "response");
}

static void
emfs_create_name_changed (GtkEntry *entry, EMFolderSelector *emfs)
{
	EMFolderTree *folder_tree;
	gchar *path;
	const gchar *text = NULL;
	gboolean active;

	if (gtk_entry_get_text_length (emfs->name_entry) > 0)
		text = gtk_entry_get_text (emfs->name_entry);

	folder_tree = em_folder_selector_get_folder_tree (emfs);

	path = em_folder_tree_get_selected_uri (folder_tree);
	active = text && path && !strchr (text, '/');
	g_free (path);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (emfs), GTK_RESPONSE_OK, active);
}

static void
folder_selected_cb (EMFolderTree *emft,
                    CamelStore *store,
                    const gchar *folder_name,
                    CamelFolderInfoFlags flags,
                    EMFolderSelector *emfs)
{
	if (emfs->name_entry)
		emfs_create_name_changed (emfs->name_entry, emfs);
	else
		gtk_dialog_set_response_sensitive (
			GTK_DIALOG (emfs), GTK_RESPONSE_OK, TRUE);
}

static void
folder_activated_cb (EMFolderTree *emft,
                     CamelStore *store,
                     const gchar *folder_name,
                     EMFolderSelector *emfs)
{
	gtk_dialog_response ((GtkDialog *) emfs, GTK_RESPONSE_OK);
}

static void
folder_selector_construct (EMFolderSelector *emfs,
                           guint32 flags,
                           const gchar *title,
                           const gchar *text,
                           const gchar *oklabel)
{
	EMailBackend *backend;
	GtkWidget *content_area;
	GtkWidget *container;
	GtkWidget *widget;

	backend = em_folder_selector_get_backend (emfs);

	gtk_window_set_default_size (GTK_WINDOW (emfs), 350, 300);
	gtk_window_set_title (GTK_WINDOW (emfs), title);
	gtk_container_set_border_width (GTK_CONTAINER (emfs), 6);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (emfs));
	gtk_box_set_spacing (GTK_BOX (content_area), 6);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 6);

	container = content_area;

	emfs->flags = flags;
	if (flags & EM_FOLDER_SELECTOR_CAN_CREATE) {
		gtk_dialog_add_button (
			GTK_DIALOG (emfs), GTK_STOCK_NEW,
			EM_FOLDER_SELECTOR_RESPONSE_NEW);
		g_signal_connect (
			emfs, "response",
			G_CALLBACK (emfs_response), emfs);
	}

	gtk_dialog_add_buttons (
		GTK_DIALOG (emfs),
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		oklabel ? oklabel : GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (emfs), GTK_RESPONSE_OK, FALSE);
	gtk_dialog_set_default_response (
		GTK_DIALOG (emfs), GTK_RESPONSE_OK);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 6);
	gtk_widget_show (widget);

	container = widget;

	widget = em_folder_tree_new (backend, E_ALERT_SINK (emfs));
	emu_restore_folder_tree_state (EM_FOLDER_TREE (widget));
	gtk_container_add (GTK_CONTAINER (container), widget);
	emfs->priv->folder_tree = EM_FOLDER_TREE (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "folder-selected",
		G_CALLBACK (folder_selected_cb), emfs);
	g_signal_connect (
		widget, "folder-activated",
		G_CALLBACK (folder_activated_cb), emfs);

	container = content_area;

	if (text != NULL) {
		widget = gtk_label_new (text);
		gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
		gtk_widget_show (widget);

		gtk_box_pack_end (GTK_BOX (container), widget, FALSE, TRUE, 6);
	}

	gtk_widget_grab_focus (GTK_WIDGET (emfs->priv->folder_tree));
}

GtkWidget *
em_folder_selector_new (GtkWindow *parent,
                        EMailBackend *backend,
                        guint32 flags,
                        const gchar *title,
                        const gchar *text,
                        const gchar *oklabel)
{
	EMFolderSelector *emfs;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	emfs = g_object_new (
		EM_TYPE_FOLDER_SELECTOR,
		"transient-for", parent,
		"backend", backend, NULL);
	folder_selector_construct (emfs, flags, title, text, oklabel);

	return (GtkWidget *) emfs;
}

static void
emfs_create_name_activate (GtkEntry *entry, EMFolderSelector *emfs)
{
	if (gtk_entry_get_text_length (emfs->name_entry) > 0) {
		EMFolderTree *folder_tree;
		gchar *path;
		const gchar *text;

		text = gtk_entry_get_text (emfs->name_entry);

		folder_tree = em_folder_selector_get_folder_tree (emfs);
		path = em_folder_tree_get_selected_uri (folder_tree);

		if (text && path && !strchr (text, '/'))
			g_signal_emit_by_name (emfs, "response", GTK_RESPONSE_OK);
		g_free (path);
	}
}

GtkWidget *
em_folder_selector_create_new (GtkWindow *parent,
                               EMailBackend *backend,
                               guint32 flags,
                               const gchar *title,
                               const gchar *text)
{
	EMFolderSelector *emfs;
	EMFolderTree *folder_tree;
	GtkWidget *hbox, *w;
	GtkWidget *container;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	/* remove the CREATE flag if it is there since that's the
	 * whole purpose of this dialog */
	flags &= ~EM_FOLDER_SELECTOR_CAN_CREATE;

	emfs = g_object_new (
		EM_TYPE_FOLDER_SELECTOR,
		"transient-for", parent,
		"backend", backend, NULL);
	folder_selector_construct (emfs, flags, title, text, _("C_reate"));

	folder_tree = em_folder_selector_get_folder_tree (emfs);
	em_folder_tree_set_excluded (folder_tree, EMFT_EXCLUDE_NOINFERIORS);

	hbox = gtk_hbox_new (FALSE, 0);
	w = gtk_label_new_with_mnemonic (_("Folder _name:"));
	gtk_box_pack_start ((GtkBox *) hbox, w, FALSE, FALSE, 6);
	emfs->name_entry = (GtkEntry *) gtk_entry_new ();
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (w), (GtkWidget *) emfs->name_entry);
	g_signal_connect (
		emfs->name_entry, "changed",
		G_CALLBACK (emfs_create_name_changed), emfs);
	g_signal_connect (
		emfs->name_entry, "activate",
		G_CALLBACK (emfs_create_name_activate), emfs);
	gtk_box_pack_start (
		(GtkBox *) hbox, (GtkWidget *) emfs->name_entry,
		TRUE, FALSE, 6);
	gtk_widget_show_all (hbox);

	container = gtk_dialog_get_content_area (GTK_DIALOG (emfs));
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, TRUE, 0);

	gtk_widget_grab_focus ((GtkWidget *) emfs->name_entry);

	return (GtkWidget *) emfs;
}

EMailBackend *
em_folder_selector_get_backend (EMFolderSelector *emfs)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (emfs), NULL);

	return emfs->priv->backend;
}

EMFolderTree *
em_folder_selector_get_folder_tree (EMFolderSelector *emfs)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (emfs), NULL);

	return emfs->priv->folder_tree;
}

const gchar *
em_folder_selector_get_selected_uri (EMFolderSelector *emfs)
{
	EMFolderTree *folder_tree;
	gchar *uri;
	const gchar *name;

	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (emfs), NULL);

	folder_tree = em_folder_selector_get_folder_tree (emfs);
	uri = em_folder_tree_get_selected_uri (folder_tree);

	if (uri == NULL) {
		d(printf ("no selected folder?\n"));
		return NULL;
	}

	if (emfs->name_entry) {
		CamelProvider *provider;
		CamelURL *url;

		provider = camel_provider_get (uri, NULL);

		name = gtk_entry_get_text (emfs->name_entry);

		url = camel_url_new (uri, NULL);
		if (provider && (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)) {
			gchar *path;

			if (url->fragment)
				path = g_strdup_printf ("%s/%s", url->fragment, name);
			else
				path = g_strdup (name);

			camel_url_set_fragment (url, path);
			g_free (path);
		} else {
			gchar *path;

			path = g_strdup_printf (
				"%s/%s", (url->path == NULL ||
				strcmp (url->path, "/") == 0) ? "":
				url->path, name);
			camel_url_set_path (url, path);
			g_free (path);
		}

		g_free (emfs->selected_uri);
		emfs->selected_uri = camel_url_to_string (url, 0);

		camel_url_free (url);
		uri = emfs->selected_uri;
	}

	return uri;
}
