/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c
 *
 * Authors: Ettore Perazzoli <ettore@ximian.com>
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <signal.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <camel/camel.h>
#include <camel/camel-vee-store.h>

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-shlib-factory.h>

#include <gal/widgets/e-gui-utils.h>

#include "e-util/e-dialog-utils.h"

#include "Evolution.h"
#include "evolution-storage.h"
#include "evolution-wizard.h"
#include "evolution-composer.h"

#include "folder-browser-factory.h"
#include "evolution-shell-component.h"
#include "evolution-shell-component-dnd.h"
#include "folder-browser.h"
#include "folder-info.h"
#include "mail.h"
#include "mail-config.h"
#include "mail-config-factory.h"
#include "mail-preferences.h"
#include "mail-composer-prefs.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-offline-handler.h"
#include "mail-local.h"
#include "mail-session.h"
#include "mail-mt.h"
#include "mail-importer.h"
#include "mail-folder-cache.h"

#include "component-factory.h"

#include "mail-send-recv.h"

#include "mail-vfolder.h"
#include "mail-autofilter.h"

#define d(x)

char *default_drafts_folder_uri;
CamelFolder *drafts_folder = NULL;
char *default_sent_folder_uri;
CamelFolder *sent_folder = NULL;
char *default_outbox_folder_uri;
CamelFolder *outbox_folder = NULL;
char *evolution_dir;

EvolutionShellClient *global_shell_client = NULL;

RuleContext *search_context = NULL;

static MailAsyncEvent *async_event = NULL;

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
	{ "mail", "evolution-inbox.png", N_("Mail"), N_("Folder containing mail"), TRUE, accepted_dnd_types, exported_dnd_types },
	{ "mail/public", "evolution-inbox.png", N_("Public Mail"), N_("Public folder containing mail"), FALSE, accepted_dnd_types, exported_dnd_types },
	{ "vtrash", "evolution-trash.png", N_("Virtual Trash"), N_("Virtual Trash folder"), FALSE, accepted_dnd_types, exported_dnd_types },
	{ NULL, NULL, NULL, NULL, FALSE, NULL, NULL }
};

static const char *schema_types[] = {
	"mailto",
	NULL
};

static inline gboolean
type_is_mail (const char *type)
{
	return !strcmp (type, "mail") || !strcmp (type, "mail/public");
}

static inline gboolean
type_is_vtrash (const char *type)
{
	return !strcmp (type, "vtrash");
}

/* EvolutionShellComponent methods and signals.  */

static BonoboControl *
create_noselect_control (void)
{
	GtkWidget *label;

	label = gtk_label_new (_("This folder cannot contain messages."));
	gtk_widget_show (label);
	return bonobo_control_new (label);
}

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     const char *folder_type,
	     const char *view_info,
	     BonoboControl **control_return,
	     void *closure)
{
	EvolutionShellClient *shell_client;
	GNOME_Evolution_Shell corba_shell;
	BonoboControl *control;
	
	shell_client = evolution_shell_component_get_owner (shell_component);
	corba_shell = evolution_shell_client_corba_objref(shell_client);
	
	if (type_is_mail (folder_type)) {
		const char *noselect;
		CamelURL *url;
		
		url = camel_url_new (physical_uri, NULL);
		noselect = url ? camel_url_get_param (url, "noselect") : NULL;
		if (noselect && !strcasecmp (noselect, "yes"))
			control = create_noselect_control ();
		else
			control = folder_browser_factory_new_control (physical_uri,
								      corba_shell);
		camel_url_free (url);
	} else if (type_is_vtrash (folder_type)) {
		if (!strncasecmp (physical_uri, "file:", 5))
			control = folder_browser_factory_new_control ("vtrash:file:/", corba_shell);
		else
			control = folder_browser_factory_new_control (physical_uri, corba_shell);
	} else
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;
	
	if (!control)
		return EVOLUTION_SHELL_COMPONENT_NOTFOUND;
	
	*control_return = control;
	return EVOLUTION_SHELL_COMPONENT_OK;
}

static void
create_folder_done (char *uri, CamelFolder *folder, void *data)
{
	GNOME_Evolution_ShellComponentListener listener = data;
	GNOME_Evolution_ShellComponentListener_Result result;
	CORBA_Environment ev;

	if (folder) {
		result = GNOME_Evolution_ShellComponentListener_OK;
	} else {
		result = GNOME_Evolution_ShellComponentListener_INVALID_URI;
	}

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
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	if (type_is_mail (type)) {
		mail_get_folder (physical_uri, CAMEL_STORE_FOLDER_CREATE, create_folder_done,
				 CORBA_Object_duplicate (listener, &ev), mail_thread_new);
	} else {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener, GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE, &ev);
	}

	CORBA_exception_free (&ev);
}

static void
remove_folder_done (char *uri, gboolean removed, void *data)
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
	       const char *type,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	if (!type_is_mail (type)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE, &ev);
		CORBA_exception_free (&ev);
		return;
	}
	
	mail_remove_folder (physical_uri, remove_folder_done, CORBA_Object_duplicate (listener, &ev));
	CORBA_exception_free (&ev);
}

typedef struct _xfer_folder_data {
	GNOME_Evolution_ShellComponentListener listener;
	gboolean remove_source;
	char *source_uri;
} xfer_folder_data;

