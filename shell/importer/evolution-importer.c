/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer.c
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
 *
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-importer.h"

#include <bonobo/bonobo-object.h>
#include <gal/util/e-util.h>

#include "GNOME_Evolution_Importer.h"

#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionImporterPrivate {
	EvolutionImporterCreateControlFn create_control_fn;
	EvolutionImporterLoadFileFn load_file_fn;
	EvolutionImporterSupportFormatFn support_format_fn;
	EvolutionImporterProcessItemFn process_item_fn;
	EvolutionImporterGetErrorFn get_error_fn;

	void *closure;
};


static inline EvolutionImporter *
evolution_importer_from_servant (PortableServer_Servant servant)
{
	return EVOLUTION_IMPORTER (bonobo_object_from_servant (servant));
}

static void
impl_GNOME_Evolution_Importer_createControl (PortableServer_Servant servant,
					     Bonobo_Control *control,
					     CORBA_Environment *ev)
{
	EvolutionImporter *importer;
	EvolutionImporterPrivate *priv;

	importer = evolution_importer_from_servant (servant);
	priv = importer->priv;

	if (priv->create_control_fn != NULL)
		(priv->create_control_fn) (importer, control, priv->closure);
	else
		CORBA_exception_set_system (ev, ex_CORBA_NO_IMPLEMENT, CORBA_COMPLETED_NO);
}

static CORBA_boolean
impl_GNOME_Evolution_Importer_supportFormat (PortableServer_Servant servant,
					     const CORBA_char *filename,
					     CORBA_Environment *ev)
{
	EvolutionImporter *importer;
	EvolutionImporterPrivate *priv;

	importer = evolution_importer_from_servant (servant);
	priv = importer->priv;

	if (priv->support_format_fn != NULL)
		return (priv->support_format_fn) (importer, filename, 
						  priv->closure);
	else
		return FALSE;
}

static CORBA_boolean
impl_GNOME_Evolution_Importer_loadFile (PortableServer_Servant servant,
					const CORBA_char *filename,
					CORBA_Environment *ev)
{
	EvolutionImporter *importer;
	EvolutionImporterPrivate *priv;

	importer = evolution_importer_from_servant (servant);
	priv = importer->priv;

	if (priv->load_file_fn != NULL)
		return (priv->load_file_fn) (importer, filename, priv->closure);
	else
		return FALSE;
}

static void
impl_GNOME_Evolution_Importer_processItem (PortableServer_Servant servant,
					   GNOME_Evolution_ImporterListener listener,
					   CORBA_Environment *ev)
{
	EvolutionImporter *importer;
	EvolutionImporterPrivate *priv;

	importer = evolution_importer_from_servant (servant);
	priv = importer->priv;

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
	EvolutionImporter *importer;
	EvolutionImporterPrivate *priv;
	CORBA_char *out_str;

	importer = evolution_importer_from_servant (servant);
	priv = importer->priv;

	if (priv->get_error_fn != NULL) {
		out_str = (priv->get_error_fn) (importer, priv->closure);
		return CORBA_string_dup (out_str ? out_str : "");
	} else
		return CORBA_string_dup ("");
}


static void
finalise (GObject *object)
{
	EvolutionImporter *importer;
	EvolutionImporterPrivate *priv;

	importer = EVOLUTION_IMPORTER (object);
	priv = importer->priv;

	if (priv == NULL)
		return;

	g_free (priv);
	importer->priv = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
evolution_importer_class_init (EvolutionImporterClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_Importer__epv *epv = &klass->epv;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = finalise;

	parent_class = g_type_class_ref(PARENT_TYPE);
	epv->createControl = impl_GNOME_Evolution_Importer_createControl;
	epv->supportFormat = impl_GNOME_Evolution_Importer_supportFormat;
	epv->loadFile = impl_GNOME_Evolution_Importer_loadFile;
	epv->processItem = impl_GNOME_Evolution_Importer_processItem;
	epv->getError = impl_GNOME_Evolution_Importer_getError;
}

static void
evolution_importer_init (EvolutionImporter *importer)
{
	EvolutionImporterPrivate *priv;

	priv = g_new0 (EvolutionImporterPrivate, 1);

	importer->priv = priv;
}



static void
evolution_importer_construct (EvolutionImporter *importer,
			      EvolutionImporterCreateControlFn create_control_fn,
			      EvolutionImporterSupportFormatFn support_format_fn,
			      EvolutionImporterLoadFileFn load_file_fn,
			      EvolutionImporterProcessItemFn process_item_fn,
			      EvolutionImporterGetErrorFn get_error_fn,
			      void *closure)
{
	EvolutionImporterPrivate *priv;

	g_return_if_fail (importer != NULL);
	g_return_if_fail (EVOLUTION_IS_IMPORTER (importer));
	g_return_if_fail (support_format_fn != NULL);
	g_return_if_fail (load_file_fn != NULL);
	g_return_if_fail (process_item_fn != NULL);

	priv = importer->priv;
	priv->create_control_fn = create_control_fn;
	priv->support_format_fn = support_format_fn;
	priv->load_file_fn = load_file_fn;
	priv->process_item_fn = process_item_fn;
	priv->get_error_fn = get_error_fn;

	priv->closure = closure;
}

/**
 * evolution_importer_new:
 * @support_format_fn: The function to be called by the supportFormat method.
 * @load_file_fn: The function to be called by the loadFile method.
 * @process_item_fn: The function to be called by the processItem method.
 * @get_error_fn: The function to be called by the getError method.
 * @closure: The data to be passed to all of the above functions.
 *
 * Creates a new EvolutionImporter object. Of the parameters only 
 * @get_error_function and @closure may be #NULL.
 *
 * Returns: A newly created EvolutionImporter object.
 */
EvolutionImporter *
evolution_importer_new (EvolutionImporterCreateControlFn create_control_fn,
	                EvolutionImporterSupportFormatFn support_format_fn,
			EvolutionImporterLoadFileFn load_file_fn,
			EvolutionImporterProcessItemFn process_item_fn,
			EvolutionImporterGetErrorFn get_error_fn,
			void *closure)
{
	EvolutionImporter *importer;

	importer = g_object_new(evolution_importer_get_type (), NULL);
	evolution_importer_construct (importer, create_control_fn, support_format_fn, load_file_fn,
				      process_item_fn, get_error_fn, closure);
	return importer;
}

BONOBO_TYPE_FUNC_FULL (EvolutionImporter,
		       GNOME_Evolution_Importer,
		       PARENT_TYPE,
		       evolution_importer);
