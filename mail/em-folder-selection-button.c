/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>

#include <gal/util/e-util.h>
#include <libgnome/gnome-i18n.h>

#include "mail-component.h"
#include "em-folder-tree.h"
#include "em-folder-selector.h"
#include "em-utils.h"

#include "em-folder-selection-button.h"

static void em_folder_selection_button_class_init (EMFolderSelectionButtonClass *klass);
static void em_folder_selection_button_init (EMFolderSelectionButton *emfsb);
static void em_folder_selection_button_destroy (GtkObject *obj);
static void em_folder_selection_button_finalize (GObject *obj);
static void em_folder_selection_button_clicked (GtkButton *button);

static GtkButtonClass *parent_class = NULL;

struct _EMFolderSelectionButtonPrivate {
	GtkWidget *icon;
	GtkWidget *label;
	
	char *uri;   /* for single-select mode */
	GList *uris; /* for multi-select mode */
	
	char *title;
	char *caption;
	
	gboolean multiple_select;
};

enum {
	SELECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GType
em_folder_selection_button_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFolderSelectionButtonClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_folder_selection_button_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFolderSelectionButton),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_folder_selection_button_init,
		};
		
		type = g_type_register_static (GTK_TYPE_BUTTON, "EMFolderSelectionButton", &info, 0);
	}
	
	return type;
}

static void
em_folder_selection_button_class_init (EMFolderSelectionButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);
	GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);
	
	parent_class = g_type_class_ref (GTK_TYPE_BUTTON);
	
	object_class->finalize = em_folder_selection_button_finalize;
	gtk_object_class->destroy = em_folder_selection_button_destroy;
	button_class->clicked = em_folder_selection_button_clicked;
	
	signals[SELECTED] = g_signal_new ("selected",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (EMFolderSelectionButtonClass, selected),
					  NULL, NULL,
					  g_cclosure_marshal_VOID__VOID,
					  G_TYPE_NONE, 0);
}

static void
set_contents_unselected (EMFolderSelectionButton *button)
{	
	gtk_image_set_from_pixbuf (GTK_IMAGE (button->priv->icon), NULL);
	gtk_label_set_text (GTK_LABEL (button->priv->label), _("<click here to select a folder>"));
}

static void
set_contents (EMFolderSelectionButton *button)
{
	struct _EMFolderSelectionButtonPrivate *priv = button->priv;
	char *folder_name = em_utils_folder_name_from_uri (priv->uri);
	
	if (folder_name) {
		gtk_label_set_text (GTK_LABEL (priv->label), folder_name);
		g_free (folder_name);
	} else {
		set_contents_unselected (button);
	}
}