static void
xfer_folder_done (gboolean ok, void *data)
{
	xfer_folder_data *xfd = (xfer_folder_data *)data;
	GNOME_Evolution_ShellComponentListener listener = xfd->listener;
	GNOME_Evolution_ShellComponentListener_Result result;
	CORBA_Environment ev;

	if (xfd->remove_source && ok) {
		mail_remove_folder (xfd->source_uri, remove_folder_done, xfd->listener);
	} else {
		if (ok)
			result = GNOME_Evolution_ShellComponentListener_OK;
		else
			result = GNOME_Evolution_ShellComponentListener_INVALID_URI;
	
		CORBA_exception_init (&ev);
		GNOME_Evolution_ShellComponentListener_notifyResult (listener, result, &ev);
		CORBA_Object_release (listener, &ev);
		CORBA_exception_free (&ev);
	}

	g_free (xfd->source_uri);
	g_free (xfd);
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
	CamelFolder *source;
	CamelException ex;
	GPtrArray *uids;
	CamelURL *src, *dst;

	d(printf("Renaming folder '%s' to dest '%s' type '%s'\n", source_physical_uri, destination_physical_uri, type));

	CORBA_exception_init (&ev);

	if (!type_is_mail (type)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE, &ev);
		return;
	}

	src = camel_url_new(source_physical_uri, NULL);
	if (src == NULL) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener, GNOME_Evolution_ShellComponentListener_INVALID_URI, &ev);
		return;
	}

	dst = camel_url_new(destination_physical_uri, NULL);
	if (dst == NULL) {
		camel_url_free(src);
		GNOME_Evolution_ShellComponentListener_notifyResult (listener, GNOME_Evolution_ShellComponentListener_INVALID_URI, &ev);
		return;
	}

	if (camel_url_get_param(dst, "noselect") != NULL) {
		camel_url_free(src);
		camel_url_free(dst);
		GNOME_Evolution_ShellComponentListener_notifyResult (listener, 
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_OPERATION, &ev);
		return;
	}
	
	camel_exception_init (&ex);

	/* If we are really doing a rename, implement it as a rename */
	if (remove && strcmp(src->protocol, dst->protocol) == 0) {
		char *sname, *dname;
		CamelStore *store;
		
		if (src->fragment)
			sname = src->fragment;
		else {
			if (src->path && *src->path)
				sname = src->path+1;
			else
				sname = "";
		}
		
		if (dst->fragment)
			dname = dst->fragment;
		else {
			if (dst->path && *dst->path)
				dname = dst->path+1;
			else
				dname = "";
		}
		
		store = camel_session_get_store(session, source_physical_uri, &ex);
		if (store != NULL)
			camel_store_rename_folder(store, sname, dname, &ex);
		
		if (camel_exception_is_set(&ex))
			GNOME_Evolution_ShellComponentListener_notifyResult (listener, GNOME_Evolution_ShellComponentListener_INVALID_URI, &ev);
		else {
			/* Since the shell doesn't play nice with local folders, we have to do this manually */
			mail_vfolder_rename_uri(store, source_physical_uri, destination_physical_uri);
			mail_filter_rename_uri(store, source_physical_uri, destination_physical_uri);
			GNOME_Evolution_ShellComponentListener_notifyResult (listener, GNOME_Evolution_ShellComponentListener_OK, &ev);
		}
		camel_object_unref((CamelObject *)store);
	} else {
		source = mail_tool_uri_to_folder (source_physical_uri, 0, &ex);
		
		if (source) {
			xfer_folder_data *xfd;
			
			xfd = g_new0 (xfer_folder_data, 1);
			xfd->remove_source = remove_source;
			xfd->source_uri = g_strdup (source_physical_uri);
			xfd->listener = CORBA_Object_duplicate (listener, &ev);
			
			uids = camel_folder_get_uids (source);
			mail_transfer_messages (source, uids, remove_source, destination_physical_uri, CAMEL_STORE_FOLDER_CREATE, xfer_folder_done, xfd);
			camel_object_unref (CAMEL_OBJECT (source));
		} else
			GNOME_Evolution_ShellComponentListener_notifyResult (listener, GNOME_Evolution_ShellComponentListener_INVALID_URI, &ev);
	}
	
	CORBA_exception_free (&ev);
	camel_exception_clear (&ex);
	
	camel_url_free(src);
	camel_url_free(dst);
}

static void
configure_folder_popup(BonoboUIComponent *component, void *user_data, const char *cname)
{
	char *uri = user_data;
	
	if (strncmp(uri, "vfolder:", 8) == 0)
		vfolder_edit_rule(uri);
	else {
		FolderBrowser *fb = folder_browser_factory_get_browser(uri);
		
		if (fb)
			configure_folder(component, fb, cname);
		else
			mail_local_reconfigure_folder(uri, NULL, NULL);
	}
}

static void
populate_folder_context_menu (EvolutionShellComponent *shell_component,
			      BonoboUIComponent *uic,
			      const char *physical_uri,
			      const char *type,
			      void *closure)
{
#ifdef TRANSLATORS_ONLY
	static char popup_xml_i18n[] = {N_("Properties..."), N_("Change this folder's properties")};
#endif
	static char popup_xml[] =
		"<menuitem name=\"ChangeFolderPropertiesPopUp\" verb=\"ChangeFolderPropertiesPopUp\""
		"          _label=\"Properties...\" _tip=\"Change this folder's properties\"/>";

	if (!type_is_mail (type))
		return;
	
	/* FIXME: handle other types */
	
	/* the unmatched test is a bit of a hack but it works */
	if ((strncmp(physical_uri, "vfolder:", 8) == 0
	     && strstr(physical_uri, "#" CAMEL_UNMATCHED_NAME) == NULL)
	    || strncmp(physical_uri, "file:", 5) == 0) {
		bonobo_ui_component_add_verb_full(uic, "ChangeFolderPropertiesPopUp",
						  g_cclosure_new(G_CALLBACK(configure_folder_popup),
								 g_strdup(physical_uri), (GClosureNotify)g_free));
		bonobo_ui_component_set_translate (uic, EVOLUTION_SHELL_COMPONENT_POPUP_PLACEHOLDER,  popup_xml, NULL);
	}
}

