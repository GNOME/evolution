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

#define COMPONENT_FACTORY_ID "OAFIID:evolution-shell-component-factory:evolution-mail:0ea887d5-622b-4b8c-b525-18aa1cbe18a6"
#define SUMMARY_FACTORY_ID "OAFIID:evolution-executive-summary-component-factory:evolution-mail:be210cba-0eee-4def-84fa-643d50321217"

static BonoboGenericFactory *factory = NULL;
static BonoboGenericFactory *summary_factory = NULL;
static gint running_objects = 0;
static GHashTable *storages_hash;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "mail", "evolution-inbox.png" },
	{ NULL, NULL }
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
	GtkWidget *folder_browser_widget;

	if (g_strcasecmp (folder_type, "mail") != 0)
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;

	shell_client = evolution_shell_component_get_owner (shell_component);
	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	control = folder_browser_factory_new_control (physical_uri, corba_shell);
	if (!control)
		return EVOLUTION_SHELL_COMPONENT_NOTFOUND;

	folder_browser_widget = bonobo_control_get_widget (control);

	g_assert (folder_browser_widget != NULL);
	g_assert (IS_FOLDER_BROWSER (folder_browser_widget));

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
owner_unset_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	mail_operations_terminate ();
	gtk_main_quit ();
}

static void
free_storage (gpointer service, gpointer storage, gpointer data)
{
	camel_service_disconnect (service, TRUE, NULL);
	camel_object_unref (service);
	gtk_object_unref (storage);
}

static void
factory_destroy (BonoboEmbeddable *embeddable,
		 BonoboObject     *destroy_factory)
{
	running_objects--;
	if (running_objects > 0)
		return;

	if (destroy_factory)
		bonobo_object_unref (BONOBO_OBJECT (destroy_factory));
	else
		g_warning ("Serious ref counting error");
	destroy_factory = NULL;

	g_hash_table_foreach (storages_hash, free_storage, NULL);
	g_hash_table_destroy (storages_hash);
	storages_hash = NULL;

	gtk_main_quit ();
}

static BonoboObject *
summary_fn (BonoboGenericFactory *factory, void *closure)
{
	BonoboObject *summary_component_factory;

	running_objects++;

	summary_component_factory = executive_summary_component_factory_new (create_summary_view, 
									     NULL);
	gtk_signal_connect (GTK_OBJECT (summary_component_factory), "destroy",
			    GTK_SIGNAL_FUNC (factory_destroy), summary_factory);

	return summary_component_factory;
}

static BonoboObject *
factory_fn (BonoboGenericFactory *factory, void *closure)
{
	EvolutionShellComponent *shell_component;

	running_objects++;

	shell_component = evolution_shell_component_new (folder_types,
							 create_view,
							 create_folder,
							 NULL,
							 NULL,
							 NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "destroy",
			    GTK_SIGNAL_FUNC (factory_destroy), factory);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}

void
component_factory_init (void)
{
	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID, factory_fn, NULL);
	summary_factory = bonobo_generic_factory_new (SUMMARY_FACTORY_ID, summary_fn, NULL);
	storages_hash = g_hash_table_new (NULL, NULL);

	if (factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's mail component."));
		exit (1);
	}

	if (summary_factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's mail summary component."));
	}

	if (storages_hash == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's mail storage hash."));
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
	storage = evolution_storage_new (name);
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
