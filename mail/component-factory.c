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

#include <bonobo.h>

#include "camel.h"

#include "Evolution.h"
#include "evolution-storage.h"

#include "folder-browser-factory.h"
#include "evolution-shell-component.h"
#include "folder-browser.h"
#include "mail.h"		/* YUCK FIXME */
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-local.h"
#include "mail-session.h"
#include "mail-mt.h"
#include "mail-importer.h"
#include "mail-vfolder.h"             /* vfolder_create_storage */
#include "openpgp-utils.h"
#include <gal/widgets/e-gui-utils.h>

#include "component-factory.h"

#include "mail-summary.h"
#include "mail-send-recv.h"

CamelFolder *drafts_folder = NULL;
CamelFolder *outbox_folder = NULL;
CamelFolder *sent_folder = NULL;     /* this one should be configurable? */
CamelFolder *trash_folder = NULL;
char *evolution_dir;

#define COMPONENT_FACTORY_ID "OAFIID:GNOME_Evolution_Mail_ShellComponentFactory"
#define SUMMARY_FACTORY_ID   "OAFIID:GNOME_Evolution_Mail_ExecutiveSummaryComponentFactory"

static BonoboGenericFactory *component_factory = NULL;
static BonoboGenericFactory *summary_factory = NULL;
static GHashTable *storages_hash;

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

	CORBA_exception_init(&ev);
	GNOME_Evolution_ShellComponentListener_notifyResult(listener, result, &ev);
	CORBA_Object_release(listener, &ev);
	CORBA_exception_free(&ev);
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

	CORBA_exception_init(&ev);
	if (!strcmp (type, "mail")) {
		uri = g_strdup_printf ("mbox://%s", physical_uri);
		mail_create_folder (uri, do_create_folder, CORBA_Object_duplicate (listener, &ev));
	} else {
		GNOME_Evolution_ShellComponentListener_notifyResult (
			listener, GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE, &ev);
	}
	CORBA_exception_free(&ev);
}

static struct {
	char *name;
	CamelFolder **folder;
} standard_folders[] = {
	{ "Drafts", &drafts_folder },
	{ "Outbox", &outbox_folder },
	{ "Sent", &sent_folder },
};

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
	
	openpgp_init (mail_config_get_pgp_path (), mail_config_get_pgp_type ());
	
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

	for (i = 0; i < sizeof (standard_folders) / sizeof (standard_folders[0]); i++) {
		char *uri = g_strdup_printf ("file://%s/local/%s", evolution_dir, standard_folders[i].name);
		mail_msg_wait (mail_get_folder (uri, got_folder, standard_folders[i].folder));
		g_free (uri);
	}
	
	/*mail_msg_wait (mail_get_trash ("file:/", got_folder, &trash_folder));*/
	mail_do_setup_trash (_("Trash"), "file:/", &trash_folder);
	mail_operation_wait_for_finish ();
	
	mail_session_enable_interaction (TRUE);
	
	mail_autoreceive_setup ();
}

static void
free_storage (gpointer service, gpointer storage, gpointer data)
{
	camel_service_disconnect (service, TRUE, NULL);
	camel_object_unref (service);
	bonobo_object_unref (storage);
}

static gboolean
idle_quit (gpointer user_data)
{
	if (e_list_length (folder_browser_factory_get_control_list ()))
		return TRUE;

	bonobo_object_unref (BONOBO_OBJECT (summary_factory));
	bonobo_object_unref (BONOBO_OBJECT (component_factory));
	g_hash_table_foreach (storages_hash, free_storage, NULL);
	g_hash_table_destroy (storages_hash);

	mail_operations_terminate ();
	gtk_main_quit ();

	return FALSE;
}	

static void
owner_unset_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	g_idle_add_full (G_PRIORITY_LOW, idle_quit, NULL, NULL);
}

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "mail", "evolution-inbox.png" },
	{ "mailstorage", "evolution-inbox.png" },
	{ NULL, NULL }
};

static BonoboObject *
component_fn (BonoboGenericFactory *factory, void *closure)
{
	EvolutionShellComponent *shell_component;

	shell_component = evolution_shell_component_new (folder_types,
							 create_view,
							 create_folder,
							 NULL, /* remove_folder_fn */
							 NULL, /* copy_folder_fn */
							 NULL, /* populate_folder_context_menu */
							 NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "destroy",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}

static BonoboObject *
summary_fn (BonoboGenericFactory *factory, void *closure)
{
	return executive_summary_component_factory_new (create_summary_view, 
							NULL);
}

void
component_factory_init (void)
{
	component_factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID,
							component_fn, NULL);
	summary_factory = bonobo_generic_factory_new (SUMMARY_FACTORY_ID,
						      summary_fn, NULL);
	mail_importer_init ();

	if (component_factory == NULL || summary_factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's mail component."));
		exit (1);
	}
}

static int
storage_create_folder (EvolutionStorage *storage, const char *path,
		       const char *type, const char *description,
		       const char *parent_physical_uri, gpointer user_data)
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
		if (!url)
			return EVOLUTION_STORAGE_ERROR_INVALID_URI;

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
		
		if (is_account_data) {
			account = iter->data;
			service = account->source;
		} else {
			service = iter->data;
		}
		
		if (service->url == NULL || service->url[0] == '\0')
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

		prov = camel_service_get_provider (store);

		/* FIXME: this case is ambiguous for things like the
		 * mbox provider, which can really be a spool
		 * (/var/spool/mail/user) or a storage (~/mail/, eg).
		 * That issue can't be resolved on the provider level
		 * -- it's a per-URL problem.
		 */
		if (prov->flags & CAMEL_PROVIDER_IS_STORAGE && prov->flags & CAMEL_PROVIDER_IS_REMOTE) {
			char *name;
			
			if (is_account_data) {
				name = g_strdup (account->name);
			} else {
				name = camel_service_get_name (store, TRUE);
			}
			add_storage (name, service->url, store, shell, &ex);
			g_free (name);
			
			if (camel_exception_is_set (&ex)) {
				/* FIXME: real error dialog */
				g_warning ("Cannot load storage: %s",
					   camel_exception_get_description (&ex));
				camel_exception_clear (&ex);
			}
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
