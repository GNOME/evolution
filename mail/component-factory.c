/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c
 *
 * Authors: Ettore Perazzoli <ettore@helixcode.com>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-generic-factory.h>
#include <gal/widgets/e-gui-utils.h>

#include "camel.h"

#include "Evolution.h"
#include "evolution-storage.h"

#include "folder-browser-factory.h"
#include "evolution-shell-component.h"
#include "evolution-shell-component-dnd.h"
#include "folder-browser.h"
#include "mail.h"		/* YUCK FIXME */
#include "mail-config.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-offline-handler.h"
#include "mail-local.h"
#include "mail-session.h"
#include "mail-mt.h"
#include "mail-importer.h"
#include "mail-vfolder.h"             /* vfolder_create_storage */

#include "component-factory.h"

#include "mail-send-recv.h"

char *default_drafts_folder_uri;
CamelFolder *drafts_folder = NULL;
char *default_sent_folder_uri;
CamelFolder *sent_folder = NULL;
char *default_outbox_folder_uri;
CamelFolder *outbox_folder = NULL;
char *evolution_dir;

#define COMPONENT_FACTORY_ID "OAFIID:GNOME_Evolution_Mail_ShellComponentFactory"
#define SUMMARY_FACTORY_ID   "OAFIID:GNOME_Evolution_Mail_ExecutiveSummaryComponentFactory"

static BonoboGenericFactory *component_factory = NULL;
static GHashTable *storages_hash;
static EvolutionShellComponent *shell_component;

enum {
	ACCEPTED_DND_TYPE_MESSAGE_RFC822,
	ACCEPTED_DND_TYPE_X_EVOLUTION_MESSAGE,
	ACCEPTED_DND_TYPE_TEXT_URI_LIST,
};

static char *accepted_dnd_types[] = {
	"message/rfc822",
	"x-evolution-message",    /* ...from an evolution message list... */
	"text/uri-list",          /* ...from nautilus... */
	NULL
};

enum {
	EXPORTED_DND_TYPE_TEXT_URI_LIST,
};

static char *exported_dnd_types[] = {
	"text/uri-list",          /* we have to export to nautilus as text/uri-list */
	NULL
};

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "mail", "evolution-inbox.png", accepted_dnd_types, exported_dnd_types },
	{ "mailstorage", "evolution-inbox.png", NULL, NULL },
	{ "vtrash", "evolution-trash.png", NULL, NULL },
	{ NULL, NULL, NULL, NULL }
};

/* EvolutionShellComponent methods and signals.  */

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     const char *folder_type,
	     BonoboControl **control_return,
	     void *closure)
{
	EvolutionShellClient *shell_client;
	GNOME_Evolution_Shell corba_shell;
	BonoboControl *control;
	
	shell_client = evolution_shell_component_get_owner (shell_component);
	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	if (g_strcasecmp (folder_type, "mail") == 0) {
		control = folder_browser_factory_new_control (physical_uri,
							      corba_shell);
	} else if (g_strcasecmp (folder_type, "mailstorage") == 0) {
		CamelService *store;
		EvolutionStorage *storage;

		store = camel_session_get_service (session, physical_uri,
						   CAMEL_PROVIDER_STORE, NULL);
		if (!store)
			return EVOLUTION_SHELL_COMPONENT_NOTFOUND;
		storage = g_hash_table_lookup (storages_hash, store);
		if (!storage) {
			camel_object_unref (CAMEL_OBJECT (store));
			return EVOLUTION_SHELL_COMPONENT_NOTFOUND;
		}

		if (!gtk_object_get_data (GTK_OBJECT (storage), "connected"))
			mail_scan_subfolders (CAMEL_STORE(store), storage);
		camel_object_unref (CAMEL_OBJECT (store));

		control = folder_browser_factory_new_control ("", corba_shell);
	} else if (g_strcasecmp (folder_type, "vtrash") == 0) {
		control = folder_browser_factory_new_control ("vtrash:file:/", corba_shell);
	} else
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;

	if (!control)
		return EVOLUTION_SHELL_COMPONENT_NOTFOUND;

	*control_return = control;
	return EVOLUTION_SHELL_COMPONENT_OK;
}

