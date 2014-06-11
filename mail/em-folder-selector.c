/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "em-folder-selector.h"

#include <config.h>
#include <string.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#include "em-folder-tree.h"
#include "em-folder-utils.h"
#include "em-utils.h"

#define d(x)

#define EM_FOLDER_SELECTOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FOLDER_SELECTOR, EMFolderSelectorPrivate))

struct _EMFolderSelectorPrivate {
	EMFolderTree *folder_tree;  /* not referenced */
	EMFolderTreeModel *model;
};

enum {
	PROP_0,
	PROP_MODEL
};

/* XXX EMFolderSelector is an EAlertSink, but it just uses the default
 *     message dialog implementation.  We should do something nicer. */

G_DEFINE_TYPE_WITH_CODE (
	EMFolderSelector,
	em_folder_selector,
	GTK_TYPE_DIALOG,
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, NULL))

static void
folder_selector_set_model (EMFolderSelector *emfs,
                           EMFolderTreeModel *model)
{
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (emfs->priv->model == NULL);

	emfs->priv->model = g_object_ref (model);
}

static void
folder_selector_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODEL:
			folder_selector_set_model (
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
		case PROP_MODEL:
			g_value_set_object (
				value,
				em_folder_selector_get_model (
				EM_FOLDER_SELECTOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
folder_selector_dispose (GObject *object)
{
	EMFolderSelector *emfs = EM_FOLDER_SELECTOR (object);

	if (emfs->created_id != 0) {
		g_signal_handler_disconnect (
			emfs->priv->model, emfs->created_id);
		emfs->created_id = 0;
	}

	if (emfs->priv->model != NULL) {
		if (emfs->priv->model && emfs->priv->model != em_folder_tree_model_get_default ())
			em_folder_tree_model_remove_all_stores (emfs->priv->model);

		g_object_unref (emfs->priv->model);
		emfs->priv->model = NULL;
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
		PROP_MODEL,
		g_param_spec_object (
			"model",
			NULL,
			NULL,
			EM_TYPE_FOLDER_TREE_MODEL,
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
emfs_response (GtkWidget *dialog,
               gint response,
               EMFolderSelector *emfs)
{
	EMFolderTree *folder_tree;
	EMailSession *session;

	if (response != EM_FOLDER_SELECTOR_RESPONSE_NEW)
		return;

	folder_tree = em_folder_selector_get_folder_tree (emfs);

	g_object_set_data (
		G_OBJECT (folder_tree), "select", GUINT_TO_POINTER (1));

	session = em_folder_tree_get_session (folder_tree);

	em_folder_utils_create_folder (
		GTK_WINDOW (dialog), session, folder_tree,
		em_folder_selector_get_selected_uri (emfs));

	g_signal_stop_emission_by_name (emfs, "response");
}

static void
emfs_create_name_changed (GtkEntry *entry,
                          EMFolderSelector *emfs)
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
	EMailSession *session;
	EMFolderTreeModel *model;
	GtkWidget *content_area;
	GtkWidget *container;
	GtkWidget *widget;

	model = em_folder_selector_get_model (emfs);
	session = em_folder_tree_model_get_session (model);

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
			GTK_DIALOG (emfs), _("_New"),
			EM_FOLDER_SELECTOR_RESPONSE_NEW);
		g_signal_connect (
			emfs, "response",
			G_CALLBACK (emfs_response), emfs);
	}

	gtk_dialog_add_buttons (
		GTK_DIALOG (emfs),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		oklabel ? oklabel : _("_OK"), GTK_RESPONSE_OK, NULL);

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
	gtk_widget_set_size_request (widget, -1, 240);
	gtk_widget_show (widget);

	container = widget;

	widget = em_folder_tree_new_with_model (
		session, E_ALERT_SINK (emfs), model);
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
                        EMFolderTreeModel *model,
                        guint32 flags,
                        const gchar *title,
                        const gchar *text,
                        const gchar *oklabel)
{
	EMFolderSelector *emfs;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	emfs = g_object_new (
		EM_TYPE_FOLDER_SELECTOR,
		"transient-for", parent,
		"model", model, NULL);
	folder_selector_construct (emfs, flags, title, text, oklabel);

	return (GtkWidget *) emfs;
}

static void
emfs_create_name_activate (GtkEntry *entry,
                           EMFolderSelector *emfs)
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
                               EMFolderTreeModel *model,
                               guint32 flags,
                               const gchar *title,
                               const gchar *text)
{
	EMFolderSelector *emfs;
	EMFolderTree *folder_tree;
	GtkWidget *hbox, *w;
	GtkWidget *container;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	/* remove the CREATE flag if it is there since that's the
	 * whole purpose of this dialog */
	flags &= ~EM_FOLDER_SELECTOR_CAN_CREATE;

	emfs = g_object_new (
		EM_TYPE_FOLDER_SELECTOR,
		"transient-for", parent,
		"model", model, NULL);
	folder_selector_construct (emfs, flags, title, text, _("C_reate"));

	folder_tree = em_folder_selector_get_folder_tree (emfs);
	em_folder_tree_set_excluded (folder_tree, EMFT_EXCLUDE_NOINFERIORS);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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

EMFolderTreeModel *
em_folder_selector_get_model (EMFolderSelector *emfs)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (emfs), NULL);

	return emfs->priv->model;
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

	g_return_val_if_fail (EM_IS_FOLDER_SELECTOR (emfs), NULL);

	folder_tree = em_folder_selector_get_folder_tree (emfs);
	uri = em_folder_tree_get_selected_uri (folder_tree);

	if (uri == NULL)
		return NULL;

	if (emfs->name_entry) {
		const gchar *name;
		gchar *temp_uri, *escaped_name;

		name = gtk_entry_get_text (emfs->name_entry);
		escaped_name = g_uri_escape_string (name, NULL, TRUE);
		temp_uri = g_strconcat (uri, "/", escaped_name, NULL);

		g_free (escaped_name);
		g_free (uri);
		uri = temp_uri;
	}

	g_free (emfs->selected_uri);
	emfs->selected_uri = uri;  /* takes ownership */

	return uri;
}
