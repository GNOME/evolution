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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <e-util/e-util.h>

#include "mail-config.h"
#include "em-folder-tree.h"
#include "em-folder-selector.h"
#include "em-utils.h"

#include "em-folder-selection-button.h"

#define EM_FOLDER_SELECTION_BUTTON_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FOLDER_SELECTION_BUTTON, EMFolderSelectionButtonPrivate))

struct _EMFolderSelectionButtonPrivate {
	GtkWidget *icon;
	GtkWidget *label;

	gchar *uri;  /* for single-select mode */
	GList *uris; /* for multi-select mode */

	gchar *title;
	gchar *caption;

	gboolean multiple_select;
};

enum {
	PROP_0,
	PROP_CAPTION,
	PROP_MULTISELECT,
	PROP_TITLE
};

enum {
	SELECTED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

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
	EAccount *account;
	GtkLabel *label;
	const gchar *uri;
	gchar *folder_name;

	uri = button->priv->uri;
	label = GTK_LABEL (button->priv->label);
	folder_name = em_utils_folder_name_from_uri (uri);

	if (folder_name == NULL) {
		folder_selection_button_unselected (button);
		return;
	}

	account = mail_config_get_account_by_source_url (uri);

	if (account != NULL) {
		gchar *tmp = folder_name;

		folder_name = g_strdup_printf (
			"%s/%s", e_account_get_string (
			account, E_ACCOUNT_NAME), _(folder_name));
		gtk_label_set_text (label, folder_name);
		g_free (tmp);
	} else
		gtk_label_set_text (label, _(folder_name));

	g_free (folder_name);
}

static void
folder_selection_button_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAPTION:
			em_folder_selection_button_set_caption (
				EM_FOLDER_SELECTION_BUTTON (object),
				g_value_get_string (value));
			return;

		case PROP_MULTISELECT:
			em_folder_selection_button_set_multiselect (
				EM_FOLDER_SELECTION_BUTTON (object),
				g_value_get_boolean (value));
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
		case PROP_CAPTION:
			g_value_set_string (
				value,
				em_folder_selection_button_get_caption (
				EM_FOLDER_SELECTION_BUTTON (object)));
			return;

		case PROP_MULTISELECT:
			g_value_set_boolean (
				value,
				em_folder_selection_button_get_multiselect (
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
folder_selection_button_finalize (GObject *object)
{
	EMFolderSelectionButtonPrivate *priv;

	priv = EM_FOLDER_SELECTION_BUTTON_GET_PRIVATE (object);

	g_list_foreach (priv->uris, (GFunc) g_free, NULL);
	g_list_free (priv->uris);

	g_free (priv->title);
	g_free (priv->caption);
	g_free (priv->uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
folder_selection_button_clicked (GtkButton *button)
{
	EMFolderSelectionButtonPrivate *priv;
	EMFolderTree *emft;
	GtkWidget *dialog;
	GtkTreeSelection *selection;
	GtkSelectionMode mode;
	gpointer parent;

	priv = EM_FOLDER_SELECTION_BUTTON_GET_PRIVATE (button);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (button));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	emft = (EMFolderTree *) em_folder_tree_new ();
	emu_restore_folder_tree_state (emft);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (emft));
	if (priv->multiple_select)
		mode = GTK_SELECTION_MULTIPLE;
	else
		mode = GTK_SELECTION_SINGLE;
	gtk_tree_selection_set_mode (selection, mode);

	em_folder_tree_set_excluded (
		emft, EMFT_EXCLUDE_NOSELECT |
		EMFT_EXCLUDE_VIRTUAL | EMFT_EXCLUDE_VTRASH);

	dialog = em_folder_selector_new (
		parent, emft, EM_FOLDER_SELECTOR_CAN_CREATE,
		priv->title, priv->caption, NULL);

	if (priv->multiple_select)
		em_folder_selector_set_selected_list (
			EM_FOLDER_SELECTOR (dialog), priv->uris);
	else
		em_folder_selector_set_selected (
			EM_FOLDER_SELECTOR (dialog), priv->uri);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	if (priv->multiple_select) {
		GList *uris;

		uris = em_folder_selector_get_selected_uris (
			EM_FOLDER_SELECTOR (dialog));
		em_folder_selection_button_set_selection_mult (
			EM_FOLDER_SELECTION_BUTTON (button), uris);
	} else {
		const gchar *uri;

		uri = em_folder_selector_get_selected_uri (
			EM_FOLDER_SELECTOR (dialog));
		em_folder_selection_button_set_selection (
			EM_FOLDER_SELECTION_BUTTON (button), uri);
	}

	g_signal_emit (button, signals[SELECTED], 0);

exit:
	gtk_widget_destroy (dialog);
}

static void
folder_selection_button_class_init (EMFolderSelectionButtonClass *class)
{
	GObjectClass *object_class;
	GtkButtonClass *button_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMFolderSelectionButtonPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = folder_selection_button_set_property;
	object_class->get_property = folder_selection_button_get_property;
	object_class->finalize = folder_selection_button_finalize;

	button_class = GTK_BUTTON_CLASS (class);
	button_class->clicked = folder_selection_button_clicked;

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
		PROP_MULTISELECT,
		g_param_spec_boolean (
			"multiselect",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

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
folder_selection_button_init (EMFolderSelectionButton *emfsb)
{
	GtkWidget *box;

	emfsb->priv = EM_FOLDER_SELECTION_BUTTON_GET_PRIVATE (emfsb);

	emfsb->priv->multiple_select = FALSE;

	box = gtk_hbox_new (FALSE, 4);

	emfsb->priv->icon = gtk_image_new ();
	gtk_widget_show (emfsb->priv->icon);
	gtk_box_pack_start (GTK_BOX (box), emfsb->priv->icon, FALSE, TRUE, 0);

	emfsb->priv->label = gtk_label_new ("");
	gtk_widget_show (emfsb->priv->label);
	gtk_label_set_justify (GTK_LABEL (emfsb->priv->label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (emfsb->priv->label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (box), emfsb->priv->label, TRUE, TRUE, 0);

	gtk_widget_show (box);
	gtk_container_add (GTK_CONTAINER (emfsb), box);

	folder_selection_button_set_contents (emfsb);
}

GType
em_folder_selection_button_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMFolderSelectionButtonClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) folder_selection_button_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMFolderSelectionButton),
			0,     /* n_preallocs */
			(GInstanceInitFunc) folder_selection_button_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_BUTTON, "EMFolderSelectionButton",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
em_folder_selection_button_new (const gchar *title,
                                const gchar *caption)
{
	return g_object_new (
		EM_TYPE_FOLDER_SELECTION_BUTTON,
		"title", title, "caption", caption, NULL);
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

	g_free (button->priv->caption);
	button->priv->caption = g_strdup (caption);

	g_object_notify (G_OBJECT (button), "caption");
}

gboolean
em_folder_selection_button_get_multiselect (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), FALSE);

	return button->priv->multiple_select;
}

void
em_folder_selection_button_set_multiselect (EMFolderSelectionButton *button,
                                            gboolean multiselect)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));

	button->priv->multiple_select = multiselect;

	g_object_notify (G_OBJECT (button), "multiselect");
}

