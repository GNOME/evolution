/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-component.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>

#include "evolution-shell-component.h"
#include "evolution-shell-component-dnd.h"
#include "evolution-storage.h"
#include "e-folder-list.h"

#include "ebook/e-book.h"
#include "ebook/e-card.h"
#include "ebook/e-book-util.h"

#include "addressbook-config.h"
#include "addressbook-storage.h"
#include "addressbook-component.h"
#include "addressbook.h"
#include "addressbook/gui/merging/e-card-merging.h"
#include "addressbook/gui/widgets/e-addressbook-util.h"



#define GNOME_EVOLUTION_ADDRESSBOOK_COMPONENT_ID "OAFIID:GNOME_Evolution_Addressbook_ShellComponent"

EvolutionShellClient *global_shell_client = NULL;

EvolutionShellClient *
addressbook_component_get_shell_client  (void)
{
	return global_shell_client;
}

static char *accepted_dnd_types[] = {
	"text/x-vcard",
	NULL
};

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "contacts", "evolution-contacts.png", N_("Contacts"), N_("Folder containing contact information"),
	  TRUE, accepted_dnd_types, NULL },
	{ "contacts/ldap", "ldap.png", N_("LDAP Server"), N_("LDAP server containing contact information"),
	  FALSE, accepted_dnd_types, NULL },
	{ "contacts/public", "evolution-contacts.png", N_("Public Contacts"), N_("Public folder containing contact information"),
	  FALSE, accepted_dnd_types, NULL },
	{ NULL }
};

#define IS_CONTACT_TYPE(x)  (g_ascii_strcasecmp((x), "contacts") == 0 || g_ascii_strcasecmp ((x), "contacts/ldap") == 0 || g_ascii_strcasecmp((x), "contacts/public") == 0)

/* EvolutionShellComponent methods and signals.  */

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     const char *type,
	     const char *view_info,
	     BonoboControl **control_return,
	     void *closure)
{
	BonoboControl *control;

	if (!IS_CONTACT_TYPE (type))
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;

	control = addressbook_new_control ();
	bonobo_control_set_property (control, NULL, "folder_uri", TC_CORBA_string, physical_uri, NULL);

	*control_return = control;

	return EVOLUTION_SHELL_COMPONENT_OK;
}

static void
create_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const char *type,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	CORBA_Environment ev;
	GNOME_Evolution_ShellComponentListener_Result result;

	if (!IS_CONTACT_TYPE (type))
		result = GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE;
	else 
		result = GNOME_Evolution_ShellComponentListener_OK;

	CORBA_exception_init(&ev);
	GNOME_Evolution_ShellComponentListener_notifyResult(listener, result, &ev);
	CORBA_exception_free(&ev);
}

static void
remove_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const char *type,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	CORBA_Environment ev;
	char *db_path, *summary_path, *subdir_path;
	struct stat sb;
	int rv;

	CORBA_exception_init(&ev);

	if (!IS_CONTACT_TYPE (type)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE,
								     &ev);
		CORBA_exception_free(&ev);
		return;
	}

	if (!strncmp (physical_uri, "ldap://", 7)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
								     &ev);
		CORBA_exception_free(&ev);
		return;
	}
	if (strncmp (physical_uri, "file://", 7)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_INVALID_URI,
								     &ev);
		CORBA_exception_free(&ev);
		return;
	}

	subdir_path = g_build_filename (physical_uri + 7, "subfolders", NULL);
	rv = stat (subdir_path, &sb);
	g_free (subdir_path);
	if (rv != -1) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_HAS_SUBFOLDERS,
								     &ev);
		CORBA_exception_free(&ev);
		return;
	}

	db_path = g_build_filename (physical_uri + 7, "addressbook.db", NULL);
	summary_path = g_build_filename (physical_uri + 7, "addressbook.db.summary", NULL);
	rv = unlink (db_path);

	if (rv == 0 || (rv == -1 && errno == ENOENT))
		rv = unlink (summary_path);

	if (rv == 0 || (rv == -1 && errno == ENOENT)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_OK,
								     &ev);
	}
	else {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED,
								     &ev);
	}

	g_free (db_path);
	g_free (summary_path);

	CORBA_exception_free(&ev);
}

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