static void
unpopulate_folder_context_menu (EvolutionShellComponent *shell_component,
				BonoboUIComponent *uic,
				const char *physical_uri,
				const char *type,
				void *closure)
{
	if (!type_is_mail (type))
		return;
	
	/* FIXME: handle other types */
	
	/* the unmatched test is a bit of a hack but it works */
	if ((strncmp(physical_uri, "vfolder:", 8) == 0
	     && strstr(physical_uri, "#" CAMEL_UNMATCHED_NAME) == NULL)
	    || strncmp(physical_uri, "file:", 5) == 0) {
		bonobo_ui_component_rm (uic, EVOLUTION_SHELL_COMPONENT_POPUP_PLACEHOLDER "/ChangeFolderPropertiesPopUp", NULL);
	}
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
	d(printf ("should get dnd selection for %s\n", physical_uri));
	
	return NULL;
}

/* Destination side DnD */
static CORBA_boolean
destination_folder_handle_motion (EvolutionShellComponentDndDestinationFolder *folder,
				  const char *physical_uri,
				  const char *folder_type,
				  const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context *destination_context,
				  GNOME_Evolution_ShellComponentDnd_Action *suggested_action_return,
				  gpointer user_data)
{
	const char *noselect;
	CamelURL *url;
	
	url = camel_url_new (physical_uri, NULL);
	noselect = url ? camel_url_get_param (url, "noselect") : NULL;
	
	if (noselect && !strcasecmp (noselect, "yes"))
		/* uh, no way to say "illegal" */
		*suggested_action_return = GNOME_Evolution_ShellComponentDnd_ACTION_DEFAULT;
	else
		*suggested_action_return = GNOME_Evolution_ShellComponentDnd_ACTION_MOVE;
	
	if (url)
		camel_url_free (url);
	
	return TRUE;
}

static gboolean
message_rfc822_dnd (CamelFolder *dest, CamelStream *stream, CamelException *ex)
{
	CamelMimeParser *mp;
	gboolean handled = FALSE;
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, TRUE);
	camel_mime_parser_init_with_stream (mp, stream);
	
	while (camel_mime_parser_step (mp, 0, 0) == HSCAN_FROM) {
		CamelMessageInfo *info;
		CamelMimeMessage *msg;
		
		handled = TRUE;
		
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mp) == -1) {
			camel_object_unref (msg);
			handled = FALSE;
			break;
		}
		
		/* append the message to the folder... */
		info = g_new0 (CamelMessageInfo, 1);
		camel_folder_append_message (dest, msg, info, NULL, ex);
		camel_object_unref (msg);
		
		if (camel_exception_is_set (ex)) {
			handled = FALSE;
			break;
		}
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}
	
	camel_object_unref (mp);
	
	return handled;
}

static CORBA_boolean
destination_folder_handle_drop (EvolutionShellComponentDndDestinationFolder *dest_folder,
				const char *physical_uri,
				const char *folder_type,
				const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context *destination_context,
				const GNOME_Evolution_ShellComponentDnd_Action action,
				const GNOME_Evolution_ShellComponentDnd_Data *data,
				gpointer user_data)
{
	char *tmp, *url, **urls;
	gboolean retval = FALSE;
	const char *noselect;
	CamelFolder *folder;
	CamelStream *stream;
	CamelException ex;
	GPtrArray *uids;
	CamelURL *uri;
	int i, type, fd;
	
	if (action == GNOME_Evolution_ShellComponentDnd_ACTION_LINK)
		return FALSE; /* we can't create links */
	
	/* this means the drag was cancelled */
	if (!data->bytes._buffer || data->bytes._length == -1)
		return FALSE;
	
	uri = camel_url_new (physical_uri, NULL);
	noselect = uri ? camel_url_get_param (uri, "noselect") : NULL;
	if (noselect && !strcasecmp (noselect, "yes")) {
		camel_url_free (uri);
		return FALSE;
	}
	
	if (uri)
		camel_url_free (uri);
	
	d(printf ("in destination_folder_handle_drop (%s)\n", physical_uri));
	
	for (type = 0; accepted_dnd_types[type]; type++)
		if (!strcmp (destination_context->dndType, accepted_dnd_types[type]))
			break;
	
	camel_exception_init (&ex);
	
	/* if this is a local vtrash folder, then it's uri is vtrash:file:/ */
	if (type_is_vtrash (folder_type) && !strncmp (physical_uri, "file:", 5))
		physical_uri = "vtrash:file:/";
	
	switch (type) {
	case ACCEPTED_DND_TYPE_TEXT_URI_LIST:
		folder = mail_tool_uri_to_folder (physical_uri, 0, &ex);
		if (!folder) {
			camel_exception_clear (&ex);
			return FALSE;
		}
		
		tmp = g_strndup (data->bytes._buffer, data->bytes._length);
		urls = g_strsplit (tmp, "\n", 0);
		g_free (tmp);
		
		retval = TRUE;
		for (i = 0; urls[i] != NULL && retval; i++) {
			/* get the path component */
			url = g_strstrip (urls[i]);
			
			uri = camel_url_new (url, NULL);
			g_free (url);

			if (!uri)
				continue;

			url = uri->path;
			uri->path = NULL;
			camel_url_free (uri);
			
			fd = open (url, O_RDONLY);
			if (fd == -1) {
				g_free (url);
				/* FIXME: okay, so what do we do in this case? */
				continue;
			}
			
			stream = camel_stream_fs_new_with_fd (fd);
			retval = message_rfc822_dnd (folder, stream, &ex);
			camel_object_unref (stream);
			camel_object_unref (folder);
			
			if (action == GNOME_Evolution_ShellComponentDnd_ACTION_MOVE && retval)
				unlink (url);
			
			g_free (url);
		}
		
		g_free (urls);
		break;
	case ACCEPTED_DND_TYPE_MESSAGE_RFC822:
		folder = mail_tool_uri_to_folder (physical_uri, 0, &ex);
		if (!folder) {
			camel_exception_clear (&ex);
			return FALSE;
		}
		
		/* write the message(s) out to a CamelStream so we can use it */
		stream = camel_stream_mem_new ();
		camel_stream_write (stream, data->bytes._buffer, data->bytes._length);
		camel_stream_reset (stream);
		
		retval = message_rfc822_dnd (folder, stream, &ex);
		camel_object_unref (stream);
		camel_object_unref (folder);
		break;
	case ACCEPTED_DND_TYPE_X_EVOLUTION_MESSAGE:
		folder = mail_tools_x_evolution_message_parse (data->bytes._buffer,
							       data->bytes._length,
							       &uids);
		
		if (!folder)
			return FALSE;
		
		mail_transfer_messages (folder, uids,
					action == GNOME_Evolution_ShellComponentDnd_ACTION_MOVE,
					physical_uri, 0, NULL, NULL);
		
		camel_object_unref (folder);
		break;
	default:
		break;
	}
	
	camel_exception_clear (&ex);
	
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
		if (standard_folders[i].folder) {
			CamelFolder *folder = *standard_folders[i].folder;
			
			*standard_folders[i].folder = NULL;
			
			if (CAMEL_OBJECT (folder)->ref_count == 1)
				d(printf ("About to finalise folder %s\n", folder->full_name));
			else
				d(printf ("Folder %s still has %d extra ref%s on it\n", folder->full_name,
					  CAMEL_OBJECT (folder)->ref_count - 1,
					  CAMEL_OBJECT (folder)->ref_count - 1 == 1 ? "" : "s"));
			
			camel_object_unref (CAMEL_OBJECT (folder));
		}
	}
}