static void
do_create_folder (char *uri, CamelFolder *folder, void *data)
{
	GNOME_Evolution_ShellComponentListener listener = data;
	CORBA_Environment ev;
	GNOME_Evolution_ShellComponentListener_Result result;

	if (folder)
		result = GNOME_Evolution_ShellComponentListener_OK;
	else
		result = GNOME_Evolution_ShellComponentListener_INVALID_URI;

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellComponentListener_notifyResult (listener, result, &ev);
	CORBA_Object_release (listener, &ev);
	CORBA_exception_free (&ev);
}

static void
create_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const char *type,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	char *uri;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	if (!strcmp (type, "mail")) {
		/* This makes the uri start with mbox://file://, which
		   looks silly but turns into a CamelURL that has
		   url->provider of "mbox" */
		uri = g_strdup_printf ("mbox://%s", physical_uri);
		mail_create_folder (uri, do_create_folder, CORBA_Object_duplicate (listener, &ev));
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_OK, &ev);
								     
	} else {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener, GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE, &ev);
	}
	CORBA_exception_free (&ev);
}

static void
do_remove_folder (char *uri, gboolean removed, void *data)
{
	GNOME_Evolution_ShellComponentListener listener = data;
	GNOME_Evolution_ShellComponentListener_Result result;
	CORBA_Environment ev;
	
	if (removed)
		result = GNOME_Evolution_ShellComponentListener_OK;
	else
		result = GNOME_Evolution_ShellComponentListener_INVALID_URI;
	
	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellComponentListener_notifyResult (listener, result, &ev);
	CORBA_Object_release (listener, &ev);
	CORBA_exception_free (&ev);
}

static void
remove_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	mail_remove_folder (physical_uri, do_remove_folder, CORBA_Object_duplicate (listener, &ev));
	GNOME_Evolution_ShellComponentListener_notifyResult (listener,
							     GNOME_Evolution_ShellComponentListener_OK, &ev);
	
	CORBA_exception_free (&ev);
}

static void
do_xfer_folder (char *src_uri, char *dest_uri, gboolean remove_source, CamelFolder *dest_folder, void *data)
{
	GNOME_Evolution_ShellComponentListener listener = data;
	GNOME_Evolution_ShellComponentListener_Result result;
	CORBA_Environment ev;
	
	if (dest_folder)
		result = GNOME_Evolution_ShellComponentListener_OK;
	else
		result = GNOME_Evolution_ShellComponentListener_INVALID_URI;
	
	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellComponentListener_notifyResult (listener, result, &ev);
	CORBA_Object_release (listener, &ev);
	CORBA_exception_free (&ev);
}

static void
xfer_folder (EvolutionShellComponent *shell_component,
	     const char *source_physical_uri,
	     const char *destination_physical_uri,
	     gboolean remove_source,
	     const GNOME_Evolution_ShellComponentListener listener,
	     void *closure)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	mail_xfer_folder (source_physical_uri, destination_physical_uri, remove_source, do_xfer_folder,
			  CORBA_Object_duplicate (listener, &ev));
	GNOME_Evolution_ShellComponentListener_notifyResult (listener,
							     GNOME_Evolution_ShellComponentListener_OK, &ev);
	CORBA_exception_free (&ev);
}

static char *
get_dnd_selection (EvolutionShellComponent *shell_component,
		   const char *physical_uri,
		   int type,
		   int *format_return,
		   const char **selection_return,
		   int *selection_length_return,
		   void *closure)
{
	g_print ("should get dnd selection for %s\n", physical_uri);
	
	return NULL;
}

/* Destination side DnD */
static CORBA_boolean
destination_folder_handle_motion (EvolutionShellComponentDndDestinationFolder *folder,
				  const char *physical_uri,
				  const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context *destination_context,
				  GNOME_Evolution_ShellComponentDnd_Action *suggested_action_return,
				  gpointer user_data)
{
	g_print ("in destination_folder_handle_motion (%s)\n", physical_uri);
	
	*suggested_action_return = GNOME_Evolution_ShellComponentDnd_ACTION_MOVE;
	
	return TRUE;
}

