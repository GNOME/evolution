/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-shared-folder-picker-dialog.c - Implementation for the shared folder
 * picker dialog.
 *
 * Copyright (C) 2002 Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-shared-folder-picker-dialog.h"

#include "e-corba-storage.h"
#include "e-shell-constants.h"
#include "evolution-storage-listener.h"

#include "Evolution-Addressbook-SelectNames.h"

#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-unicode.h>

#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-widget.h>

#include <gtk/gtk.h>


/* Timeout for showing the progress dialog (msecs).  */

#define PROGRESS_DIALOG_DELAY 500


/* Dialog creation and handling.  */

static void
setup_folder_name_combo (GladeXML *glade_xml)
{
	GtkWidget *combo;
	GList *string_list;
	char *strings[] = {
		"Calendar",
		"Inbox",
		"Contacts",
		NULL
		/* FIXME: Should these be translated?  */
	};
	int i;

	combo = glade_xml_get_widget (glade_xml, "folder-name-combo");
	g_assert (GTK_IS_COMBO (combo));

	string_list = NULL;
	for (i = 0; strings[i] != NULL; i ++)
		string_list = g_list_append (string_list, strings[i]);
	gtk_combo_set_popdown_strings (GTK_COMBO (combo), string_list);
	g_list_free (string_list);

	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo)->entry), "Calendar");
}

static void
user_clicked (GtkWidget *button, GNOME_Evolution_Addressbook_SelectNames corba_iface)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	GNOME_Evolution_Addressbook_SelectNames_activateDialog (corba_iface, "User", &ev);

	if (BONOBO_EX (&ev)) {
		g_warning ("Cannot activate SelectNames dialog -- %s", BONOBO_EX_ID (&ev));
	}
	CORBA_exception_free (&ev);
}

static GtkWidget *
setup_name_selector (GladeXML *glade_xml,
		     GNOME_Evolution_Addressbook_SelectNames *iface_ret)
{
	GNOME_Evolution_Addressbook_SelectNames corba_iface;
	Bonobo_Control control;
	CORBA_Environment ev;
	GtkWidget *placeholder;
	GtkWidget *control_widget;
	GtkWidget *button;

	placeholder = glade_xml_get_widget (glade_xml, "user-picker-placeholder");
	g_assert (GTK_IS_CONTAINER (placeholder));

	CORBA_exception_init (&ev);

	corba_iface = oaf_activate_from_id ("OAFIID:GNOME_Evolution_Addressbook_SelectNames",
					    0, NULL, &ev);
	if (corba_iface == CORBA_OBJECT_NIL || BONOBO_EX (&ev)) {
		g_warning ("Cannot activate SelectNames -- %s", BONOBO_EX_ID (&ev));
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	GNOME_Evolution_Addressbook_SelectNames_addSectionWithLimit (corba_iface, "User", "User", 1, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Cannot add SelectNames section -- %s", BONOBO_EX_ID (&ev));
		goto err;
	}

	control = GNOME_Evolution_Addressbook_SelectNames_getEntryBySection (corba_iface, "User", &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Cannot get SelectNames section -- %s", BONOBO_EX_ID (&ev));
		goto err;
	}

	control_widget = bonobo_widget_new_control_from_objref (control, CORBA_OBJECT_NIL);
	gtk_container_add (GTK_CONTAINER (placeholder), control_widget);
	gtk_widget_show (control_widget);

	button = glade_xml_get_widget (glade_xml, "button-user");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    user_clicked, corba_iface);

	CORBA_exception_free (&ev);
	*iface_ret = corba_iface;
	return control_widget;

 err:
	bonobo_object_release_unref (corba_iface, NULL);
	CORBA_exception_free (&ev);
	return NULL;
}