static void
got_folder (char *uri, CamelFolder *folder, void *data)
{
	CamelFolder **fp = data;
	
	*fp = folder;
	
	if (folder) {
		camel_object_ref (CAMEL_OBJECT (folder));
		
		/* emit a changed event, this is a little hack so that the folderinfo cache
		   will update knowing whether this is the outbox_folder or not, etc */
		if (folder == outbox_folder) {
			CamelFolderChangeInfo *changes = camel_folder_change_info_new();
			
			camel_object_trigger_event((CamelObject *)folder, "folder_changed", changes);
			camel_folder_change_info_free(changes);
		}
	}
}

static void
shell_client_destroy (GtkObject *object)
{
	global_shell_client = NULL;
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	GNOME_Evolution_Shell corba_shell;
	EAccountList *accounts;
	int i;
	
	/* FIXME: should we ref this? */
	global_shell_client = shell_client;
	g_object_weak_ref ((GObject *) shell_client, (GWeakNotify) shell_client_destroy, NULL);
	
	evolution_dir = g_strdup (evolution_homedir);
	mail_session_init ();
	
	async_event = mail_async_event_new();
	
	storages_hash = g_hash_table_new (NULL, NULL);
	
	corba_shell = evolution_shell_client_corba_objref (shell_client);
	
	for (i = 0; i < sizeof (standard_folders) / sizeof (standard_folders[0]); i++)
		*standard_folders[i].uri = g_strdup_printf ("file://%s/local/%s", evolution_dir, standard_folders[i].name);
	
	vfolder_load_storage(corba_shell);
	
	accounts = mail_config_get_accounts ();
	mail_load_storages (corba_shell, accounts);
	
	mail_local_storage_startup (shell_client, evolution_dir);
	mail_importer_init (shell_client);
	
	for (i = 0; i < sizeof (standard_folders) / sizeof (standard_folders[0]); i++) {
		mail_msg_wait (mail_get_folder (*standard_folders[i].uri, CAMEL_STORE_FOLDER_CREATE,
						got_folder, standard_folders[i].folder, mail_thread_new));
	}
	
	mail_autoreceive_setup ();
	
	{
		/* setup the global quick-search context */
		char *user = g_strdup_printf ("%s/searches.xml", evolution_dir);
		char *system = g_strdup (EVOLUTION_PRIVDATADIR "/vfoldertypes.xml");
		
		search_context = rule_context_new ();
		g_object_set_data_full(G_OBJECT(search_context), "user", user, g_free);
		g_object_set_data_full(G_OBJECT(search_context), "system", system, g_free);
		
		rule_context_add_part_set (search_context, "partset", filter_part_get_type (),
					   rule_context_add_part, rule_context_next_part);
		
		rule_context_add_rule_set (search_context, "ruleset", filter_rule_get_type (),
					   rule_context_add_rule, rule_context_next_rule);
		
		rule_context_load (search_context, system, user);
	}
	
	if (mail_config_is_corrupt ()) {
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
						 _("Some of your mail settings seem corrupt, "
						   "please check that everything is in order."));
		g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
		gtk_widget_show (dialog);
	}
	
	/* Everything should be ready now */
	evolution_folder_info_notify_ready ();
}

static void
free_storage (gpointer service, gpointer storage, gpointer data)
{
	if (service) {
		mail_note_store_remove((CamelStore *)service);
		camel_service_disconnect (CAMEL_SERVICE (service), TRUE, NULL);
		camel_object_unref (CAMEL_OBJECT (service));
	}
	
	if (storage)
		bonobo_object_unref (BONOBO_OBJECT (storage));
}

static void
debug_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	extern gboolean camel_verbose_debug;
	
	camel_verbose_debug = 1;
}

static void
interactive_cb (EvolutionShellComponent *shell_component, gboolean on,
		gulong new_view_xid, gpointer user_data)
{
	mail_session_set_interactive (on);

	if (on)
		/* how do we get the parent window? */
		e_msg_composer_check_autosave(NULL);
}

static void
handle_external_uri_cb (EvolutionShellComponent *shell_component,
			const char *uri,
			void *data)
{
	if (strncmp (uri, "mailto:", 7) != 0) {
		/* FIXME: Exception?  The EvolutionShellComponent object should
		   give me a chance to do so, but currently it doesn't.  */
		g_warning ("Invalid URI requested to mail component -- %s", uri);
		return;
	}
	
	/* FIXME: Sigh.  This shouldn't be here.  But the code is messy, so
	   I'll just put it here anyway.  */
	send_to_url (uri, NULL);
}