/* This code is cut & pasted from calendar/gui/component-factory.c */

static GNOME_Evolution_ShellComponentListener_Result
xfer_file (GnomeVFSURI *base_src_uri,
	   GnomeVFSURI *base_dest_uri,
	   const char *file_name,
	   int remove_source)
{
	GnomeVFSURI *src_uri, *dest_uri;
	GnomeVFSHandle *hin, *hout;
	GnomeVFSResult result;
	GnomeVFSFileInfo file_info;
	GnomeVFSFileSize size;
	char *buffer;
	
	src_uri = gnome_vfs_uri_append_file_name (base_src_uri, file_name);

	result = gnome_vfs_open_uri (&hin, src_uri, GNOME_VFS_OPEN_READ);
	if (result == GNOME_VFS_ERROR_NOT_FOUND) {
		gnome_vfs_uri_unref (src_uri);
		return GNOME_Evolution_ShellComponentListener_OK; /* No need to xfer anything.  */
	}
	if (result != GNOME_VFS_OK) {
		gnome_vfs_uri_unref (src_uri);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	result = gnome_vfs_get_file_info_uri (src_uri, &file_info, GNOME_VFS_FILE_INFO_DEFAULT);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_uri_unref (src_uri);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	dest_uri = gnome_vfs_uri_append_file_name (base_dest_uri, file_name);

	result = gnome_vfs_create_uri (&hout, dest_uri, GNOME_VFS_OPEN_WRITE, FALSE, 0600);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_close (hin);
		gnome_vfs_uri_unref (src_uri);
		gnome_vfs_uri_unref (dest_uri);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	/* write source file to destination file */
	buffer = g_malloc (file_info.size);
	result = gnome_vfs_read (hin, buffer, file_info.size, &size);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_close (hin);
		gnome_vfs_close (hout);
		gnome_vfs_uri_unref (src_uri);
		gnome_vfs_uri_unref (dest_uri);
		g_free (buffer);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	result = gnome_vfs_write (hout, buffer, file_info.size, &size);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_close (hin);
		gnome_vfs_close (hout);
		gnome_vfs_uri_unref (src_uri);
		gnome_vfs_uri_unref (dest_uri);
		g_free (buffer);
		return GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED;
	}

	if (remove_source) {
		char *text_uri;

		/* Sigh, we have to do this as there is no gnome_vfs_unlink_uri(). :-(  */

		text_uri = gnome_vfs_uri_to_string (src_uri, GNOME_VFS_URI_HIDE_NONE);
		result = gnome_vfs_unlink (text_uri);
		g_free (text_uri);
	}

	gnome_vfs_close (hin);
	gnome_vfs_close (hout);
	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dest_uri);
	g_free (buffer);

	return GNOME_Evolution_ShellComponentListener_OK;
}

static void
xfer_folder (EvolutionShellComponent *shell_component,
	     const char *source_physical_uri,
	     const char *destination_physical_uri,
	     const char *type,
	     gboolean remove_source,
	     const GNOME_Evolution_ShellComponentListener listener,
	     void *closure)
{
	CORBA_Environment ev;

	GnomeVFSURI *src_uri;
	GnomeVFSURI *dest_uri;
	GnomeVFSResult result;

	CORBA_exception_init (&ev);
	
	if (!IS_CONTACT_TYPE (type)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE,
								     &ev);
		CORBA_exception_free(&ev);
		return;
	}

	if (!strncmp (source_physical_uri, "ldap://", 7)
	    || !strncmp (destination_physical_uri, "ldap://", 7)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
							     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
							     &ev);
		CORBA_exception_free(&ev);
		return;
	}

	if (strncmp (source_physical_uri, "file://", 7)
	    || strncmp (destination_physical_uri, "file://", 7)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
							     GNOME_Evolution_ShellComponentListener_INVALID_URI,
							     &ev);
		CORBA_exception_free(&ev);
		return;
	}

	/* check URIs */
	src_uri = gnome_vfs_uri_new (source_physical_uri);
	dest_uri = gnome_vfs_uri_new (destination_physical_uri);
	if (!src_uri || ! dest_uri) {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener,
			GNOME_Evolution_ShellComponentListener_INVALID_URI,
			&ev);
		gnome_vfs_uri_unref (src_uri);
		gnome_vfs_uri_unref (dest_uri);
		CORBA_exception_free (&ev);
		return;
	}

	result = xfer_file (src_uri, dest_uri, "addressbook.db", remove_source);

	if ((result == GNOME_Evolution_ShellComponentListener_OK) && remove_source) {
		char *summary_uri;

		summary_uri = g_strconcat (source_physical_uri, "/addressbook.db.summary", NULL);
		result = gnome_vfs_unlink (summary_uri);
		g_free (summary_uri);
	}

	GNOME_Evolution_ShellComponentListener_notifyResult (listener, result, &ev);

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dest_uri);

        CORBA_exception_free (&ev);	
}