static void
server_option_menu_item_activate_callback (GtkMenuItem *menu_item,
					   void *data)
{
	char **storage_name_return;

	storage_name_return = (char **) data;
	if (*storage_name_return != NULL)
		g_free (*storage_name_return);

	*storage_name_return = g_strdup ((const char *) gtk_object_get_data (GTK_OBJECT (menu_item),
									     "storage_name"));
}

static void
setup_server_option_menu (EShell *shell,
			  GladeXML *glade_xml,
			  char **storage_name_return)
{
	GList *storages;
	GList *p;
	GtkWidget *widget;
	GtkWidget *menu;

	widget = glade_xml_get_widget (glade_xml, "server-option-menu");
	g_assert (GTK_IS_OPTION_MENU (widget));

	menu = gtk_menu_new ();
	gtk_widget_show (menu);

	*storage_name_return = NULL;
	storages = e_storage_set_get_storage_list (e_shell_get_storage_set (shell));
	for (p = storages; p != NULL; p = p->next) {
		GtkWidget *menu_item;
		const char *storage_name;

		if (!e_storage_supports_shared_folders (p->data))
			continue;

		storage_name = e_storage_get_name (E_STORAGE (p->data));

		menu_item = e_utf8_gtk_menu_item_new_with_label (GTK_MENU (menu), storage_name);
		gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				    GTK_SIGNAL_FUNC (server_option_menu_item_activate_callback),
				    storage_name_return);
		gtk_object_set_data_full (GTK_OBJECT (menu_item), "storage_name",
					  g_strdup (storage_name), g_free);

		gtk_widget_show (menu_item);
		gtk_menu_append (GTK_MENU (menu), menu_item);

		if (*storage_name_return == NULL)
			*storage_name_return = g_strdup (storage_name);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);

	/* FIXME: Default to the current storage in the shell view.  */
}

static gboolean
show_dialog (EShell *shell,
	     EShellView *parent,
	     char **user_email_address_return,
	     char **storage_name_return,
	     char **folder_name_return)
{
	GNOME_Evolution_Addressbook_SelectNames corba_iface;
	GladeXML *glade_xml;
	GtkWidget *dialog;
	GtkWidget *name_selector_widget;
	GtkWidget *folder_name_entry;
	int button_num;

	glade_xml = glade_xml_new (EVOLUTION_GLADEDIR "/e-shell-shared-folder-picker-dialog.glade",
				   NULL);
	g_assert (glade_xml != NULL);

	name_selector_widget = setup_name_selector (glade_xml, &corba_iface);
	if (name_selector_widget == NULL)
		return FALSE;

	setup_server_option_menu (shell, glade_xml, storage_name_return);
	setup_folder_name_combo (glade_xml);

	dialog = glade_xml_get_widget (glade_xml, "dialog");
	g_assert (dialog != NULL);

	gnome_dialog_close_hides (GNOME_DIALOG (dialog), TRUE);

	button_num = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	if (button_num == 1) {	/* Cancel */
		g_free (*storage_name_return);
		*storage_name_return = NULL;
		gtk_widget_destroy (dialog);
		bonobo_object_release_unref (corba_iface, NULL);
		return FALSE;
	}

	bonobo_widget_get_property (BONOBO_WIDGET (name_selector_widget),
				    "addresses", user_email_address_return,
				    NULL);

	folder_name_entry = glade_xml_get_widget (glade_xml, "folder-name-entry");
	*folder_name_return = g_strdup (gtk_entry_get_text (GTK_ENTRY (folder_name_entry)));

	gtk_widget_destroy (dialog);
	bonobo_object_release_unref (corba_iface, NULL);
	return TRUE;
}


/* Discovery process.  */

struct _DiscoveryData {
	EShell *shell;
	EShellView *parent;
	GtkWidget *dialog;
	char *user_email_address;
	char *folder_name;
	EStorage *storage;
};
typedef struct _DiscoveryData DiscoveryData;