static void
user_create_new_item_cb (EvolutionShellComponent *shell_component,
			 const char *id,
			 const char *parent_folder_physical_uri,
			 const char *parent_folder_type,
			 gpointer data)
{
	if (!strcmp (id, "message")) {
		send_to_url (NULL, parent_folder_physical_uri);
		return;
	} else if (!strcmp (id, "post")) {
		post_to_url (parent_folder_physical_uri);
		return;
	}
	
	g_warning ("Don't know how to create item of type \"%s\"", id);
}

static void owner_unset_cb (EvolutionShellComponent *shell_component, gpointer user_data);

/* Table for signal handler setup/cleanup */
static struct {
	char *sig;
	GCallback func;
	int hand;
} shell_component_handlers[] = {
	{ "owner_set", G_CALLBACK(owner_set_cb), },
	{ "owner_unset", G_CALLBACK(owner_unset_cb), },
	{ "debug", G_CALLBACK(debug_cb), },
	{ "interactive", G_CALLBACK(interactive_cb) },
	{ "destroy", G_CALLBACK(owner_unset_cb), },
	{ "handle_external_uri", G_CALLBACK(handle_external_uri_cb), },
	{ "user_create_new_item", G_CALLBACK(user_create_new_item_cb) }
};

static void
owner_unset_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	CORBA_Environment ev;
	GConfClient *gconf;
	int i;
	EIterator *it;
	
	gconf = mail_config_get_gconf_client ();
	
	for (i=0;i<sizeof(shell_component_handlers)/sizeof(shell_component_handlers[0]);i++)
		g_signal_handler_disconnect((GtkObject *)shell_component, shell_component_handlers[i].hand);
	
	if (gconf_client_get_bool (gconf, "/apps/evolution/mail/trash/empty_on_exit", NULL))
		empty_trash (NULL, NULL, NULL);
	
	unref_standard_folders ();
	mail_local_storage_shutdown ();
	mail_importer_uninit ();
	
	global_shell_client = NULL;
	mail_session_set_interactive (FALSE);
	
	g_object_unref (search_context);
	search_context = NULL;

	/* force de-activate of all controls, tho only one should be active anyway? */
	CORBA_exception_init(&ev);
	for (it = e_list_get_iterator(folder_browser_factory_get_control_list());
	     e_iterator_is_valid(it);
	     e_iterator_next(it)) {
		Bonobo_Control_activate(bonobo_object_corba_objref((BonoboObject *)e_iterator_get(it)),
					  FALSE, &ev);
	}
	CORBA_exception_free(&ev);

	for (i= 0;i<3;i++) {
		/* need to flush any outstanding tasks before proceeding */

		/* NOTE!!  This may cause a deadlock situation, if we were
		   called from a deeper main loop than the top level
		   - is there a way to detect this?
		   - is this a very big problem?
		   FIXME: should use semaphores or something to wait rather than polling */
		while (e_thread_busy(NULL) || mail_msg_active(-1)) {
			if (g_main_context_pending(NULL))
				g_main_context_iteration(NULL, TRUE);
			else
				usleep(100000);
		}

		switch(i) {
		case 0:
			mail_vfolder_shutdown();
			break;
		case 1:
			if (mail_async_event_destroy(async_event) == -1) {
				g_warning("Cannot destroy async event: would deadlock");
				g_warning(" system may be unstable at exit");
			}
			break;
		case 2:
			g_hash_table_foreach (storages_hash, free_storage, NULL);
			g_hash_table_destroy (storages_hash);
			storages_hash = NULL;
			break;
		}
	}
}

static void
send_receive_cb (EvolutionShellComponent *shell_component,
		 gboolean show_dialog,
		 void *data)
{
	EAccount *account;
	GtkWidget *dialog;

	/* FIXME: configure_mail() should be changed to work without a
	   FolderBrowser, and then we will be able to call configure_mail from
	   here properly.  */
	if (!mail_config_is_configured () /* && !configure_mail (fb) */)
		return;
	
	account = mail_config_get_default_account ();
	if (!account || !account->transport->url) {
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 _("You have not set a mail transport method"));
		g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
		gtk_widget_show (dialog);
	} else {
		dialog = mail_send_receive ();
		e_dialog_set_transient_for_xid((GtkWindow *)dialog, evolution_shell_component_get_parent_view_xid(shell_component));
	}
}

static gboolean
request_quit (EvolutionShellComponent *shell_component,
	      void *closure)
{
	GtkWidget *dialog;
	int resp;

	if (!e_msg_composer_request_close_all ())
		return FALSE;
	
	if (!outbox_folder || !camel_folder_get_message_count (outbox_folder))
		return TRUE;

	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO,
					_("You have unsent messages, do you wish to quit anyway?"));
	gtk_dialog_set_default_response((GtkDialog *)dialog, GTK_RESPONSE_NO);
	resp = gtk_dialog_run((GtkDialog *)dialog);
	gtk_widget_destroy(dialog);

	return resp == GTK_RESPONSE_YES;
}