static char*
get_dnd_selection (EvolutionShellComponent *shell_component,
		   const char *physical_uri,
		   int type,
		   int *format_return,
		   const char **selection_return,
		   int *selection_length_return,
		   void *closure)
{
	/* g_print ("should get dnd selection for %s\n", physical_uri); */
	return NULL;
}

static int owner_count = 0;

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	owner_count ++;

	if (global_shell_client == NULL)
		global_shell_client = shell_client;

	addressbook_storage_setup (shell_component, evolution_homedir);
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component,
		GNOME_Evolution_Shell shell_interface,
		gpointer user_data)
{
	owner_count --;

	if (owner_count == 0)
		global_shell_client = NULL;

	addressbook_storage_cleanup ();
}

/* FIXME We should perhaps take the time to figure out if the book is editable. */
static void
new_item_cb (EBook *book, gpointer closure)
{
	gboolean is_list = GPOINTER_TO_INT (closure);
	ECard *card;

	if (book == NULL)
		return;

	card = e_card_new ("");
	if (is_list)
		e_addressbook_show_contact_list_editor (book, card, TRUE, TRUE);
	else
		e_addressbook_show_contact_editor (book, card, TRUE, TRUE);
	g_object_unref (card);
}

static void
user_create_new_item_cb (EvolutionShellComponent *shell_component,
			 const char *id,
			 const char *parent_folder_physical_uri,
			 const char *parent_folder_type,
			 gpointer data)
{
	gboolean is_contact_list;
	if (!strcmp (id, "contact")) {
		is_contact_list = FALSE;
	} else if (!strcmp (id, "contact_list")) {
		is_contact_list = TRUE;
	} else {
		g_warning ("Don't know how to create item of type \"%s\"", id);
		return;
	}
	if (IS_CONTACT_TYPE (parent_folder_type)) {
		e_book_use_address_book_by_uri (parent_folder_physical_uri,
						new_item_cb, GINT_TO_POINTER (is_contact_list));
	} else {
		e_book_use_default_book (new_item_cb, GINT_TO_POINTER (is_contact_list));
	}
}


/* Destination side DnD */

static CORBA_boolean
destination_folder_handle_motion (EvolutionShellComponentDndDestinationFolder *folder,
				  const char *physical_uri,
				  const char *folder_type,
				  const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context * destination_context,
				  GNOME_Evolution_ShellComponentDnd_Action * suggested_action_return,
				  gpointer user_data)
{
	*suggested_action_return = GNOME_Evolution_ShellComponentDnd_ACTION_MOVE;
	return TRUE;
}

static void
dnd_drop_book_open_cb (EBook *book, EBookStatus status, GList *card_list)
{
	GList *l;

	for (l = card_list; l; l = l->next) {
		ECard *card = l->data;

		e_card_merging_book_add_card (book, card, NULL /* XXX */, NULL);
	}
}

static CORBA_boolean
destination_folder_handle_drop (EvolutionShellComponentDndDestinationFolder *folder,
				const char *physical_uri,
				const char *folder_type,
				const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context * destination_context,
				const GNOME_Evolution_ShellComponentDnd_Action action,
				const GNOME_Evolution_ShellComponentDnd_Data * data,
				gpointer user_data)
{
	EBook *book;
	GList *card_list;
	char *expanded_uri;

	if (action == GNOME_Evolution_ShellComponentDnd_ACTION_LINK)
		return FALSE; /* we can't create links in our addressbook format */

	/* g_print ("in destination_folder_handle_drop (%s)\n", physical_uri); */

	card_list = e_card_load_cards_from_string_with_default_charset (data->bytes._buffer, "ISO-8859-1");

	expanded_uri = e_book_expand_uri (physical_uri);

	book = e_book_new ();
	addressbook_load_uri (book, expanded_uri,
			      (EBookCallback)dnd_drop_book_open_cb, card_list);

	g_free (expanded_uri);

	return TRUE;
}


