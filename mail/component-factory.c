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
#include <gal/widgets/e-gui-utils.h>

#include "component-factory.h"

#include "mail-summary.h"

CamelFolder *drafts_folder = NULL;
CamelFolder *outbox_folder = NULL;
CamelFolder *sent_folder = NULL;     /* this one should be configurable? */
char *evolution_dir;

static void create_vfolder_storage (EvolutionShellComponent *shell_component);

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
			mail_do_scan_subfolders (store, storage);
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
create_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const char *type,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	mail_do_create_folder (listener, physical_uri, type);
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	GSList *sources;
	GNOME_Evolution_Shell corba_shell;

	g_print ("evolution-mail: Yeeeh! We have an owner!\n");	/* FIXME */
	
	evolution_dir = g_strdup (evolution_homedir);
	mail_session_init ();
	mail_config_init ();

	storages_hash = g_hash_table_new (NULL, NULL);

	create_vfolder_storage (shell_component);

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	sources = mail_config_get_sources ();
	mail_load_storages (corba_shell, sources);
	sources = mail_config_get_news ();
	mail_load_storages (corba_shell, sources);

	mail_local_storage_startup (shell_client, evolution_dir);

	mail_do_setup_folder ("Drafts", &drafts_folder);
	mail_do_setup_folder ("Outbox", &outbox_folder);
	mail_do_setup_folder ("Sent", &sent_folder);
	/* Don't proceed until those _folder variables are valid. */
	mail_operation_wait_for_finish ();
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

	shell_component = evolution_shell_component_new (
		folder_types, create_view, create_folder,
		NULL, NULL, NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
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

	if (component_factory == NULL || summary_factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's mail component."));
		exit (1);
	}
}

/* FIXME: remove */
static void
create_vfolder_storage (EvolutionShellComponent *shell_component)
{
	void vfolder_create_storage(EvolutionShellComponent *shell_component);

	vfolder_create_storage(shell_component);
}

static void
add_storage (const char *uri, CamelService *store,
	     GNOME_Evolution_Shell corba_shell, CamelException *ex)
{
	EvolutionStorage *storage;
	EvolutionStorageResult res;
	char *name;

	name = camel_service_get_name (store, TRUE);
	storage = evolution_storage_new (name, uri, "mailstorage");
	g_free (name);

	res = evolution_storage_register_on_shell (storage, corba_shell);

	switch (res) {
	case EVOLUTION_STORAGE_OK:
		g_hash_table_insert (storages_hash, store, storage);
		camel_object_ref (CAMEL_OBJECT (store));
		mail_do_scan_subfolders (CAMEL_STORE (store), storage);
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

void
mail_load_storages (GNOME_Evolution_Shell corba_shell, GSList *sources)
{
	CamelException ex;
	MailConfigService *svc;
	GSList *iter;

	camel_exception_init (&ex);	

	/* Load each service (don't connect!). Check its provider and
	 * see if this belongs in the shell's folder list. If so, add
	 * it.
	 */

	for (iter = sources; iter; iter = iter->next) {
		CamelService *store;
		CamelProvider *prov;

		svc = (MailConfigService *) iter->data;
		if (svc->url == NULL || svc->url[0] == '\0')
			continue;

		store = camel_session_get_service (session, svc->url, 
						   CAMEL_PROVIDER_STORE, &ex);
		if (store == NULL) {
			/* FIXME: real error dialog */
			g_warning ("couldn't get service %s: %s\n", svc->url,
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
		if (prov->flags & CAMEL_PROVIDER_IS_STORAGE &&
		    prov->flags & CAMEL_PROVIDER_IS_REMOTE) {
			add_storage (svc->url, store, corba_shell, &ex);
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