static BonoboObject *
create_component (void)
{
	EvolutionShellComponentDndDestinationFolder *destination_interface;
	MailOfflineHandler *offline_handler;
	GdkPixbuf *icon;
	int i;
	
	shell_component = evolution_shell_component_new (folder_types,
							 schema_types,
							 create_view,
							 create_folder,
							 remove_folder,
							 xfer_folder,
							 populate_folder_context_menu,
							 unpopulate_folder_context_menu,
							 get_dnd_selection,
							 request_quit,
							 NULL);

	g_signal_connect((shell_component), "send_receive",
			    G_CALLBACK (send_receive_cb), NULL);
	
	destination_interface = evolution_shell_component_dnd_destination_folder_new (destination_folder_handle_motion,
										      destination_folder_handle_drop,
										      shell_component);
	
	bonobo_object_add_interface (BONOBO_OBJECT (shell_component),
				     BONOBO_OBJECT (destination_interface));
	
	icon = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/new-message.xpm", NULL);
	evolution_shell_component_add_user_creatable_item (shell_component, "message",
							   _("New Mail Message"), _("_Mail Message"),
							   _("Compose a new mail message"),
							   "mail", 'm',
							   icon);
	if (icon != NULL)
		g_object_unref (icon);

	icon = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/post-message-16.png", NULL);
	evolution_shell_component_add_user_creatable_item (shell_component, "post",
							   _("New Message Post"), _("_Post Message"),
							   _("Post a new mail message"),
							   "mail/public", 'p',
							   icon);
	if (icon != NULL)
		g_object_unref (icon);

	for (i=0;i<sizeof(shell_component_handlers)/sizeof(shell_component_handlers[0]);i++) {
		shell_component_handlers[i].hand = g_signal_connect((shell_component),
								      shell_component_handlers[i].sig,
								      shell_component_handlers[i].func, NULL);
	}
	
	offline_handler = mail_offline_handler_new ();
	bonobo_object_add_interface (BONOBO_OBJECT (shell_component), BONOBO_OBJECT (offline_handler));
	
	return BONOBO_OBJECT (shell_component);
}

static void
notify_listener (const Bonobo_Listener listener, 
		 GNOME_Evolution_Storage_Result corba_result)
{
	CORBA_any any;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	any._type = TC_GNOME_Evolution_Storage_Result;
	any._value = &corba_result;

	Bonobo_Listener_event (listener, "result", &any, &ev);

	CORBA_exception_free (&ev);
}

static void
notify_listener_exception(const Bonobo_Listener listener, CamelException *ex)
{
	GNOME_Evolution_Storage_Result result;
	
	switch(camel_exception_get_id(ex)) {
	case CAMEL_EXCEPTION_SERVICE_UNAVAILABLE:
		result = GNOME_Evolution_Storage_NOT_ONLINE;
		break;
	case CAMEL_EXCEPTION_NONE:
		result = GNOME_Evolution_Storage_OK;
		break;
	case CAMEL_EXCEPTION_FOLDER_INVALID_PATH:
	case CAMEL_EXCEPTION_SERVICE_URL_INVALID:
		result = GNOME_Evolution_Storage_INVALID_URI;
		break;
	default:
		result = GNOME_Evolution_Storage_GENERIC_ERROR;
		break;
	}

	notify_listener(listener, result);
}

static void
storage_create_folder (EvolutionStorage *storage,
		       const Bonobo_Listener listener,
		       const char *path,
		       const char *type,
		       const char *description,
		       const char *parent_physical_uri,
		       gpointer user_data)
{
	CamelStore *store = user_data;
	CamelFolderInfo *root, *fi;
	char *name;
	CamelURL *url;
	CamelException ex;
	
	/* We could just use 'path' always here? */

	if (!type_is_mail (type)) {
		notify_listener (listener, GNOME_Evolution_Storage_UNSUPPORTED_TYPE);
		return;
	}
	
	name = strrchr (path, '/');
	if (!name) {
		notify_listener (listener, GNOME_Evolution_Storage_INVALID_URI);
		return;
	}
	name++;

	/* we can not directly create folders on a vfolder store, so fudge it */
	if (CAMEL_IS_VEE_STORE(store)) {
		VfolderRule *rule;
		rule = vfolder_rule_new();

		filter_rule_set_name((FilterRule *)rule, path+1);
		vfolder_gui_add_rule(rule);
	} else {
		camel_exception_init (&ex);
		if (*parent_physical_uri) {
			url = camel_url_new (parent_physical_uri, NULL);
			if (!url) {
				notify_listener (listener, GNOME_Evolution_Storage_INVALID_URI);
				return;
			}
		
			root = camel_store_create_folder (store, url->fragment?url->fragment:url->path + 1, name, &ex);
			camel_url_free (url);
		} else
			root = camel_store_create_folder (store, NULL, name, &ex);

		if (camel_exception_is_set (&ex)) {
			notify_listener_exception(listener, &ex);
			camel_exception_clear (&ex);
			return;
		}
	
		if (camel_store_supports_subscriptions (store)) {
			for (fi = root; fi; fi = fi->child)
				camel_store_subscribe_folder (store, fi->full_name, NULL);
		}
	
		camel_store_free_folder_info (store, root);
	}

	notify_listener (listener, GNOME_Evolution_Storage_OK);
}

static void
storage_remove_folder_recursive (EvolutionStorage *storage, CamelStore *store, CamelFolderInfo *root, CamelException *ex)
{
	CamelFolderInfo *fi;
	
	/* delete all children */
	fi = root->child;
	while (fi && !camel_exception_is_set (ex)) {
		storage_remove_folder_recursive (storage, store, fi, ex);
		fi = fi->sibling;
	}
	
	if (!camel_exception_is_set (ex)) {
		if (camel_store_supports_subscriptions (store))
			camel_store_unsubscribe_folder (store, root->full_name, NULL);
		
		camel_store_delete_folder (store, root->full_name, ex);
		
		if (!camel_exception_is_set (ex))
			evolution_storage_removed_folder (storage, root->path);
	}
}

