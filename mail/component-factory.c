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
#include "e-util/e-gui-utils.h"

#include "component-factory.h"

CamelFolder *drafts_folder = NULL;
CamelFolder *outbox_folder = NULL;
CamelFolder *sent_folder = NULL;     /* this one should be configurable? */
char *evolution_dir;

static void create_vfolder_storage (EvolutionShellComponent *shell_component);

#define COMPONENT_FACTORY_ID "OAFIID:evolution-shell-component-factory:evolution-mail:0ea887d5-622b-4b8c-b525-18aa1cbe18a6"

static BonoboGenericFactory *factory = NULL;
static gint running_objects = 0;

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
	Evolution_Shell corba_shell;
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
	       const Evolution_ShellComponentListener listener,
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
	Evolution_Shell corba_shell;

	g_print ("evolution-mail: Yeeeh! We have an owner!\n");	/* FIXME */
	
	evolution_dir = g_strdup (evolution_homedir);
	
	mail_config_init ();
	mail_do_setup_folder ("Drafts", &drafts_folder);
	mail_do_setup_folder ("Outbox", &outbox_folder);
	mail_do_setup_folder ("Sent Messages", &sent_folder);
	/* Don't proceed until those _folder variables are valid. */
	mail_operation_wait_for_finish ();

	create_vfolder_storage (shell_component);

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	sources = mail_config_get_sources ();
	mail_load_storages (corba_shell, sources);
	sources = mail_config_get_news ();
	mail_load_storages (corba_shell, sources);
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	mail_operations_terminate ();
	gtk_main_quit ();
}

static void
factory_destroy (BonoboEmbeddable *embeddable,
		 gpointer          dummy)
{
	running_objects--;
	if (running_objects > 0)
		return;

	if (factory)
		bonobo_object_unref (BONOBO_OBJECT (factory));
	else
		g_warning ("Serious ref counting error");
	factory = NULL;

	gtk_main_quit ();
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
			    GTK_SIGNAL_FUNC (factory_destroy), NULL);
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

	if (factory == NULL) {
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

void 
mail_load_storages (Evolution_Shell corba_shell, GSList *sources)
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
		CamelService *temp;
		CamelProvider *prov = NULL;

		svc = (MailConfigService *) iter->data;
		if (svc->url == NULL || svc->url[0] == '\0')
			continue;

		temp = camel_session_get_service (session, svc->url, 
						  CAMEL_PROVIDER_STORE, &ex);
		if (temp == NULL) {
			/* FIXME: real error dialog */

			g_warning ("couldn't get service %s: %s\n",
				   svc->url, camel_exception_get_description (&ex));
			continue;
		}

		prov = camel_service_get_provider (temp);

		/* FIXME: this case is ambiguous for things like the mbox provider,
		 * which can really be a spool (/var/spool/mail/user) or a storage
		 * (~/mail/, eg). That issue can't be resolved on the provider
		 * level -- it's a per-URL problem.
		 */

		if (prov->flags & CAMEL_PROVIDER_IS_STORAGE && prov->flags & CAMEL_PROVIDER_IS_REMOTE) {
			mail_add_new_storage (svc->url, corba_shell, &ex);

			if (camel_exception_is_set (&ex)) {
				/* FIXME: real error dialog */
				g_warning ("Cannot load storage: %s",
					   camel_exception_get_description (&ex));
			}
		}

		camel_object_unref (CAMEL_OBJECT (temp));
	}
}

void
mail_add_new_storage (const char *uri, Evolution_Shell corba_shell, CamelException *ex)
{
	EvolutionStorage *storage;
	EvolutionStorageResult res;
	CamelURL *url;

	g_return_if_fail (uri && uri[0] != '\0');
	
	url = camel_url_new (uri, ex);
	if (url == NULL)
		return;

	if (url->host == NULL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Bad storage URL (no server): %s",
				      uri);
		return;
	}

	storage = evolution_storage_new (url->host);
	camel_url_free (url);

	res = evolution_storage_register_on_shell (storage, corba_shell);

	switch (res) {
	case EVOLUTION_STORAGE_OK:
		mail_do_scan_subfolders (uri, storage);
		/* falllll */
	case EVOLUTION_STORAGE_ERROR_ALREADYREGISTERED:
	case EVOLUTION_STORAGE_ERROR_EXISTS:
		return;
	default:
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     "mail_tool_add_new_storage: Cannot register storage on shell");
		break;
	}
}
