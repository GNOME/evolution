/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author:
 *   Bertrand Guiheneuf (bg@aful.org)
 * 
 * Dumped from bonobo/bonobo-persist-stream.c
 * 
 * (C) 2000 Helix Code, Inc.
 */

/* 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include "evolution-service-repository.h"

/* Parent class */
static BonoboObjectClass *evolution_service_repository_parent_class;


/* CORBA implementation Entry Point Vector */
POA_Evolution_ServiceRepository__vepv evolution_service_repository_vepv;





/* 
 *function assigned to the 
 * Evolution::ServiceRepository::set_shell
 * method. 
 * This function calls the set_shell_fn in 
 * the EvolutionServiceRepository
 */
static void
impl_set_shell (PortableServer_Servant servant,
		Evolution_Shell shell,
		CORBA_Environment *ev)
{
	BonoboObject *object = bonobo_object_from_servant (servant);
	EvolutionServiceRepository *sr = EVOLUTION_SERVICE_REPOSITORY (object);
	
	if (sr->set_shell_fn != NULL) {
		(*sr->set_shell_fn)(sr, shell, sr->closure);
	} else {
		GtkObjectClass *oc = GTK_OBJECT (sr)->klass;
		EvolutionServiceRepositoryClass *class = EVOLUTION_SERVICE_REPOSITORY_CLASS (oc);
		(*class->set_shell)(sr, shell);
	}
	
}

/**
 * evolution_service_repository_get_epv:
 * create and initialize the ServiceRepository
 * epv.
 */
POA_Evolution_ServiceRepository__epv *
evolution_service_repository_get_epv (void)
{
	POA_Evolution_ServiceRepository__epv *epv;

	epv = g_new0 (POA_Evolution_ServiceRepository__epv, 1);

	epv->set_shell        = impl_set_shell;

	return epv;
}


/* create the Evolution_ServiceRepository vepv */
static void
init_service_repository_corba_class (void)
{
	/* create the Bonobo interface epv */
	evolution_service_repository_vepv.Bonobo_Unknown_epv = 
		bonobo_object_get_epv ();
	
	/* create the ServiceRepository interface epv.
	 * Defined above */
	evolution_service_repository_vepv.Evolution_ServiceRepository_epv = 
		evolution_service_repository_get_epv ();
}


/* default implementation for the ::set_shell method */
static void
evolution_service_repository_set_shell_default (EvolutionServiceRepository *service_repository,
						Evolution_Shell shell)
{
	/* do nothing */
}



static void
evolution_service_repository_destroy (GtkObject *object)
{
	GTK_OBJECT_CLASS (evolution_service_repository_parent_class)->destroy (object);
}


/* initialize the Gtk object class */
static void
evolution_service_repository_class_init (EvolutionServiceRepositoryClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	evolution_service_repository_parent_class = gtk_type_class (bonobo_object_get_type ());

	/*
	 * Override and initialize methods
	 */
	object_class->destroy = evolution_service_repository_destroy;
	klass->set_shell = evolution_service_repository_set_shell_default;

	/* create the corba class */
	init_service_repository_corba_class ();
}

static void
evolution_service_repository_init (EvolutionServiceRepository *service_repository)
{
}


/**
 * evolution_service_repository_get_type:
 *
 * Returns: the GtkType for the EvolutionServiceRepository class.
 */
GtkType
evolution_service_repository_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EvolutionServiceRepository",
			sizeof (EvolutionServiceRepository),
			sizeof (EvolutionServiceRepositoryClass),
			(GtkClassInitFunc) evolution_service_repository_class_init,
			(GtkObjectInitFunc) evolution_service_repository_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_object_get_type (), &info);
	}

	return type;
}




/**
 * evolution_service_repository_construct: construct the object
 * @sr: the gtk service repository object to construct
 * @corba_service_repository: the corresponding corba object
 * @set_shell_fn: the ::set_shell implementation for this object 
 * @closure: data to pass to the set_shell operation
 * 
 * This construct an EvolutionServiceRepository object. 
 * The caller can give the function that will implement
 * the Corba interface. If those methods are %NULL then the
 * default class method will be called instead. 
 * 
 * Return value: The constructed service repository.
 **/
EvolutionServiceRepository *
evolution_service_repository_construct (EvolutionServiceRepository *sr,
					Evolution_ServiceRepository corba_service_repository,
					EvolutionServiceRepositorySetShellFn set_shell_fn,
					void *closure)
{
	g_return_val_if_fail (sr != NULL, NULL);
	g_return_val_if_fail (EVOLUTION_IS_SERVICE_REPOSITORY (sr), NULL);
	g_return_val_if_fail (corba_service_repository != CORBA_OBJECT_NIL, NULL);
	
	bonobo_object_construct (BONOBO_OBJECT (sr), 
				 corba_service_repository);
	
	sr->closure            = closure;
	sr->set_shell_fn       = set_shell_fn;

	return sr;
}


/* 
 * construct the corba 
 * Evolution_ServiceRepository object 
 */
static Evolution_ServiceRepository
create_evolution_service_repository (BonoboObject *object)
{
	POA_Evolution_ServiceRepository *servant;
	CORBA_Environment ev;

	/* create a servant */
	servant = (POA_Evolution_ServiceRepository *) g_new0 (BonoboObjectServant, 1);

	/* initialize its virtual entry point vector */
	servant->vepv = &evolution_service_repository_vepv;

	CORBA_exception_init (&ev);

	/* initialise the servant */
	POA_Evolution_ServiceRepository__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}
	CORBA_exception_free (&ev);

	/* activate it and return */
	return (Evolution_ServiceRepository) bonobo_object_activate_servant (object, servant);
}





/**
 * evolution_service_repository_new: create a new EvolutionServiceRepository object
 * @set_shell_fn: The ::set_shell method 
 * @closure: The data passed to the ::set_shell method
 * 
 * Create a full EvolutionServiceRepository. Also create the correspoding
 * servant. The ::set_shell method calls set_shell_fn unless set_shell_fn
 * is %NULL, in which case, the class default method is called.
 * 
 * Return value: The newly created EvolutionServiceRepository object
 **/
EvolutionServiceRepository *
evolution_service_repository_new (EvolutionServiceRepositorySetShellFn set_shell_fn,
				  void *closure)
{
	Evolution_ServiceRepository corba_sr;
	EvolutionServiceRepository *sr;
	
	/* create the gtk object */
	sr = gtk_type_new (evolution_service_repository_get_type ());

	/* create the Corba object */
	corba_sr = create_evolution_service_repository (
		BONOBO_OBJECT (sr));

	/* check for an error in the creation of the corba object */
	if (corba_sr == CORBA_OBJECT_NIL){
		gtk_object_destroy (GTK_OBJECT (sr));
		return NULL;
	}

	/* construct the object */
	evolution_service_repository_construct (sr, corba_sr, set_shell_fn, closure);

	return sr;
}
