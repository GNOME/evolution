/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer.c
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
#include "evolution-importer.h"


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionImporterPrivate {
	EvolutionImporterProcessItemFn process_item_fn;
	EvolutionImporterGetErrorFn get_error_fn;

	void *closure;
};


static POA_GNOME_Evolution_Importer__vepv Importer_vepv;

static POA_GNOME_Evolution_Importer *
create_servant (void)
{
	POA_GNOME_Evolution_Importer *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_Importer *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &Importer_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_Importer__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static void
impl_GNOME_Evolution_Importer_processItem (PortableServer_Servant servant,
					   GNOME_Evolution_ImporterListener listener,
					   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionImporter *importer;
	EvolutionImporterPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	importer = EVOLUTION_IMPORTER (bonobo_object);
	priv = importer->private;

	if (priv->process_item_fn != NULL)
		(priv->process_item_fn) (importer, listener, priv->closure, ev);
	else
		GNOME_Evolution_ImporterListener_notifyResult (listener,
							       GNOME_Evolution_ImporterListener_UNSUPPORTED_OPERATION, FALSE, ev);
}

static CORBA_char *
impl_GNOME_Evolution_Importer_getError (PortableServer_Servant servant,
					CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionImporter *importer;
	EvolutionImporterPrivate *priv;
	CORBA_char *out_str;

	bonobo_object = bonobo_object_from_servant (servant);
	importer = EVOLUTION_IMPORTER (bonobo_object);
	priv = importer->private;

	if (priv->get_error_fn != NULL) {
		out_str = (priv->get_error_fn) (importer, priv->closure);
		return CORBA_string_dup (out_str ? out_str : "");
	} else
		return CORBA_string_dup ("");
}


static void
destroy (GtkObject *object)
{
	EvolutionImporter *importer;
	EvolutionImporterPrivate *priv;

	importer = EVOLUTION_IMPORTER (object);
	priv = importer->private;

	if (priv == NULL)
		return;

	g_free (priv);
	importer->private = NULL;

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
corba_class_init (void)
{
	POA_GNOME_Evolution_Importer__vepv *vepv;
	POA_GNOME_Evolution_Importer__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	
	epv = g_new0 (POA_GNOME_Evolution_Importer__epv, 1);
	epv->processItem = impl_GNOME_Evolution_Importer_processItem;
	epv->getError = impl_GNOME_Evolution_Importer_getError;

	vepv = &Importer_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_Importer_epv = epv;
}

static void
class_init (EvolutionImporterClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (PARENT_TYPE);
	corba_class_init ();
}

static void
init (EvolutionImporter *importer)
{
	EvolutionImporterPrivate *priv;

	priv = g_new0 (EvolutionImporterPrivate, 1);

	importer->private = priv;
}



void
evolution_importer_construct (EvolutionImporter *importer,
			      GNOME_Evolution_Importer corba_object,
			      EvolutionImporterProcessItemFn process_item_fn,
			      EvolutionImporterGetErrorFn get_error_fn,
			      void *closure)
{
	EvolutionImporterPrivate *priv;

	g_return_if_fail (importer != NULL);
	g_return_if_fail (EVOLUTION_IS_IMPORTER (importer));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	g_return_if_fail (process_item_fn != NULL);
	g_return_if_fail (get_error_fn != NULL);

	bonobo_object_construct (BONOBO_OBJECT (importer), corba_object);

	priv = importer->private;
	priv->process_item_fn = process_item_fn;
	priv->get_error_fn = get_error_fn;

	priv->closure = closure;
}

EvolutionImporter *
evolution_importer_new (EvolutionImporterProcessItemFn process_item_fn,
			EvolutionImporterGetErrorFn get_error_fn,
			void *closure)
{
	EvolutionImporter *importer;
	POA_GNOME_Evolution_Importer *servant;
	GNOME_Evolution_Importer corba_object;

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	importer = gtk_type_new (evolution_importer_get_type ());
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (importer),
						       servant);
	evolution_importer_construct (importer, corba_object, process_item_fn,
				      get_error_fn, closure);
	return importer;
}

E_MAKE_TYPE (evolution_importer, "EvolutionImporter", EvolutionImporter,
	     class_init, init, PARENT_TYPE);