static void
storage_remove_folder (EvolutionStorage *storage,
		       const Bonobo_Listener listener,
		       const char *path,
		       const char *physical_uri,
		       gpointer user_data)
{
	CamelStore *store = user_data;
	CamelFolderInfo *root, *fi;
	CamelURL *url = NULL;
	CamelException ex;
	char *name;
	
	g_warning ("storage_remove_folder: path=\"%s\"; uri=\"%s\"", path, physical_uri);
	
	if (!path || !*path || !physical_uri || !strncmp (physical_uri, "vtrash:", 7)) {
		notify_listener (listener, GNOME_Evolution_Storage_INVALID_URI);
		return;
	}
	
	url = camel_url_new (physical_uri, NULL);
	if (!url) {
		notify_listener (listener, GNOME_Evolution_Storage_INVALID_URI);
		return;
	}
	
	if (url->fragment)
		name = url->fragment;
	else if (url->path && url->path[0])
		name = url->path+1;
	else
		name = "";
	
	camel_exception_init (&ex);
	
	root = camel_store_get_folder_info (store, name, CAMEL_STORE_FOLDER_INFO_FAST |
					    CAMEL_STORE_FOLDER_INFO_RECURSIVE, &ex);
	
	if (!root || camel_exception_is_set (&ex)) {
		notify_listener_exception (listener, &ex);
		camel_exception_clear (&ex);
		camel_url_free (url);
		return;
	}
	
	/* walk the tree until we find the particular child folder we want to delete */
	fi = root;
	while (fi) {
		if (!strcmp (fi->full_name, name))
			break;
		fi = fi->child;
	}
	
	camel_url_free (url);
	
	if (!fi) {
		notify_listener (listener, GNOME_Evolution_Storage_INVALID_URI);
		camel_store_free_folder_info (store, root);
		return;
	}
	
	storage_remove_folder_recursive (storage, store, fi, &ex);
	camel_store_free_folder_info (store, root);
	if (camel_exception_is_set (&ex)) {
		notify_listener_exception (listener, &ex);
		camel_exception_clear (&ex);
	} else {
		notify_listener (listener, GNOME_Evolution_Storage_OK);
	}
}

static void
storage_xfer_folder (EvolutionStorage *storage,
		     const Bonobo_Listener listener,
		     const char *source_path,
		     const char *destination_path,
		     gboolean remove_source,
		     CamelStore *store)
{
	CamelException ex;
	char *src, *dst;
	char sep;

	d(printf("Transfer folder on store source = '%s' dest = '%s'\n", source_path, destination_path));

	/* FIXME: this is totally not gonna work once we have namespaces */
	
	/* Remap the 'path' to the camel friendly name based on the store dir separator */
	sep = store->dir_sep;
	src = g_strdup(source_path[0]=='/'?source_path+1:source_path);
	dst = g_strdup(destination_path[0]=='/'?destination_path+1:destination_path);
	camel_exception_init (&ex);
	if (remove_source) {
		d(printf("trying to rename\n"));
		camel_store_rename_folder(store, src, dst, &ex);
		notify_listener_exception(listener, &ex);
	} else {
		d(printf("No remove, can't rename\n"));
		/* FIXME: Implement folder 'copy' for remote stores */
		/* This exception never goes anywhere, so it doesn't need translating or using */
		notify_listener (listener, GNOME_Evolution_Storage_UNSUPPORTED_OPERATION);
	}

	g_free(src);
	g_free(dst);

	camel_exception_clear (&ex);
}

static void
storage_connected (CamelStore *store, CamelFolderInfo *info, void *listener)
{
	notify_listener (listener, (info ? GNOME_Evolution_Storage_OK :
				    GNOME_Evolution_Storage_GENERIC_ERROR));
}

static void
storage_connect (EvolutionStorage *storage,
		 const Bonobo_Listener listener,
		 const char *path,
		 CamelStore *store)
{
	mail_note_store (CAMEL_STORE (store), NULL, storage, CORBA_OBJECT_NIL,
			 storage_connected, listener);
}

static void
add_storage (const char *name, const char *uri, CamelService *store,
	     GNOME_Evolution_Shell corba_shell, CamelException *ex)
{
	EvolutionStorage *storage;
	EvolutionStorageResult res;
	
	storage = evolution_storage_new (name, FALSE);
	g_signal_connect(storage, "open_folder", G_CALLBACK(storage_connect), store);
	g_signal_connect(storage, "create_folder", G_CALLBACK(storage_create_folder), store);
	g_signal_connect(storage, "remove_folder", G_CALLBACK(storage_remove_folder), store);
	g_signal_connect(storage, "xfer_folder", G_CALLBACK(storage_xfer_folder), store);
	
	res = evolution_storage_register_on_shell (storage, corba_shell);
	
	switch (res) {
	case EVOLUTION_STORAGE_OK:
		evolution_storage_has_subfolders (storage, "/", _("Connecting..."));
		mail_hash_storage (store, storage);
		/*if (auto_connect)*/
		mail_note_store ((CamelStore *) store, NULL, storage, CORBA_OBJECT_NIL, NULL, NULL);
		/* falllll */
	case EVOLUTION_STORAGE_ERROR_ALREADYREGISTERED:
	case EVOLUTION_STORAGE_ERROR_EXISTS:
		bonobo_object_unref (BONOBO_OBJECT (storage));
		return;
	default:
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot register storage with shell"));
		break;
	}
}

void
mail_add_storage (CamelStore *store, const char *name, const char *uri)
{
	EvolutionShellClient *shell_client;
	GNOME_Evolution_Shell shell;
	CamelException ex;
	
	g_return_if_fail (CAMEL_IS_STORE (store));
	
	shell_client = evolution_shell_component_get_owner (shell_component);
	shell = evolution_shell_client_corba_objref (shell_client);
	
	camel_exception_init (&ex);
	
	if (name == NULL) {
		char *service_name;
		
		service_name = camel_service_get_name ((CamelService *) store, TRUE);
		add_storage (service_name, uri, (CamelService *) store, shell, &ex);
		g_free (service_name);
	} else {
		add_storage (name, uri, (CamelService *) store, shell, &ex);
	}
	
	camel_exception_clear (&ex);
}

