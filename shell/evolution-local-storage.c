/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-local-storage.c
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "e-util/e-util.h"

#include "evolution-local-storage.h"


#define PARENT_TYPE evolution_storage_get_type ()
static EvolutionStorageClass *parent_class = NULL;

struct _EvolutionLocalStoragePrivate {
	int dummy;
};


enum {
	SET_DISPLAY_NAME,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* CORBA interface implementation.  */

static POA_Evolution_LocalStorage__vepv LocalStorage_vepv;

static void
impl_Evolution_LocalStorage_set_display_name (PortableServer_Servant servant,
					      const CORBA_char *path,
					      const CORBA_char *display_name,
					      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionLocalStorage *local_storage;

	bonobo_object = bonobo_object_from_servant (servant);
	local_storage = EVOLUTION_LOCAL_STORAGE (bonobo_object);

	gtk_signal_emit (GTK_OBJECT (local_storage), signals[SET_DISPLAY_NAME], path, display_name);
}

static POA_Evolution_LocalStorage *
create_servant (void)
{
	POA_Evolution_LocalStorage *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_LocalStorage *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &LocalStorage_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_LocalStorage__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EvolutionLocalStorage *local_storage;
	EvolutionLocalStoragePrivate *priv;

	local_storage = EVOLUTION_LOCAL_STORAGE (object);
	priv = local_storage->priv;

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
corba_class_init (void)
{
	POA_Evolution_LocalStorage__vepv *vepv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	vepv = &LocalStorage_vepv;
	vepv->Bonobo_Unknown_epv         = bonobo_object_get_epv ();
	vepv->Evolution_Storage_epv      = evolution_storage_get_epv ();
	vepv->Evolution_LocalStorage_epv = evolution_local_storage_get_epv ();
}

static void
class_init (EvolutionLocalStorageClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = impl_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);

	signals[SET_DISPLAY_NAME] = gtk_signal_new ("set_display_name",
						    GTK_RUN_FIRST,
						    object_class->type,
						    GTK_SIGNAL_OFFSET (EvolutionLocalStorageClass,
								       set_display_name),
						    gtk_marshal_NONE__POINTER_POINTER,
						    GTK_TYPE_NONE, 2,
						    GTK_TYPE_STRING,
						    GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	corba_class_init ();
}

static void
init (EvolutionLocalStorage *local_storage)
{
	EvolutionLocalStoragePrivate *priv;

	priv = g_new (EvolutionLocalStoragePrivate, 1);

	local_storage->priv = priv;
}


POA_Evolution_LocalStorage__epv *
evolution_local_storage_get_epv (void)
{
	POA_Evolution_LocalStorage__epv *epv;

	epv = g_new0 (POA_Evolution_LocalStorage__epv, 1);
	epv->set_display_name = impl_Evolution_LocalStorage_set_display_name;

	return epv;
}

void
evolution_local_storage_construct (EvolutionLocalStorage *local_storage,
				   Evolution_LocalStorage corba_object,
				   const char *name)
{
	g_return_if_fail (local_storage != NULL);
	g_return_if_fail (EVOLUTION_IS_LOCAL_STORAGE (local_storage));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	g_return_if_fail (name != NULL);
	g_return_if_fail (name[0] != '\0');

	evolution_storage_construct (EVOLUTION_STORAGE (local_storage), corba_object, name);
}

EvolutionLocalStorage *
evolution_local_storage_new (const char *name)
{
	EvolutionLocalStorage *new;
	POA_Evolution_LocalStorage *servant;
	Evolution_LocalStorage corba_object;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (name[0] != '\0', NULL);

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	new = gtk_type_new (evolution_local_storage_get_type ());

	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (new), servant);
	evolution_local_storage_construct (new, corba_object, name);

	return new;
}


E_MAKE_TYPE (evolution_local_storage, "EvolutionLocalStorage", EvolutionLocalStorage,
	     class_init, init, PARENT_TYPE)
