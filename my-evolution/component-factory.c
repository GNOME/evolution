/*
 * component-factory.c: Component factory registration for Executive Summary.
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>

#include <shell/evolution-shell-component.h>
#include <shell/Evolution.h>

#include "e-summary-factory.h"
#include "e-summary-offline-handler.h"
#include "component-factory.h"
#include <gal/widgets/e-gui-utils.h>

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_Summary_ShellComponentFactory"

static gint running_objects = 0;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "summary", "evolution-today.png", N_("Summary"), N_("Folder containing the Evolution Summary"), FALSE, NULL, NULL },
	{ NULL, NULL }
};

static char *evolution_dir = NULL;

/* EvolutionShellComponent methods and signals */

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell,
	     const char *physical_uri,
	     const char *folder_type,
	     BonoboControl **control_return,
	     void *closure)
{
	EvolutionShellClient *shell_client;
	ESummaryOfflineHandler *offline_handler;
	GNOME_Evolution_Shell corba_shell;
	BonoboControl *control;


	g_print ("folder_type: %s\n", folder_type);
	if (g_strcasecmp (folder_type, "Summary") != 0) {
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;
	}

	offline_handler = gtk_object_get_data (GTK_OBJECT (shell), 
					       "offline-handler");
	shell_client = evolution_shell_component_get_owner (shell);
	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
	control = e_summary_factory_new_control (physical_uri, corba_shell,
						 offline_handler);
	if (!control)
		return EVOLUTION_SHELL_COMPONENT_NOTFOUND;

	*control_return = control;

	return EVOLUTION_SHELL_COMPONENT_OK;
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	if (evolution_dir != NULL) {
		evolution_dir = g_strdup (evolution_homedir);
	}
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component,
		gpointer user_data)
{
	gtk_main_quit ();
}

static void
component_destroy (BonoboObject *factory,
		 gpointer user_data)
{
	running_objects--;

	if (running_objects > 0) {
		return;
	}

	gtk_main_quit ();
}

static BonoboObject *
factory_fn (BonoboGenericFactory *factory,
	    void *closure)
{
	EvolutionShellComponent *shell_component;
	ESummaryOfflineHandler *offline_handler;

	running_objects++;

	shell_component = evolution_shell_component_new (folder_types,
							 NULL,
							 create_view,
							 NULL, NULL, 
							 NULL, NULL,
							 NULL, NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "destroy",
			    GTK_SIGNAL_FUNC (component_destroy), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);

	offline_handler = e_summary_offline_handler_new ();
	gtk_object_set_data (GTK_OBJECT (shell_component), "offline-handler",
			     offline_handler);
	bonobo_object_add_interface (BONOBO_OBJECT (shell_component), BONOBO_OBJECT (offline_handler));

	return BONOBO_OBJECT (shell_component);
}

void
component_factory_init (void)
{
	BonoboGenericFactory *object;

	object = bonobo_generic_factory_new (COMPONENT_FACTORY_IID,
					     factory_fn, NULL);

	if (object == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's Executive Summary component."));
		exit (1);
	}

	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (object));
}
