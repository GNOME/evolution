/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* em-folder-selection-button.c
 *
 * Copyright (C) 2003  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>

#include <string.h>

#include "em-folder-selection-button.h"

#include "mail-component.h"
#include "em-folder-selector.h"

#include <gal/util/e-util.h>

#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>


#define PARENT_TYPE gtk_button_get_type ()
static GtkButtonClass *parent_class = NULL;


struct _EMFolderSelectionButtonPrivate {
	GtkWidget *icon;
	GtkWidget *label;

	char *uri;

	char *title;
	char *caption;
};

enum {
	SELECTED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };


/* Utility functions.  */

static void
set_contents_unselected (EMFolderSelectionButton *button)
{	
	gtk_image_set_from_pixbuf (GTK_IMAGE (button->priv->icon), NULL);
	gtk_label_set_text (GTK_LABEL (button->priv->label), _("<click here to select a folder>"));
}

static void
set_contents (EMFolderSelectionButton *button)
{
	EMFolderSelectionButtonPrivate *priv = button->priv;
	char *path, *tmp, *label;

	if (priv->uri == NULL)
		goto unset;

	/* We set the button name directly from the storage set path, which is /accountname/path/foldername */
	path = e_storage_set_get_path_for_physical_uri(mail_component_peek_storage_set(mail_component_peek()), priv->uri);

	if (path == NULL)
		goto unknown;

	tmp = strchr(path+1, '/');
	if (tmp == NULL)
		goto unknown;
	*tmp++ = 0;

	label = g_strdup_printf(_("\"%s\" in \"%s\""), tmp, path+1);
	gtk_label_set_text (GTK_LABEL (priv->label), label);
	g_free (label);

	g_free(path);
	return;

unknown:
	g_free(path);
unset:
	set_contents_unselected(button);
}

static void
impl_finalize (GObject *object)
{
	EMFolderSelectionButtonPrivate *priv = EM_FOLDER_SELECTION_BUTTON (object)->priv;

	g_free (priv->title);
	g_free (priv->caption);
	g_free(priv->uri);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
emfsb_selector_response(EMFolderSelector *emfs, int response, EMFolderSelectionButton *button)
{
	if (response == GTK_RESPONSE_OK) {
		const char *uri = em_folder_selector_get_selected_uri(emfs);

		em_folder_selection_button_set_selection(button, uri);
		g_signal_emit(button, signals[SELECTED], 0);
	}

	gtk_widget_destroy((GtkWidget *)emfs);
}

static void
impl_clicked (GtkButton *button)
{
	EMFolderSelectionButtonPrivate *priv = EM_FOLDER_SELECTION_BUTTON (button)->priv;
	EStorageSet *ess;
	GtkWidget *w;
	GtkWidget *toplevel;

	if (GTK_BUTTON_CLASS (parent_class)->clicked != NULL)
		(* GTK_BUTTON_CLASS (parent_class)->clicked) (button);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	ess = mail_component_peek_storage_set(mail_component_peek());
	w = em_folder_selector_new(ess, EM_FOLDER_SELECTOR_CAN_CREATE, priv->title, priv->caption);
	em_folder_selector_set_selected_uri((EMFolderSelector *)w, priv->uri);
	g_signal_connect(w, "response", G_CALLBACK(emfsb_selector_response), button);
	gtk_widget_show(w);
}
#if 0
{
	uri = em_folder_selection_run_dialog_uri((GtkWindow *)toplevel,
						 priv->title,
						 priv->caption,
						 priv->uri);

	em_folder_selection_button_set_selection (EM_FOLDER_SELECTION_BUTTON (button), uri);
	g_free(uri);

	g_signal_emit (button, signals[SELECTED], 0);
}
#endif

static void
class_init (EMFolderSelectionButtonClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkButtonClass *button_class = GTK_BUTTON_CLASS (class);

	object_class->finalize = impl_finalize;

	button_class->clicked = impl_clicked;

	parent_class = g_type_class_peek_parent (class);

	signals[SELECTED] = g_signal_new ("selected",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (EMFolderSelectionButtonClass, selected),
					  NULL, NULL,
					  g_cclosure_marshal_VOID__VOID,
					  G_TYPE_NONE, 0);
}

static void
init (EMFolderSelectionButton *folder_selection_button)
{
	EMFolderSelectionButtonPrivate *priv;
	GtkWidget *box;

	priv = g_new0 (EMFolderSelectionButtonPrivate, 1);
	folder_selection_button->priv = priv;

	box = gtk_hbox_new (FALSE, 4);

	priv->icon = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (box), priv->icon, FALSE, TRUE, 0);

	priv->label = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (priv->label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (box), priv->label, TRUE, TRUE, 0);

	gtk_widget_show_all (box);
	gtk_container_add (GTK_CONTAINER (folder_selection_button), box);

	set_contents (folder_selection_button);
}

GtkWidget *
em_folder_selection_button_new(const char *title, const char *caption)
{
	EMFolderSelectionButton *button = g_object_new (EM_TYPE_FOLDER_SELECTION_BUTTON, NULL);

	button->priv->title = g_strdup (title);
	button->priv->caption = g_strdup (caption);

	return GTK_WIDGET (button);
}


void
em_folder_selection_button_set_selection(EMFolderSelectionButton *button, const char *uri)
{
	EMFolderSelectionButtonPrivate *p = button->priv;

	g_return_if_fail(EM_IS_FOLDER_SELECTION_BUTTON(button));

	if (p->uri != uri) {
		g_free(p->uri);
		p->uri = g_strdup(uri);
	}

	set_contents(button);
}


const char *
em_folder_selection_button_get_selection(EMFolderSelectionButton *button)
{
	g_return_val_if_fail (EM_IS_FOLDER_SELECTION_BUTTON (button), NULL);

	return button->priv->uri;
}

E_MAKE_TYPE (em_folder_selection_button, "EMFolderSelectionButton", EMFolderSelectionButton, class_init, init, PARENT_TYPE)
