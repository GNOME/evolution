/*
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

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>
#include <e-util/e-util.h>

#include <libemail-engine/libemail-engine.h>

#include "em-folder-tree.h"
#include "em-folder-selector.h"
#include "em-utils.h"

#include "em-folder-selection-button.h"

struct _EMFolderSelectionButtonPrivate {
	EMailSession *session;
	GtkWidget *icon;
	GtkWidget *label;
	CamelStore *store;

	gchar *title;
	gchar *caption;
	gchar *folder_uri;

	gboolean can_none;
};

enum {
	PROP_0,
	PROP_CAN_NONE,
	PROP_CAPTION,
	PROP_FOLDER_URI,
	PROP_SESSION,
	PROP_STORE,
	PROP_TITLE
};

enum {
	SELECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMFolderSelectionButton, em_folder_selection_button, GTK_TYPE_BUTTON)

static void
folder_selection_button_unselected (EMFolderSelectionButton *button)
{
	const gchar *text;

	text = _("<click here to select a folder>");
	gtk_image_set_from_pixbuf (GTK_IMAGE (button->priv->icon), NULL);
	gtk_label_set_text (GTK_LABEL (button->priv->label), text);
}

static void
folder_selection_button_set_contents (EMFolderSelectionButton *button)
{
	EMailSession *session;
	CamelStore *store = NULL;
	CamelService *service;
	GtkLabel *label;
	const gchar *display_name;
	gchar *folder_name = NULL;

	label = GTK_LABEL (button->priv->label);
	session = em_folder_selection_button_get_session (button);

	if (session != NULL && button->priv->folder_uri != NULL)
		e_mail_folder_uri_parse (
			CAMEL_SESSION (session),
			button->priv->folder_uri,
			&store, &folder_name, NULL);

	if (store == NULL || folder_name == NULL) {
		folder_selection_button_unselected (button);
		return;
	}

	service = CAMEL_SERVICE (store);
	display_name = camel_service_get_display_name (service);

	if (display_name != NULL) {
		gchar *text;

		text = g_strdup_printf (
			"%s/%s", display_name, _(folder_name));
		gtk_label_set_text (label, text);
		g_free (text);
	} else
		gtk_label_set_text (label, _(folder_name));

	g_object_unref (store);
	g_free (folder_name);
}

static void
folder_selection_button_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_NONE:
			em_folder_selection_button_set_can_none (
				EM_FOLDER_SELECTION_BUTTON (object),
				g_value_get_boolean (value));
			return;

		case PROP_CAPTION:
			em_folder_selection_button_set_caption (
				EM_FOLDER_SELECTION_BUTTON (object),
				g_value_get_string (value));
			return;

		case PROP_FOLDER_URI:
			em_folder_selection_button_set_folder_uri (
				EM_FOLDER_SELECTION_BUTTON (object),
				g_value_get_string (value));
			return;

		case PROP_SESSION:
			em_folder_selection_button_set_session (
				EM_FOLDER_SELECTION_BUTTON (object),
				g_value_get_object (value));
			return;

		case PROP_STORE:
			em_folder_selection_button_set_store (
				EM_FOLDER_SELECTION_BUTTON (object),
				g_value_get_object (value));
			return;

		case PROP_TITLE:
			em_folder_selection_button_set_title (
				EM_FOLDER_SELECTION_BUTTON (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
folder_selection_button_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_NONE:
			g_value_set_boolean (
				value,
				em_folder_selection_button_get_can_none (
				EM_FOLDER_SELECTION_BUTTON (object)));
			return;

		case PROP_CAPTION:
			g_value_set_string (
				value,
				em_folder_selection_button_get_caption (
				EM_FOLDER_SELECTION_BUTTON (object)));
			return;

		case PROP_FOLDER_URI:
			g_value_set_string (
				value,
				em_folder_selection_button_get_folder_uri (
				EM_FOLDER_SELECTION_BUTTON (object)));
			return;

		case PROP_SESSION:
			g_value_set_object (
				value,
				em_folder_selection_button_get_session (
				EM_FOLDER_SELECTION_BUTTON (object)));
			return;

		case PROP_STORE:
			g_value_set_object (
				value,
				em_folder_selection_button_get_store (
				EM_FOLDER_SELECTION_BUTTON (object)));
			return;

		case PROP_TITLE:
			g_value_set_string (
				value,
				em_folder_selection_button_get_title (
				EM_FOLDER_SELECTION_BUTTON (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
folder_selection_button_dispose (GObject *object)
{
	EMFolderSelectionButton *self = EM_FOLDER_SELECTION_BUTTON (object);

	g_clear_object (&self->priv->session);
	g_clear_object (&self->priv->store);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_folder_selection_button_parent_class)->dispose (object);
}

static void
folder_selection_button_finalize (GObject *object)
{
	EMFolderSelectionButton *self = EM_FOLDER_SELECTION_BUTTON (object);

	g_free (self->priv->title);
	g_free (self->priv->caption);
	g_free (self->priv->folder_uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_folder_selection_button_parent_class)->finalize (object);
}

static void
folder_selection_button_clicked (GtkButton *button)
{
	EMFolderSelectionButton *self = EM_FOLDER_SELECTION_BUTTON (button);
	EMFolderSelector *selector;
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model = NULL;
	GtkWidget *dialog;
	GtkTreeSelection *selection;
	gpointer parent;
	gint response;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (button));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	if (self->priv->store != NULL) {
		model = em_folder_tree_model_new ();
		em_folder_tree_model_set_session (model, self->priv->session);
		em_folder_tree_model_add_store (model, self->priv->store);
	}

	if (model == NULL)
		model = g_object_ref (em_folder_tree_model_get_default ());

	dialog = em_folder_selector_new (parent, model);

	gtk_window_set_title (GTK_WINDOW (dialog), self->priv->title);

	g_object_unref (model);

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_can_create (selector, TRUE);
	em_folder_selector_set_can_none (selector, self->priv->can_none);
	em_folder_selector_set_caption (selector, self->priv->caption);

	folder_tree = em_folder_selector_get_folder_tree (selector);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	em_folder_tree_set_excluded (
		folder_tree,
		EMFT_EXCLUDE_NOSELECT |
		EMFT_EXCLUDE_VIRTUAL |
		EMFT_EXCLUDE_VTRASH);

	em_folder_tree_set_selected (folder_tree, self->priv->folder_uri, FALSE);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_OK) {
		const gchar *uri;

		uri = em_folder_selector_get_selected_uri (selector);
		em_folder_selection_button_set_folder_uri (
			EM_FOLDER_SELECTION_BUTTON (button), uri);

		g_signal_emit (button, signals[SELECTED], 0);
	} else if (response == GTK_RESPONSE_NO) {
		em_folder_selection_button_set_folder_uri (EM_FOLDER_SELECTION_BUTTON (button), NULL);
		g_signal_emit (button, signals[SELECTED], 0);
	}

	gtk_widget_destroy (dialog);
}

static void
em_folder_selection_button_class_init (EMFolderSelectionButtonClass *class)
{
	GObjectClass *object_class;
	GtkButtonClass *button_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = folder_selection_button_set_property;
	object_class->get_property = folder_selection_button_get_property;
	object_class->dispose = folder_selection_button_dispose;
	object_class->finalize = folder_selection_button_finalize;

	button_class = GTK_BUTTON_CLASS (class);
	button_class->clicked = folder_selection_button_clicked;

	g_object_class_install_property (
		object_class,
		PROP_CAN_NONE,
		g_param_spec_boolean (
			"can-none",
			"Can None",
			"Whether can show 'None' button, to be able to unselect folder",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_CAPTION,
		g_param_spec_string (
			"caption",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_FOLDER_URI,
		g_param_spec_string (
			"folder-uri",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_STORE,
		g_param_spec_object (
			"store",
			NULL,
			NULL,
			CAMEL_TYPE_STORE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TITLE,
		g_param_spec_string (
			"title",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[SELECTED] = g_signal_new (
		"selected",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMFolderSelectionButtonClass, selected),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
em_folder_selection_button_init (EMFolderSelectionButton *emfsb)
{
	GtkWidget *box;

	emfsb->priv = em_folder_selection_button_get_instance_private (emfsb);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_add (GTK_CONTAINER (emfsb), box);
	gtk_widget_show (box);

	emfsb->priv->icon = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (box), emfsb->priv->icon, FALSE, TRUE, 0);
	gtk_widget_show (emfsb->priv->icon);

	emfsb->priv->label = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (emfsb->priv->label), GTK_JUSTIFY_LEFT);
	gtk_label_set_xalign (GTK_LABEL (emfsb->priv->label), 0);
	gtk_box_pack_start (GTK_BOX (box), emfsb->priv->label, TRUE, TRUE, 0);
	gtk_widget_show (emfsb->priv->label);

	folder_selection_button_set_contents (emfsb);
}

GtkWidget *
em_folder_selection_button_new (EMailSession *session,
                                const gchar *title,
                                const gchar *caption)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_FOLDER_SELECTION_BUTTON,
		"session", session, "title", title,
		"caption", caption, NULL);
}

EMailSession *
em_folder_selection_button_get_session (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), NULL);

	return button->priv->session;
}

void
em_folder_selection_button_set_session (EMFolderSelectionButton *button,
                                        EMailSession *session)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));

	if (button->priv->session == session)
		return;

	if (session != NULL) {
		g_return_if_fail (E_IS_MAIL_SESSION (session));
		g_object_ref (session);
	}

	if (button->priv->session != NULL)
		g_object_unref (button->priv->session);

	button->priv->session = session;

	g_object_notify (G_OBJECT (button), "session");
}

gboolean
em_folder_selection_button_get_can_none (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), FALSE);

	return button->priv->can_none;
}

void
em_folder_selection_button_set_can_none (EMFolderSelectionButton *button,
					 gboolean can_none)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));

	if (button->priv->can_none == can_none)
		return;

	button->priv->can_none = can_none;

	g_object_notify (G_OBJECT (button), "can-none");
}

const gchar *
em_folder_selection_button_get_caption (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), NULL);

	return button->priv->caption;
}

void
em_folder_selection_button_set_caption (EMFolderSelectionButton *button,
                                        const gchar *caption)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));

	if (g_strcmp0 (button->priv->caption, caption) == 0)
		return;

	g_free (button->priv->caption);
	button->priv->caption = g_strdup (caption);

	g_object_notify (G_OBJECT (button), "caption");
}

const gchar *
em_folder_selection_button_get_folder_uri (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), NULL);

	return button->priv->folder_uri;
}

void
em_folder_selection_button_set_folder_uri (EMFolderSelectionButton *button,
                                           const gchar *folder_uri)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));

	/* An empty string is equivalent to NULL. */
	if (folder_uri != NULL && *folder_uri == '\0')
		folder_uri = NULL;

	if (g_strcmp0 (button->priv->folder_uri, folder_uri) == 0)
		return;

	g_free (button->priv->folder_uri);
	button->priv->folder_uri = g_strdup (folder_uri);

	folder_selection_button_set_contents (button);

	g_object_notify (G_OBJECT (button), "folder-uri");
}

CamelStore *
em_folder_selection_button_get_store (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), NULL);

	return button->priv->store;
}

void
em_folder_selection_button_set_store (EMFolderSelectionButton *button,
                                      CamelStore *store)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));

	if (button->priv->store == store)
		return;

	if (store != NULL) {
		g_return_if_fail (CAMEL_IS_STORE (store));
		g_object_ref (store);
	}

	if (button->priv->store != NULL)
		g_object_unref (button->priv->store);

	button->priv->store = store;

	g_object_notify (G_OBJECT (button), "store");
}

const gchar *
em_folder_selection_button_get_title (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_FOLDER_SELECTION_BUTTON (button), NULL);

	return button->priv->title;
}

void
em_folder_selection_button_set_title (EMFolderSelectionButton *button,
                                      const gchar *title)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));

	if (g_strcmp0 (button->priv->title, title) == 0)
		return;

	g_free (button->priv->title);
	button->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (button), "title");
}
