/* Evolution calendar - Framework for a calendar component editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "comp-editor.h"



/* Private part of the CompEditor structure */
struct _CompEditorPrivate {
	/* The pages we have */
	GList *pages;

	/* Toplevel window for the dialog */
	GtkWidget *window;

	/* Notebook to hold the pages */
	GtkNotebook *notebook;
};



static void comp_editor_class_init (CompEditorClass *class);
static void comp_editor_init (CompEditor *editor);
static void comp_editor_destroy (GtkObject *object);

static GtkObjectClass *parent_class;



GtkType
comp_editor_get_type (void)
{
	static GtkType comp_editor_type;

	if (!comp_editor_type) {
		static const GtkTypeInfo comp_editor_info = {
			"CompEditor",
			sizeof (CompEditor),
			sizeof (CompEditorClass),
			(GtkClassInitFunc) comp_editor_class_init,
			(GtkObjectInitfunc) comp_editor_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		}

		comp_editor_type = gtk_type_unique (GTK_TYPE_OBJECT, &comp_editor_info); 
	}

	return comp_editor_type;
}

/* Class initialization function for the calendar component editor */
static void
comp_editor_class_init (CompEditorClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = comp_editor_destroy;
}

/* Creates the basic in the editor */
static void
setup_widgets (CompEditor *editor)
{
	CompEditorPrivate *priv;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *pixmap;
	GtkWidget *button;

	priv = editor->priv;

	/* Window and basic vbox */

	priv->window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_signal_connect (GTK_OBJECT (priv->window), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event_cb), editor);

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (priv->window), vbox);

	/* Notebook */

	priv->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_box_pack_start (GTK_BOX (vbox), priv->notebook, TRUE, TRUE, 0);

	/* Buttons */

	bbox = gtk_hbutton_box_new ();
	gtk_hbutton_box_set_layout_default (GTK_HBUTTON_BOX (bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);

	pixmap = gnome_stock_pixmap_widget (NULL, GNOME_STOCK_PIXMAP_SAVE);
	button = gnome_pixmap_button (pixmap, _("Save"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

	button = gnome_stock_button (GNOME_STOCK_BUTTON_CLOSE);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

	button = gnome_stock_button (GNOME_STOCK_BUTTON_HELP);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
}

/* Object initialization function for the calendar component editor */
static void
comp_editor_init (CompEditor *editor)
{
	CompEditorPrivate *priv;

	priv = g_new0 (CompEditorPrivate, 1);
	editor->priv = priv;

	setup_widgets (editor);

	priv->pages = NULL;
}

/* Destroy handler for the calendar component editor */
static void
comp_editor_destroy (GtkObject *object)
{
	CompEditor *editor;
	CompEditorPrivate *priv;

	editor = COMP_EDITOR (object);
	priv = editor->priv;

	if (priv->window) {
		gtk_widget_destroy (priv->window);
		priv->window = NULL;
	}

	g_free (priv);
	editor->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



void
comp_editor_append_page (CompEditor *editor, EditorPage *page, const char *label)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	GtkWidget *label_widget;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_EDITOR_PAGE (page));
	g_return_if_fail (label != NULL);

	priv = editor->priv;

	/* Only allow adding the pages while a component has not been set */
	g_return_if_fail (priv->comp == NULL);

	page_widget = editor_page_get_widget (page);
	g_assert (page_widget != NULL);

	label_widget = gtk_label_new (label);

	priv->pages = g_list_append (priv->pages, page);
	gtk_notebook_append_page (priv->notebook, page_widget, label_widget);
}