static void
cleanup_discovery (DiscoveryData *discovery_data)
{
	if (discovery_data->dialog != NULL)
		gtk_widget_destroy (discovery_data->dialog);

	g_free (discovery_data->user_email_address);
	g_free (discovery_data->folder_name);
	gtk_object_unref (GTK_OBJECT (discovery_data->storage));
	g_free (discovery_data);
}

static int
progress_bar_timeout_callback (void *data)
{
	GtkAdjustment *adjustment;
	float value;

	adjustment = GTK_PROGRESS (data)->adjustment;
	value = adjustment->value + 1;
	if (value > adjustment->upper)
		value = adjustment->lower;

	gtk_progress_set_value (GTK_PROGRESS (data), value);

	return TRUE;
}

static void
progress_bar_destroy_callback (GtkObject *object,
			       void *data)
{
	int timeout_id;

	timeout_id = GPOINTER_TO_INT (data);
	g_source_remove (timeout_id);
}

static int
progress_dialog_close_callback (GnomeDialog *dialog,
				void *data)
{
	/* Don't allow the dialog to be closed through the window manager close
	   command.  */
	return TRUE;
}

/* This is invoked if the "Cancel" button is clicked.  */
static void
progress_dialog_clicked_callback (GnomeDialog *dialog,
				  int button_number,
				  void *data)
{
	DiscoveryData *discovery_data;

	discovery_data = (DiscoveryData *) data;

	e_storage_cancel_discover_shared_folder (discovery_data->storage,
						 discovery_data->user_email_address,
						 discovery_data->folder_name);

	cleanup_discovery (discovery_data);
}

static int
progress_dialog_show_timeout_callback (void *data)
{
	GtkWidget *dialog;

	dialog = GTK_WIDGET (data);
	gtk_widget_show_all (dialog);
	return FALSE;
}

static GtkWidget *
create_progress_dialog (EShell *shell,
			EStorage *storage,
			const char *user_email_address,
			const char *folder_name)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *progress_bar;
	int timeout_id;
	char *text;

	dialog = gnome_dialog_new (_("Opening Folder"), GNOME_STOCK_BUTTON_CANCEL, NULL);
	gtk_widget_set_usize (dialog, 300, -1);
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

	gtk_signal_connect (GTK_OBJECT (dialog), "close",
			    GTK_SIGNAL_FUNC (progress_dialog_close_callback), NULL);

	text = g_strdup_printf (_("Opening Folder \"%s\""), folder_name);
	label = gtk_label_new (text);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, FALSE, TRUE, 0);
	g_free (text);

	text = g_strdup_printf (_("in \"%s\" ..."), e_storage_get_name (storage));
	label = gtk_label_new (text);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, FALSE, TRUE, 0);
	g_free (text);

	progress_bar = gtk_progress_bar_new ();
	gtk_progress_set_activity_mode (GTK_PROGRESS (progress_bar), TRUE);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), progress_bar, FALSE, TRUE, 0);

	timeout_id = g_timeout_add (50, progress_bar_timeout_callback, progress_bar);
	gtk_signal_connect (GTK_OBJECT (progress_bar), "destroy",
			    GTK_SIGNAL_FUNC (progress_bar_destroy_callback),
			    GINT_TO_POINTER (timeout_id));

	timeout_id = g_timeout_add (PROGRESS_DIALOG_DELAY, progress_dialog_show_timeout_callback, dialog);
	gtk_signal_connect (GTK_OBJECT (progress_bar), "destroy",
			    GTK_SIGNAL_FUNC (progress_bar_destroy_callback),
			    GINT_TO_POINTER (timeout_id));

	return dialog;
}

static void
shell_destroy_callback (GtkObject *object,
			void *data)
{
	DiscoveryData *discovery_data;

	discovery_data = (DiscoveryData *) data;
	cleanup_discovery (discovery_data);
}

static void
shell_view_destroy_callback (GtkObject *object,
			     void *data)
{
	DiscoveryData *discovery_data;

	discovery_data = (DiscoveryData *) data;
	discovery_data->parent = NULL;
}