static void
em_folder_selection_button_init (EMFolderSelectionButton *emfsb)
{
	struct _EMFolderSelectionButtonPrivate *priv;
	GtkWidget *box;
	
	priv = g_new0 (struct _EMFolderSelectionButtonPrivate, 1);
	emfsb->priv = priv;
	
	priv->multiple_select = FALSE;
	
	box = gtk_hbox_new (FALSE, 4);
	
	priv->icon = gtk_image_new ();
	gtk_widget_show (priv->icon);
	gtk_box_pack_start (GTK_BOX (box), priv->icon, FALSE, TRUE, 0);
	
	priv->label = gtk_label_new ("");
	gtk_widget_show (priv->label);
	gtk_label_set_justify (GTK_LABEL (priv->label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (box), priv->label, TRUE, TRUE, 0);
	
	gtk_widget_show (box);
	gtk_container_add (GTK_CONTAINER (emfsb), box);
	
	set_contents (emfsb);
}

static void
em_folder_selection_button_destroy (GtkObject *obj)
{
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static void
em_folder_selection_button_finalize (GObject *obj)
{
	struct _EMFolderSelectionButtonPrivate *priv = ((EMFolderSelectionButton *) obj)->priv;

	GList *lst = ((EMFolderSelectionButton*) obj)->priv->uris;
	g_list_foreach (lst, (GFunc) g_free, NULL);
	g_list_free (lst);
	
	g_free (priv->title);
	g_free (priv->caption);
	g_free (priv->uri);
	g_free (priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
emfsb_selector_response (EMFolderSelector *emfs, int response, EMFolderSelectionButton *button)
{
	if (response == GTK_RESPONSE_OK) {
		if (button->priv->multiple_select) {
			GList *uris = em_folder_selector_get_selected_uris (emfs);
			
			em_folder_selection_button_set_selection_mult (button, uris);
			g_signal_emit (button, signals[SELECTED], 0);
		} else {
			const char *uri = em_folder_selector_get_selected_uri (emfs);
			
			em_folder_selection_button_set_selection (button, uri);
			g_signal_emit (button, signals[SELECTED], 0);
		}
	}
	
	gtk_widget_destroy ((GtkWidget *) emfs);
}

static void
em_folder_selection_button_clicked (GtkButton *button)
{
	struct _EMFolderSelectionButtonPrivate *priv = EM_FOLDER_SELECTION_BUTTON (button)->priv;
	EMFolderTreeModel *model;
	EMFolderTree *emft;
	GtkWidget *dialog;
	
	if (GTK_BUTTON_CLASS (parent_class)->clicked != NULL)
		(* GTK_BUTTON_CLASS (parent_class)->clicked) (button);
	
	model = mail_component_peek_tree_model (mail_component_peek ());
	emft = (EMFolderTree *) em_folder_tree_new_with_model (model);
	em_folder_tree_set_multiselect (emft, priv->multiple_select);
	em_folder_tree_set_excluded(emft, EMFT_EXCLUDE_NOSELECT|EMFT_EXCLUDE_VIRTUAL|EMFT_EXCLUDE_VTRASH);
	dialog = em_folder_selector_new (emft, EM_FOLDER_SELECTOR_CAN_CREATE, priv->title, priv->caption, NULL);
	if (priv->multiple_select)
		em_folder_selector_set_selected_list ((EMFolderSelector *) dialog, priv->uris);
	else
		em_folder_selector_set_selected ((EMFolderSelector *) dialog, priv->uri);
	g_signal_connect (dialog, "response", G_CALLBACK (emfsb_selector_response), button);
	gtk_widget_show (dialog);
}

GtkWidget *
em_folder_selection_button_new (const char *title, const char *caption)
{
	EMFolderSelectionButton *button = g_object_new (EM_TYPE_FOLDER_SELECTION_BUTTON, NULL);
	
	button->priv->title = g_strdup (title);
	button->priv->caption = g_strdup (caption);
	
	return GTK_WIDGET (button);
}

void
em_folder_selection_button_set_selection (EMFolderSelectionButton *button, const char *uri)
{
	struct _EMFolderSelectionButtonPrivate *priv = button->priv;
	
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));
	
	if (priv->uri != uri) {
		g_free (priv->uri);
		priv->uri = g_strdup (uri);
	}
	
	set_contents (button);
}

const char *
em_folder_selection_button_get_selection (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), NULL);
	
	return button->priv->uri;
}

void
em_folder_selection_button_set_selection_mult (EMFolderSelectionButton *button, GList *uris)
{
	struct _EMFolderSelectionButtonPrivate *priv = button->priv;
	char *caption, *tmp, *tmp2;
	
	g_return_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button));
	
	if (priv->uris) {
		g_list_foreach (priv->uris, (GFunc) g_free, NULL);
		g_list_free (priv->uris);
		priv->uris = NULL;
	}
	
	priv->uris = uris;
	
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
			priv->uris = g_list_remove (priv->uris, uris->data);
		}
	}
	
	if (caption[0])
		gtk_label_set_text (GTK_LABEL (priv->label), caption + 2);
	else
		set_contents_unselected (button);
	
	g_free (caption);
}

GList *
em_folder_selection_button_get_selection_mult (EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), NULL);
	
	return button->priv->uris;
}

void
em_folder_selection_button_set_multiselect (EMFolderSelectionButton *button, gboolean value)
{
	button->priv->multiple_select = value;
}

gboolean
em_folder_selection_button_get_multiselect (EMFolderSelectionButton *button)
{
	return button->priv->multiple_select;
}
