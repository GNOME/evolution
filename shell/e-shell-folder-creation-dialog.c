/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <glade/glade-xml.h>

#include "e-util/e-util.h"
#include "widgets/misc/e-scroll-frame.h"

#include "e-storage-set.h"
#include "e-storage-set-view.h"

#include "e-shell-folder-creation-dialog.h"


#define GLADE_FILE_NAME E_GLADEDIR "/e-shell-folder-creation-dialog.glade"


/* Dialog callbacks.  */

static void
dialog_clicked_cb (GnomeDialog *dialog,
		   int button_number,
		   void *data)
{
	g_print ("Clicked -- %d\n", button_number);
	gnome_dialog_close (dialog);
}

static void
dialog_close_cb (GnomeDialog *dialog,
		 void *data)
{
	g_print ("Closed\n");
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
folder_name_entry_changed_cb (GtkEditable *editable,
			      void *data)
{
	GnomeDialog *dialog;
	GtkEntry *entry;

	entry  = GTK_ENTRY (editable);
	dialog = GNOME_DIALOG (data);

	if (entry->text_length > 0)
		gnome_dialog_set_sensitive (dialog, 0, TRUE);
	else
		gnome_dialog_set_sensitive (dialog, 0, FALSE);
}


/* Dialog setup.  */

static void
setup_dialog (GtkWidget *dialog,
	      GladeXML *gui,
	      EShell *shell,
	      GtkWindow *parent)
{
	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Evolution - Create new folder"));

	gnome_dialog_set_default   (GNOME_DIALOG (dialog), 0);
	gnome_dialog_set_sensitive (GNOME_DIALOG (dialog), 0, FALSE);

	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (dialog_clicked_cb), shell);
	gtk_signal_connect (GTK_OBJECT (dialog), "close",
			    GTK_SIGNAL_FUNC (dialog_close_cb), shell);

	gtk_widget_show (dialog);
}

static void
setup_folder_name_entry (GtkWidget *dialog,
			 GladeXML *gui,
			 EShell *shell)
{
	GtkWidget *folder_name_entry;

	folder_name_entry = glade_xml_get_widget (gui, "folder_name_entry");

	gnome_dialog_editable_enters (GNOME_DIALOG (dialog), GTK_EDITABLE (folder_name_entry));

	gtk_signal_connect (GTK_OBJECT (folder_name_entry), "changed",
			    GTK_SIGNAL_FUNC (folder_name_entry_changed_cb), dialog);
}

static void
add_storage_set_view (GtkWidget *dialog,
		      GladeXML *gui,
		      EShell *shell,
		      const char *default_parent_folder)
{
	EStorageSet *storage_set;
	GtkWidget *storage_set_view;
	GtkWidget *scroll_frame;
	GtkWidget *vbox;

	storage_set = e_shell_get_storage_set (shell);
	storage_set_view = e_storage_set_new_view (storage_set);

	GTK_WIDGET_SET_FLAGS (storage_set_view, GTK_CAN_FOCUS);

	if (default_parent_folder != NULL)
		e_storage_set_view_set_current_folder (E_STORAGE_SET_VIEW (storage_set_view),
						       default_parent_folder);

	vbox = glade_xml_get_widget (gui, "main_vbox");

	scroll_frame = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (scroll_frame), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (vbox), scroll_frame, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (scroll_frame), storage_set_view);

	gtk_widget_show (scroll_frame);
	gtk_widget_show (storage_set_view);
}

static void
add_folder_types (GtkWidget *dialog,
		  GladeXML *gui,
		  EShell *shell)
{
	EFolderTypeRegistry *folder_type_registry;
	GtkWidget *folder_type_option_menu;
	GtkWidget *menu;
	GList *types;
	GList *p;
	int default_item;
	int i;

	folder_type_option_menu = glade_xml_get_widget (gui, "folder_type_option_menu");

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (folder_type_option_menu));
	g_assert (menu != NULL);
	g_assert (GTK_IS_MENU (menu));

	folder_type_registry = e_shell_get_folder_type_registry (shell);
	g_assert (folder_type_registry != NULL);

	types = e_folder_type_registry_get_type_names (folder_type_registry);
	if (types == NULL)
		return;		/* Uh? */

	types = g_list_sort (types, (GCompareFunc) g_strcasecmp);

	/* FIXME: Use descriptive name (not in the registry's implementation yet).  */
	/* FIXME: Add icon (I don't feel like writing an alpha-capable thingie again).  */

	default_item = 0;
	for (p = types, i = 0; p != NULL; p = p->next, i++) {
		const char *type_name;
		GtkWidget *menu_item;

		type_name = (const char *) p->data;

		menu_item = gtk_menu_item_new_with_label (type_name);
		gtk_menu_append (GTK_MENU (menu), menu_item);
		gtk_widget_show (menu_item);

		if (strcmp (type_name, "mail") == 0)
			default_item = i;
	}

	e_free_string_list (types);

	gtk_option_menu_set_history (GTK_OPTION_MENU (folder_type_option_menu), default_item);
}


/* FIXME: Currently this is modal.  I think it's OK, but if people think it is
   not, we should change it to non-modal and make sure only one of these is
   open at once.  Currently it relies on modality for this.  */
void
e_shell_show_folder_creation_dialog (EShell *shell,
				     GtkWindow *parent,
				     const char *default_parent_folder)
{
	GladeXML *gui;
	GtkWidget *dialog;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	gui = glade_xml_new (GLADE_FILE_NAME, NULL);
	if (gui == NULL) {
		g_warning ("Cannot load Glade description file for the folder creation dialog -- %s",
			   GLADE_FILE_NAME);
		return;
	}

	dialog = glade_xml_get_widget (gui, "create_folder_dialog");

	setup_dialog (dialog, gui, shell, parent);
	setup_folder_name_entry (dialog, gui, shell);

	add_storage_set_view (dialog, gui, shell, default_parent_folder);
	add_folder_types (dialog, gui, shell);

	gtk_object_unref (GTK_OBJECT (gui));
}
