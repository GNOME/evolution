/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include "evolution-service-repository.h"

/* Parent class */
static BonoboObjectClass *evolution_service_repository_parent_class;

/**
 * evolution_service_repository_get_epv:
 */
POA_Evolution_ServiceRepository__epv *
evolution_service_repository_get_epv (void)
{
	POA_Evolution_ServiceRepository__epv *epv;

	epv = g_new0 (POA_Evolution_ServiceRepository__epv, 1);

	return epv;
}

static void
init_service_repository_corba_class (void)
{
}

static void
evolution_service_repository_destroy (GtkObject *object)
{
	GTK_OBJECT_CLASS (evolution_service_repository_parent_class)->destroy (object);
}

static void
evolution_service_repository_class_init (EvolutionServiceRepositoryClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	evolution_service_repository_parent_class = gtk_type_class (bonobo_object_get_type ());

	/*
	 * Override and initialize methods
	 */
	object_class->destroy = evolution_service_repository_destroy;

	init_service_repository_corba_class ();
}

static void
evolution_service_repository_init (EvolutionServiceRepository *service_repository)
{
}

EvolutionServiceRepository *
evolution_service_repository_construct (EvolutionServiceRepository *service_repository,
			 Evolution_ServiceRepository corba_service_repository)
{
	g_return_val_if_fail (service_repository != NULL, NULL);
	g_return_val_if_fail (EVOLUTION_IS_SERVICE_REPOSITORY (service_repository), NULL);
	g_return_val_if_fail (corba_service_repository != CORBA_OBJECT_NIL, NULL);
	
	bonobo_object_construct (BONOBO_OBJECT (service_repository), corba_service_repository);

	return service_repository;
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