/* Quitting.  */

static gboolean
request_quit (EvolutionShellComponent *shell_component,
	      void *data)
{
	if (! e_contact_editor_request_close_all ()
	    || ! e_contact_list_editor_request_close_all ())
		return FALSE;
	else
		return TRUE;
}


/* The factory function.  */

static void
add_creatable_item (EvolutionShellComponent *shell_component,
		    const char *id,
		    const char *description,
		    const char *menu_description,
		    const char *tooltip,
		    char menu_shortcut,
		    const char *icon_name)
{
	char *icon_path;
	GdkPixbuf *icon;

	if (icon_name == NULL) {
		icon_path = NULL;
		icon = NULL;
	} else {
		icon_path = g_build_filename (EVOLUTION_IMAGESDIR, icon_name, NULL);
		icon = gdk_pixbuf_new_from_file (icon_path, NULL);
	}

	evolution_shell_component_add_user_creatable_item (shell_component,
							   id,
							   description,
							   menu_description,
							   tooltip,
							   "contacts",
							   menu_shortcut,
							   icon);


	if (icon != NULL)
		gdk_pixbuf_unref (icon);
	g_free (icon_path);
}

static BonoboObject *
create_component (void)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentDndDestinationFolder *destination_interface;

	shell_component = evolution_shell_component_new (folder_types, NULL,
							 create_view, create_folder,
							 remove_folder, xfer_folder,
							 NULL, NULL,
							 get_dnd_selection,
							 request_quit,
							 NULL);

	destination_interface = evolution_shell_component_dnd_destination_folder_new (destination_folder_handle_motion,
										      destination_folder_handle_drop,
										      shell_component);

	bonobo_object_add_interface (BONOBO_OBJECT (shell_component),
				     BONOBO_OBJECT (destination_interface));

	add_creatable_item (shell_component, "contact",
			    _("New Contact"), _("_Contact"),
			    _("Create a new contact"), 'c',
			    "evolution-contacts-mini.png");
	add_creatable_item (shell_component, "contact_list",
			    _("New Contact List"), _("Contact _List"),
			    _("Create a new contact list"), 'l',
			    "contact-list-16.png");

	g_signal_connect (shell_component, "owner_set",
			  G_CALLBACK (owner_set_cb), NULL);
	g_signal_connect (shell_component, "owner_unset",
			  G_CALLBACK (owner_unset_cb), NULL);
	g_signal_connect (shell_component, "user_create_new_item",
			  G_CALLBACK (user_create_new_item_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}

static void
ensure_completion_uris_exist()
{
	/* Initialize the completion uris if they aren't set yet.  The
	   default set is just the local Contacts folder. */
	EConfigListener *db;
	char *val;

	db = e_book_get_config_database ();
		
	val = e_config_listener_get_string (db, "/apps/evolution/addressbook/completion/uris");

	if (val && !*val) {
		g_free (val);
		val = NULL;
	}
		
	if (!val) {
		EFolderListItem f[2];
		char *dirname, *uri;
		/* in the case where the user is running for the first
		   time, populate the list with the local contact
		   folder */
		dirname = g_build_filename (g_get_home_dir (), "evolution/local/Contacts", NULL);
		uri = g_strdup_printf ("file://%s", dirname);
			
		f[0].uri = "evolution:/local/Contacts";
		f[0].physical_uri = uri;
		f[0].display_name = _("Contacts");

		memset (&f[1], 0, sizeof (f[1]));

		val = e_folder_list_create_xml (f);

		g_free (dirname);
		g_free (uri);
		e_config_listener_set_string (db, "/apps/evolution/addressbook/completion/uris", val);
	}

	g_free (val);
}


/* FIXME this is wrong.  */
BonoboObject *
addressbook_component_init (void)
{
	ensure_completion_uris_exist ();
	return create_component ();
}