void
mail_load_storage_by_uri (GNOME_Evolution_Shell shell, const char *uri, const char *name)
{
	CamelException ex;
	CamelService *store;
	CamelProvider *prov;
	
	camel_exception_init (&ex);
	
	/* Load the service (don't connect!). Check its provider and
	 * see if this belongs in the shell's folder list. If so, add
	 * it.
	 */
	
	prov = camel_session_get_provider (session, uri, &ex);
	if (prov == NULL) {
		/* FIXME: real error dialog */
		g_warning ("couldn't get service %s: %s\n", uri,
			   camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
		return;
	}
	
	if (!(prov->flags & CAMEL_PROVIDER_IS_STORAGE) ||
	    (prov->flags & CAMEL_PROVIDER_IS_EXTERNAL))
		return;
	
	store = camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex);
	if (store == NULL) {
		/* FIXME: real error dialog */
		g_warning ("couldn't get service %s: %s\n", uri,
			   camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
		return;
	}
	
	if (name == NULL) {
		char *service_name;
		
		service_name = camel_service_get_name (store, TRUE);
		add_storage (service_name, uri, store, shell, &ex);
		g_free (service_name);
	} else
		add_storage (name, uri, store, shell, &ex);
	
	if (camel_exception_is_set (&ex)) {
		/* FIXME: real error dialog */
		g_warning ("Cannot load storage: %s",
			   camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
	}
	
	camel_object_unref (CAMEL_OBJECT (store));
}

void
mail_load_storages (GNOME_Evolution_Shell shell, EAccountList *accounts)
{
	CamelException ex;
	EIterator *iter;
	
	camel_exception_init (&ex);
	
	/* Load each service (don't connect!). Check its provider and
	 * see if this belongs in the shell's folder list. If so, add
	 * it.
	 */
	
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccountService *service;
		EAccount *account;
		const char *name;
		
		account = (EAccount *) e_iterator_get (iter);
		service = account->source;
		name = account->name;
		
		if (account->enabled && service->url != NULL)
			mail_load_storage_by_uri (shell, service->url, name);
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
}

void
mail_hash_storage (CamelService *store, EvolutionStorage *storage)
{
	camel_object_ref (CAMEL_OBJECT (store));
	g_hash_table_insert (storages_hash, store, storage);
}

EvolutionStorage *
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
		bonobo_object_ref (BONOBO_OBJECT (storage));
	
	return storage;
}

static void
store_disconnect(CamelStore *store, void *event_data, void *data)
{
	camel_service_disconnect (CAMEL_SERVICE (store), TRUE, NULL);
	camel_object_unref (CAMEL_OBJECT (store));
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
	if (!storage)
		return;
	
	g_hash_table_remove (storages_hash, store);
	
	/* so i guess potentially we could have a race, add a store while one
	   being removed.  ?? */
	mail_note_store_remove(store);
	
	shell_client = evolution_shell_component_get_owner (shell_component);
	corba_shell = evolution_shell_client_corba_objref(shell_client);
	
	evolution_storage_deregister_on_shell (storage, corba_shell);
	
	mail_async_event_emit(async_event, MAIL_ASYNC_THREAD, (MailAsyncFunc)store_disconnect, store, NULL, NULL);
}

void
mail_remove_storage_by_uri (const char *uri)
{
	CamelProvider *prov;
	CamelService *store;

	prov = camel_session_get_provider (session, uri, NULL);
	if (!prov)
		return;
	if (!(prov->flags & CAMEL_PROVIDER_IS_STORAGE) ||
	    (prov->flags & CAMEL_PROVIDER_IS_EXTERNAL))
		return;

	store = camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, NULL);
	if (store != NULL) {
		mail_remove_storage (CAMEL_STORE (store));
		camel_object_unref (CAMEL_OBJECT (store));
	}
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


#define FACTORY_ID "OAFIID:GNOME_Evolution_Mail_ControlFactory"

#define MAIL_CONFIG_IID "OAFIID:GNOME_Evolution_MailConfig"
#define WIZARD_IID "OAFIID:GNOME_Evolution_Mail_Wizard"
#define FOLDER_INFO_IID "OAFIID:GNOME_Evolution_FolderInfo"
#define COMPOSER_IID "OAFIID:GNOME_Evolution_Mail_Composer"

static BonoboObject *
factory (BonoboGenericFactory *factory,
	 const char *component_id,
	 void *closure)
{
	if (strcmp (component_id, COMPONENT_ID) == 0)
		return create_component();
	else if (strcmp(component_id, MAIL_CONFIG_IID) == 0)
		return (BonoboObject *)g_object_new (evolution_mail_config_get_type (), NULL);
	else if (strcmp(component_id, FOLDER_INFO_IID) == 0)
		return evolution_folder_info_new();
	else if (strcmp(component_id, WIZARD_IID) == 0)
		return evolution_mail_config_wizard_new();
	else if (strcmp (component_id, MAIL_ACCOUNTS_CONTROL_ID) == 0
		 || strcmp (component_id, MAIL_PREFERENCES_CONTROL_ID) == 0
		 || strcmp (component_id, MAIL_COMPOSER_PREFS_CONTROL_ID) == 0)
		return mail_config_control_factory_cb (factory, component_id, evolution_shell_client_corba_objref (global_shell_client));
	else if (strcmp(component_id, COMPOSER_IID) == 0)
		return (BonoboObject *)evolution_composer_new(composer_send_cb, composer_save_draft_cb);

	g_warning (FACTORY_ID ": Don't know what to do with %s", component_id);
	return NULL;
}

static Bonobo_Unknown
make_factory (PortableServer_POA poa, const char *iid, gpointer impl_ptr, CORBA_Environment *ev)
{
	static int init = 0;

	if (!init) {
		/* init ? */
		mail_config_init ();
		mail_msg_init ();
		init = 1;
	}

	return bonobo_shlib_factory_std (FACTORY_ID, poa, impl_ptr, factory, NULL, ev);
}

static BonoboActivationPluginObject plugin_list[] = {
	{FACTORY_ID, make_factory},
	{ NULL }
};
const  BonoboActivationPlugin Bonobo_Plugin_info = {
	plugin_list, "Evolution Mail component factory"
};