static gboolean
message_rfc822_dnd (CamelFolder *dest, CamelStream *stream)
{
	gboolean retval = FALSE;
	CamelMimeParser *mp;
	CamelException *ex;
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, TRUE);
	camel_mime_parser_init_with_stream (mp, stream);
	
	ex = camel_exception_new ();
	
	while (camel_mime_parser_step (mp, 0, 0) == HSCAN_FROM) {
		CamelMessageInfo *info;
		CamelMimeMessage *msg;
		
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mp) == -1) {
			camel_object_unref (CAMEL_OBJECT (msg));
			break;
		} else {
			/* we got at least 1 message so we will return TRUE */
			retval = TRUE;
		}
		
		/* append the message to the folder... */
		info = g_new0 (CamelMessageInfo, 1);
		camel_folder_append_message (dest, msg, info, ex);
		camel_exception_clear (ex);
		camel_object_unref (CAMEL_OBJECT (msg));
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}
	
	camel_object_unref (CAMEL_OBJECT (mp));
	camel_exception_free (ex);
	
	return retval;
}

static CORBA_boolean
destination_folder_handle_drop (EvolutionShellComponentDndDestinationFolder *folder,
				const char *physical_uri,
				const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context *destination_context,
				const GNOME_Evolution_ShellComponentDnd_Action action,
				const GNOME_Evolution_ShellComponentDnd_Data *data,
				gpointer user_data)
{
	char *url, *name, *in, *inptr, *inend;
	gboolean retval = FALSE;
	CamelFolder *source;
	CamelStream *stream;
	GPtrArray *uids;
	CamelURL *uri;
	int type, fd;
	
	if (action == GNOME_Evolution_ShellComponentDnd_ACTION_LINK)
		return FALSE; /* we can't create links */
	
	g_print ("in destination_folder_handle_drop (%s)\n", physical_uri);
	
	for (type = 0; accepted_dnd_types[type]; type++)
		if (!strcmp (destination_context->dndType, accepted_dnd_types[type]))
			break;
	
	switch (type) {
	case ACCEPTED_DND_TYPE_TEXT_URI_LIST:
		source = mail_tool_uri_to_folder (physical_uri, NULL);
		if (!source)
			return FALSE;
		
		url = g_strndup (data->bytes._buffer, data->bytes._length);
		inend = strchr (url, '\n');
		if (inend)
			*inend = '\0';
		
		/* get the path component */
		g_strstrip (url);
		uri = camel_url_new (url, NULL);
		g_free (url);
		url = uri->path;
		uri->path = NULL;
		camel_url_free (uri);
		
		fd = open (url, O_RDONLY);
		if (fd == -1) {
			g_free (url);
			return FALSE;
		}
		
		stream = camel_stream_fs_new_with_fd (fd);
		retval = message_rfc822_dnd (source, stream);
		camel_object_unref (CAMEL_OBJECT (stream));
		camel_object_unref (CAMEL_OBJECT (source));
		
		if (action == GNOME_Evolution_ShellComponentDnd_ACTION_MOVE)
			unlink (url);
		
		g_free (url);
		break;
	case ACCEPTED_DND_TYPE_MESSAGE_RFC822:
		source = mail_tool_uri_to_folder (physical_uri, NULL);
		if (!source)
			return FALSE;
		
		/* write the message(s) out to a CamelStream so we can use it */
		stream = camel_stream_mem_new ();
		camel_stream_write (stream, data->bytes._buffer, data->bytes._length);
		camel_stream_reset (stream);
		
		retval = message_rfc822_dnd (source, stream);
		camel_object_unref (CAMEL_OBJECT (stream));
		camel_object_unref (CAMEL_OBJECT (source));
		break;
	case ACCEPTED_DND_TYPE_X_EVOLUTION_MESSAGE:
		/* format: "url folder_name uid1\0uid2\0uid3\0...\0uidn" */
		
		in = data->bytes._buffer;
		inend = in + data->bytes._length;
		
		inptr = strchr (in, ' ');
		url = g_strndup (in, inptr - in);
		
		name = inptr + 1;
		inptr = strchr (name, ' ');
		name = g_strndup (name, inptr - name);
		
		source = mail_tool_get_folder_from_urlname (url, name, 0, NULL);
		g_free (name);
		g_free (url);
		
		if (!source)
			return FALSE;
		
		/* split the uids */
		inptr++;
		uids = g_ptr_array_new ();
		while (inptr < inend) {
			char *start = inptr;
			
			while (inptr < inend && *inptr)
				inptr++;
			
			g_ptr_array_add (uids, g_strndup (start, inptr - start));
			inptr++;
		}
		
		mail_do_transfer_messages (source, uids,
					   action == GNOME_Evolution_ShellComponentDnd_ACTION_MOVE,
					   physical_uri);
		
		camel_object_unref (CAMEL_OBJECT (source));
		break;
	default:
		break;
	}
	
	return retval;
}


