/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer-factory.c
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
 *
 * Author: Iain Holmes  <iain@helixcode.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>
#include <gal/util/e-util.h>

#include "GNOME_Evolution_Importer.h"
#include "evolution-importer-factory.h"


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionImporterFactoryPrivate {
	EvolutionImporterFactorySupportFormatFn support_format_fn;
	EvolutionImporterFactoryLoadFileFn load_file_fn;

	void *closure;
};


static POA_GNOME_Evolution_ImporterFactory__vepv ImporterFactory_vepv;

static POA_GNOME_Evolution_ImporterFactory *
create_servant (void)
{
	POA_GNOME_Evolution_ImporterFactory *servant;
	CORBA_Environment ev;
	
	servant = (POA_GNOME_Evolution_ImporterFactory *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &ImporterFactory_vepv;
	
	CORBA_exception_init (&ev);
	
	POA_GNOME_Evolution_ImporterFactory__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	return servant;
}

static CORBA_boolean
impl_GNOME_Evolution_ImporterFactory_supportFormat (PortableServer_Servant servant,
						    const CORBA_char *filename,
						    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionImporterFactory *factory;
	EvolutionImporterFactoryPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	factory = EVOLUTION_IMPORTER_FACTORY (bonobo_object);
	priv = factory->private;

	if (priv->support_format_fn != NULL)
		return (priv->support_format_fn) (factory, filename, priv->closure);
	else
		return FALSE;
}

static GNOME_Evolution_Importer
impl_GNOME_Evolution_ImporterFactory_loadFile (PortableServer_Servant servant,
					       const CORBA_char *filename,
					       CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionImporterFactory *factory;
	EvolutionImporterFactoryPrivate *priv;
	EvolutionImporter *importer;
	GNOME_Evolution_Importer obj, obj_dup;

	bonobo_object = bonobo_object_from_servant (servant);
	factory = EVOLUTION_IMPORTER_FACTORY (bonobo_object);
	priv = factory->private;

	if (priv->load_file_fn != NULL) {
		importer = (priv->load_file_fn) (factory, filename, priv->closure);
		obj = bonobo_object_corba_objref (BONOBO_OBJECT (importer));
		obj_dup = CORBA_Object_duplicate (obj, ev);
		return obj_dup;
	} else {
		return CORBA_OBJECT_NIL;
	}
}

static void
destroy (GtkObject *object)
{
	EvolutionImporterFactory *factory;
	EvolutionImporterFactoryPrivate *priv;

	factory = EVOLUTION_IMPORTER_FACTORY (object);
	priv = factory->private;

	if (priv == NULL)
		return;

	g_free (priv);
	factory->private = NULL;

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
corba_class_init (void)
{
	POA_GNOME_Evolution_ImporterFactory__vepv *vepv;
	POA_GNOME_Evolution_ImporterFactory__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;
	
	epv = g_new0 (POA_GNOME_Evolution_ImporterFactory__epv, 1);
	epv->supportFormat = impl_GNOME_Evolution_ImporterFactory_supportFormat;
	epv->loadFile = impl_GNOME_Evolution_ImporterFactory_loadFile;

	vepv = &ImporterFactory_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_ImporterFactory_epv = epv;
}

static void
class_init (EvolutionImporterFactoryClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (PARENT_TYPE);
	corba_class_init ();
}

static void
init (EvolutionImporterFactory *factory)
{
	EvolutionImporterFactoryPrivate *priv;

	priv = g_new0 (EvolutionImporterFactoryPrivate, 1);

	factory->private = priv;
}


void
evolution_importer_factory_construct (EvolutionImporterFactory *factory,
				      GNOME_Evolution_ImporterFactory corba_object,
				      EvolutionImporterFactorySupportFormatFn support_format_fn,
				      EvolutionImporterFactoryLoadFileFn load_file_fn,
				      void *closure)
{
	EvolutionImporterFactoryPrivate *priv;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (EVOLUTION_IS_IMPORTER_FACTORY (factory));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	g_return_if_fail (support_format_fn != NULL);
	g_return_if_fail (load_file_fn != NULL);

	bonobo_object_construct (BONOBO_OBJECT (factory), corba_object);
	
	priv = factory->private;
	priv->support_format_fn = support_format_fn;
	priv->load_file_fn = load_file_fn;

	priv->closure = closure;
}

EvolutionImporterFactory *
evolution_importer_factory_new (EvolutionImporterFactorySupportFormatFn support_format_fn,
				EvolutionImporterFactoryLoadFileFn load_file_fn,
				void *closure)
{
	EvolutionImporterFactory *factory;
	POA_GNOME_Evolution_ImporterFactory *servant;
	GNOME_Evolution_ImporterFactory corba_object;

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	factory = gtk_type_new (evolution_importer_factory_get_type ());
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (factory),
						       servant);
	evolution_importer_factory_construct (factory, corba_object,
					      support_format_fn,
					      load_file_fn, closure);
	return factory;
}

E_MAKE_TYPE (evolution_importer_factory, "EvolutionImporterFactory",
	     EvolutionImporterFactory, class_init, init, PARENT_TYPE);