static void
storage_destroy_callback (GtkObject *object,
			  void *data)
{
	DiscoveryData *discovery_data;

	discovery_data = (DiscoveryData *) data;
	cleanup_discovery (discovery_data);

	/* FIXME: Should we signal the user when this happens?  I.e. when the
	   storage dies for some reason before the folder is discovered.  */
}

static void
shared_folder_discovery_callback (EStorage *storage,
				  EStorageResult result,
				  const char *path,
				  void *data)
{
	DiscoveryData *discovery_data;
	EShell *shell;
	EShellView *parent;

	discovery_data = (DiscoveryData *) data;
	shell = discovery_data->shell;
	parent = discovery_data->parent;

	/* Make sure the progress dialog doesn't show up now. */
	cleanup_discovery (discovery_data);

	if (result == E_STORAGE_OK) {
		char *uri;

		uri = g_strconcat (E_SHELL_URI_PREFIX, "/",
				   e_storage_get_name (storage),
				   path, NULL);

		if (discovery_data->parent != NULL)
			e_shell_view_display_uri (parent, uri, TRUE);
		else
			e_shell_create_view (shell, uri, NULL);
	} else {
		e_notice (parent ? GTK_WINDOW (parent) : NULL,
			  GNOME_MESSAGE_BOX_ERROR,
			  _("Could not open shared folder: %s."),
			  e_storage_result_to_string (result));
	}
}

static void
discover_folder (EShell *shell,
		 EShellView *parent,
		 const char *user_email_address,
		 const char *storage_name,
		 const char *folder_name)
{
	EStorageSet *storage_set;
	EStorage *storage;
	GtkWidget *dialog;
	DiscoveryData *discovery_data;

	storage_set = e_shell_get_storage_set (shell);
	if (storage_set == NULL)
		goto error;

	storage = e_storage_set_get_storage (storage_set, storage_name);
	if (storage == NULL || ! e_storage_supports_shared_folders (storage))
		goto error;

	dialog = create_progress_dialog (shell, storage, user_email_address, folder_name);

	discovery_data = g_new (DiscoveryData, 1);
	discovery_data->dialog             = dialog;
	discovery_data->shell              = shell;
	discovery_data->parent             = parent;
	discovery_data->user_email_address = g_strdup (user_email_address);
	discovery_data->folder_name        = g_strdup (folder_name);
	discovery_data->storage            = storage;
	gtk_object_ref (GTK_OBJECT (storage));

	gtk_signal_connect (GTK_OBJECT (shell), "destroy",
			    GTK_SIGNAL_FUNC (shell_destroy_callback), discovery_data);

	gtk_signal_connect (GTK_OBJECT (parent), "destroy",
			    GTK_SIGNAL_FUNC (shell_view_destroy_callback), discovery_data);

	gtk_signal_connect (GTK_OBJECT (storage), "destroy",
			    GTK_SIGNAL_FUNC (storage_destroy_callback), discovery_data);

	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (progress_dialog_clicked_callback), discovery_data);

	e_storage_async_discover_shared_folder (storage,
						user_email_address,
						folder_name,
						shared_folder_discovery_callback,
						discovery_data);
	return;

 error:
	/* FIXME: Be more verbose?  */
	e_notice (GTK_WINDOW (parent), GNOME_MESSAGE_BOX_ERROR,
		  _("Cannot find the specified shared folder."));
}


void
e_shell_show_shared_folder_picker_dialog (EShell *shell,
					  EShellView *parent)
{
	char *user_email_address;
	char *storage_name;
	char *folder_name;

	g_return_if_fail (E_IS_SHELL (shell));

	if (! show_dialog (shell, parent, &user_email_address, &storage_name, &folder_name))
		return;

	discover_folder (shell, parent, user_email_address, storage_name, folder_name);

	g_free (user_email_address);
	g_free (storage_name);
	g_free (folder_name);
}