static struct {
	char *name, **uri;
	CamelFolder **folder;
} standard_folders[] = {
	{ "Drafts", &default_drafts_folder_uri, &drafts_folder },
	{ "Outbox", &default_outbox_folder_uri, &outbox_folder },
	{ "Sent", &default_sent_folder_uri, &sent_folder },
};

static void
unref_standard_folders (void)
{
	int i;
	
	for (i = 0; i < sizeof (standard_folders) / sizeof (standard_folders[0]); i++) {
		if (standard_folders[i].folder)
			camel_object_unref (CAMEL_OBJECT (*standard_folders[i].folder));
	}
}

static void
got_folder (char *uri, CamelFolder *folder, void *data)
{
	CamelFolder **fp = data;
	
	if (folder) {
		*fp = folder;
		camel_object_ref (CAMEL_OBJECT (folder));
	}
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	GNOME_Evolution_Shell corba_shell;
	const GSList *accounts;
#ifdef ENABLE_NNTP
	const GSList *news;
#endif
	int i;

	g_print ("evolution-mail: Yeeeh! We have an owner!\n");	/* FIXME */

	evolution_dir = g_strdup (evolution_homedir);
	mail_session_init ();
	
	mail_config_init ();
	
	storages_hash = g_hash_table_new (NULL, NULL);
	
	vfolder_create_storage (shell_component);

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
	
	accounts = mail_config_get_accounts ();
	mail_load_storages (corba_shell, accounts, TRUE);
	
#ifdef ENABLE_NNTP
	news = mail_config_get_news ();
	mail_load_storages (corba_shell, news, FALSE);
#endif

	mail_local_storage_startup (shell_client, evolution_dir);
	mail_importer_init (shell_client);
	
	for (i = 0; i < sizeof (standard_folders) / sizeof (standard_folders[0]); i++) {
		*standard_folders[i].uri = g_strdup_printf ("file://%s/local/%s", evolution_dir, standard_folders[i].name);
		mail_msg_wait (mail_get_folder (*standard_folders[i].uri, got_folder, standard_folders[i].folder));
	}
	
	mail_session_enable_interaction (TRUE);
	
	mail_autoreceive_setup ();
}

static void
free_storage (gpointer service, gpointer storage, gpointer data)
{
	camel_service_disconnect (CAMEL_SERVICE (service), TRUE, NULL);
	camel_object_unref (CAMEL_OBJECT (service));
	bonobo_object_unref (BONOBO_OBJECT (storage));
}

static gboolean
idle_quit (gpointer user_data)
{
	if (e_list_length (folder_browser_factory_get_control_list ()))
		return TRUE;

	bonobo_object_unref (BONOBO_OBJECT (component_factory));
	g_hash_table_foreach (storages_hash, free_storage, NULL);
	g_hash_table_destroy (storages_hash);

	gtk_main_quit ();

	return FALSE;
}	

static void
owner_unset_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	unref_standard_folders ();
	mail_importer_uninit ();
	
	g_idle_add_full (G_PRIORITY_LOW, idle_quit, NULL, NULL);
}

static void
debug_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	extern gboolean camel_verbose_debug;

	camel_verbose_debug = 1;
}

static BonoboObject *
component_fn (BonoboGenericFactory *factory, void *closure)
{
	EvolutionShellComponentDndDestinationFolder *destination_interface;
	MailOfflineHandler *offline_handler;
	
	shell_component = evolution_shell_component_new (folder_types,
							 create_view,
							 create_folder,
							 remove_folder,
							 xfer_folder,
							 NULL, /* populate_folder_context_menu_fn */
							 get_dnd_selection,
							 NULL);
	
	destination_interface = evolution_shell_component_dnd_destination_folder_new (destination_folder_handle_motion,
										      destination_folder_handle_drop,
										      shell_component);
	
	bonobo_object_add_interface (BONOBO_OBJECT (shell_component),
				     BONOBO_OBJECT (destination_interface));
	
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "debug",
			    GTK_SIGNAL_FUNC (debug_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "destroy",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);
	
	offline_handler = mail_offline_handler_new ();
	bonobo_object_add_interface (BONOBO_OBJECT (shell_component), BONOBO_OBJECT (offline_handler));
	
	return BONOBO_OBJECT (shell_component);
}

void
component_factory_init (void)
{
	component_factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID,
							component_fn, NULL);

	evolution_mail_config_factory_init ();
	evolution_folder_info_factory_init ();

	if (component_factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's mail component."));
		exit (1);
	}
}