const gchar *
em_folder_selection_button_get_selection (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), NULL);

	return button->priv->uri;
}

void
em_folder_selection_button_set_selection (EMFolderSelectionButton *button,
                                          const gchar *uri)
{
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));

	if (g_strcmp0 (button->priv->uri, uri) == 0)
		return;

	g_free (button->priv->uri);
	button->priv->uri = g_strdup (uri);

	folder_selection_button_set_contents (button);
}

GList *
em_folder_selection_button_get_selection_mult (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), NULL);

	return button->priv->uris;
}

void
em_folder_selection_button_set_selection_mult (EMFolderSelectionButton *button,
                                               GList *uris)
{
	gchar *caption, *tmp, *tmp2;

	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));

	g_list_foreach (button->priv->uris, (GFunc) g_free, NULL);
	g_list_free (button->priv->uris);

	button->priv->uris = uris;

	/* compile the name */
	caption = g_strdup ("");

	while (uris) {
		tmp = em_utils_folder_name_from_uri (uris->data);
		if (tmp) {
			tmp2 = g_strconcat (caption, ", ", tmp, NULL);
			g_free (caption);
			caption = tmp2;
			g_free (tmp);
			uris = uris->next;
		} else {
			/* apparently, we do not know this folder, so we'll just skip it */
			g_free (uris->data);
			uris = g_list_next (uris);
			button->priv->uris = g_list_remove (
				button->priv->uris, uris->data);
		}
	}

	if (caption[0])
		gtk_label_set_text (
			GTK_LABEL (button->priv->label), caption + 2);
	else
		folder_selection_button_unselected (button);

	g_free (caption);
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
	g_return_if_fail (EM_FOLDER_SELECTION_BUTTON (button));

	g_free (button->priv->title);
	button->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (button), "title");
}