static int
storage_create_folder (EvolutionStorage *storage,
		       const char *path,
		       const char *type,
		       const char *description,
		       const char *parent_physical_uri,
		       gpointer user_data)
{
	CamelStore *store = user_data;
	char *name;
	CamelURL *url;
	CamelException ex;
	CamelFolderInfo *fi;

	if (strcmp (type, "mail") != 0)
		return EVOLUTION_STORAGE_ERROR_UNSUPPORTED_TYPE;

	name = strrchr (path, '/');
	if (!name++)
		return EVOLUTION_STORAGE_ERROR_INVALID_URI;
	
	camel_exception_init (&ex);
	if (*parent_physical_uri) {
		url = camel_url_new (parent_physical_uri, NULL);
		if (!url) {
			return EVOLUTION_STORAGE_ERROR_INVALID_URI;
		}
		
		fi = camel_store_create_folder (store, url->path + 1, name, &ex);
		camel_url_free (url);
	} else
		fi = camel_store_create_folder (store, NULL, name, &ex);
	
	if (camel_exception_is_set (&ex)) {
		/* FIXME: do better than this */
		camel_exception_clear (&ex);
		return EVOLUTION_STORAGE_ERROR_INVALID_URI;
	}
	
	if (camel_store_supports_subscriptions (store))
		camel_store_subscribe_folder (store, fi->full_name, NULL);
	
	folder_created (store, fi);
	
	camel_store_free_folder_info (store, fi);
	
	return EVOLUTION_STORAGE_OK;
}

static int
storage_remove_folder (EvolutionStorage *storage,
		       const char *path,
		       const char *physical_uri,
		       gpointer user_data)
{
	CamelStore *store = user_data;
	CamelURL *url = NULL;
	CamelFolderInfo *fi;
	CamelException ex;
	
	g_warning ("storage_remove_folder: path=\"%s\"; uri=\"%s\"", path, physical_uri);
	
	if (*physical_uri) {
		url = camel_url_new (physical_uri, NULL);
		if (!url)
			return EVOLUTION_STORAGE_ERROR_INVALID_URI;
	} else {
		if (!*path)
			return EVOLUTION_STORAGE_ERROR_INVALID_URI;
	}
	
	camel_exception_init (&ex);
	fi = camel_store_get_folder_info (store, url ? url->path + 1 : path + 1,
					  CAMEL_STORE_FOLDER_INFO_FAST, &ex);
	if (url)
		camel_url_free (url);
	if (camel_exception_is_set (&ex))
		goto exception;
	
	camel_store_delete_folder (store, fi->full_name, &ex);
	if (camel_exception_is_set (&ex))
		goto exception;
	
	if (camel_store_supports_subscriptions (store))
		camel_store_unsubscribe_folder (store, fi->full_name, NULL);
	
	folder_deleted (store, fi);
	
	camel_store_free_folder_info (store, fi);
	
	return EVOLUTION_STORAGE_OK;
	
 exception:
	/* FIXME: do better than this... */
	
	if (fi)
		camel_store_free_folder_info (store, fi);
	
	return EVOLUTION_STORAGE_ERROR_INVALID_URI;
}

static void
add_storage (const char *name, const char *uri, CamelService *store,
	     GNOME_Evolution_Shell corba_shell, CamelException *ex)
{
	EvolutionStorage *storage;
	EvolutionStorageResult res;
	
	storage = evolution_storage_new (name, uri, "mailstorage");
	gtk_signal_connect (GTK_OBJECT (storage), "create_folder",
			    GTK_SIGNAL_FUNC (storage_create_folder),
			    store);
	gtk_signal_connect (GTK_OBJECT (storage), "remove_folder",
			    GTK_SIGNAL_FUNC (storage_remove_folder),
			    store);
	
	res = evolution_storage_register_on_shell (storage, corba_shell);
	
	switch (res) {
	case EVOLUTION_STORAGE_OK:
		mail_hash_storage (store, storage);
		mail_scan_subfolders (CAMEL_STORE (store), storage);
		/* falllll */
	case EVOLUTION_STORAGE_ERROR_ALREADYREGISTERED:
	case EVOLUTION_STORAGE_ERROR_EXISTS:
		return;
	default:
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot register storage with shell"));
		break;
	}
}


/* FIXME: 'is_account_data' is an ugly hack, if we remove support for NNTP we can take it out -- fejj */
void
mail_load_storages (GNOME_Evolution_Shell shell, const GSList *sources, gboolean is_account_data)
{
	CamelException ex;
	const GSList *iter;
	
	camel_exception_init (&ex);
	
	/* Load each service (don't connect!). Check its provider and
	 * see if this belongs in the shell's folder list. If so, add
	 * it.
	 */
	
	for (iter = sources; iter; iter = iter->next) {
		const MailConfigAccount *account = NULL;
		const MailConfigService *service = NULL;
		CamelService *store;
		CamelProvider *prov;
		char *name;
		
		if (is_account_data) {
			account = iter->data;
			service = account->source;
		} else {
			service = iter->data;
		}
		
		if (service->url == NULL || service->url[0] == '\0')
			continue;
		
		prov = camel_session_get_provider (session, service->url, &ex);
		if (prov == NULL) {
			/* FIXME: real error dialog */
			g_warning ("couldn't get service %s: %s\n", service->url,
				   camel_exception_get_description (&ex));
			camel_exception_clear (&ex);
			continue;
		}
		
		/* FIXME: this case is ambiguous for things like the
		 * mbox provider, which can really be a spool
		 * (/var/spool/mail/user) or a storage (~/mail/, eg).
		 * That issue can't be resolved on the provider level
		 * -- it's a per-URL problem.
		 *  MPZ Added a hack to let spool protocol through temporarily ...
		 */
		if ((!(prov->flags & CAMEL_PROVIDER_IS_STORAGE) ||
		     !(prov->flags & CAMEL_PROVIDER_IS_REMOTE))
		    && !((strcmp(prov->protocol, "spool") == 0)
			 || strcmp(prov->protocol, "maildir") == 0))
			continue;
		
		store = camel_session_get_service (session, service->url,
						   CAMEL_PROVIDER_STORE, &ex);
		if (store == NULL) {
			/* FIXME: real error dialog */
			g_warning ("couldn't get service %s: %s\n", service->url,
				   camel_exception_get_description (&ex));
			camel_exception_clear (&ex);
			continue;
		}
		
		if (is_account_data)
			name = g_strdup (account->name);
		else
			name = camel_service_get_name (store, TRUE);
		
		add_storage (name, service->url, store, shell, &ex);
		g_free (name);
		
		if (camel_exception_is_set (&ex)) {
			/* FIXME: real error dialog */
			g_warning ("Cannot load storage: %s",
				   camel_exception_get_description (&ex));
			camel_exception_clear (&ex);
		}
		
		camel_object_unref (CAMEL_OBJECT (store));
	}
}

void
mail_hash_storage (CamelService *store, EvolutionStorage *storage)
{
	camel_object_ref (CAMEL_OBJECT (store));
	g_hash_table_insert (storages_hash, store, storage);
}

EvolutionStorage*
mail_lookup_storage (CamelStore *store)
{
	EvolutionStorage *storage;
	
	/* Because the storages_hash holds a reference to each store
	 * used as a key in it, none of them will ever be gc'ed, meaning
	 * any call to camel_session_get_{service,store} with the same
	 * URL will always return the same object. So this works.
	 */
	
	storage = g_hash_table_lookup (storages_hash, store);
	if (storage)
		gtk_object_ref (GTK_OBJECT (storage));
	
	return storage;
}

void
mail_remove_storage (CamelStore *store)
{
	EvolutionStorage *storage;
	EvolutionShellClient *shell_client;
	GNOME_Evolution_Shell corba_shell;
	
	/* Because the storages_hash holds a reference to each store
	 * used as a key in it, none of them will ever be gc'ed, meaning
	 * any call to camel_session_get_{service,store} with the same
	 * URL will always return the same object. So this works.
	 */

	storage = g_hash_table_lookup (storages_hash, store);
	g_hash_table_remove (storages_hash, store);
	
	shell_client = evolution_shell_component_get_owner (shell_component);
	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	evolution_storage_deregister_on_shell (storage, corba_shell);
	
	camel_service_disconnect (CAMEL_SERVICE (store), TRUE, NULL);
	camel_object_unref (CAMEL_OBJECT (store));
}

int
mail_storages_count (void)
{
	return g_hash_table_size (storages_hash);
}

void
mail_storages_foreach (GHFunc func, gpointer data)
{
	g_hash_table_foreach (storages_hash, func, data);
}
