/* GNOME calendar factory
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "cal-factory.h"



/* Private part of the CalFactory structure */
typedef struct {
	/* Hash table from canonized uris to loaded calendars */
	GHashTable *calendars;
} CalFactoryPrivate;



static void cal_factory_class_init (CalFactoryClass *class);
static void cal_factory_init (CalFactory *factory);
static void cal_factory_destroy (GtkObject *object);

static POA_GNOME_Calendar_CalFactory__vepv cal_factory_vepv;

static GnomeObjectClass *parent_class;



/**
 * cal_factory_get_type:
 * @void:
 *
 * Registers the #CalFactory class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalFactory class.
 **/
GtkType
cal_factory_get_type (void)
{
	static GtkType cal_factory_type = 0;

	if (!cal_factory_type) {
		static const GtkTypeInfo cal_factory_info = {
			"CalFactory",
			sizeof (CalFactory),
			sizeof (CalFactoryClass),
			(GtkClassInitFunc) cal_factory_class_init,
			(GtkObjectInitFunc) cal_factory_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_factory_type = gtk_type_unique (gnome_object_get_type (), &cal_factory_info);
	}

	return cal_factory_type;
}

/* CORBA class initialization function for the calendar factory */
static void
init_cal_factory_corba_class (void)
{
	cal_factory_vepv.GNOME_Unknown_epv = gnome_object_get_epv ();
	cal_factory_vepv.GNOME_Calendar_CalFactory_epv = cal_factory_get_epv ();
}

/* Class initialization function for the calendar factory */
static void
cal_factory_class_init (CalFactoryClass *class)
{
	GtkObjectClass *parent_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gnome_object_get_type ());

	object_class->destroy = cal_factory_destroy;

	init_cal_factory_corba_class ();
}

/* Object initialization function for the calendar factory */
static void
cal_factory_init (CalFactory *factory)
{
	CalFactoryPrivate *priv;

	priv = g_new0 (CalFactoryPrivate, 1);
	factory->priv = priv;

	priv->calendars = g_hash_table_new (g_str_hash, g_str_equal);
}

/* Destroy handler for the calendar */
static void
cal_factory_destroy (GtkObject *object)
{
	CalFactory *factory;
	CalFactoryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_FACTORY (object));

	factory = CAL_FACTORY (object);
	priv = factory->priv;

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* CORBA servant implementation */

/* CalFactory::load method */
static void
CalFactory_load (PortableServer_Servant servant,
		 CORBA_char *uri,
		 GNOME_Calendar_Listener listener,
		 CORBA_Environment *ev)
{
	CalFactory *factory;
	CalFactoryPrivate *priv;

	factory = CAL_FACTORY (gnome_object_from_servant (servant));
	priv = factory->priv;

	cal_factory_load (factory, uri, listener);
}

/* CalFactory::create method */
static GNOME_Calendar_Cal
CalFactory_create (PortableServer_Servant servant,
		   CORBA_char *uri,
		   CORBA_Environment *ev)
{
	CalFactory *factory;
	CalFactoryPrivate *priv;

	factory = CAL_FACTORY (gnome_object_from_servant (servant));
	priv = factory->priv;

	return cal_factory_create (factory, uri);
}

POA_GNOME_Calendar_CalFactory__epv *
cal_factory_get_epv (void)
{
	POA_GNOME_Calendar_CalFactory__epv *epv;

	epv = g_new0 (POA_GNOME_Calendar_CalFactory__epv, 1);
	epv->load = CalFactory_load;
	epv->create = CalFactory_create;

	return epv;
}



/* Returns whether a CORBA object is nil */
static gboolean
corba_object_is_nil (CORBA_Object object)
{
	CORBA_Environment ev;
	gboolean retval;

	CORBA_exception_init (&ev);
	retval = CORBA_Object_is_nil (object, &ev);
	CORBA_exception_free (&ev);

	return retval;
}

/**
 * cal_factory_construct:
 * @factory: A calendar factory.
 * @corba_factory: CORBA object for the calendar factory.
 *
 * Constructs a calendar factory by binding the corresponding CORBA object to
 * it.
 *
 * Return value: The same object as the @factory argument.
 **/
CalFactory *
cal_factory_construct (CalFactory *factory, GNOME_Calendar_CalFactory corba_factory)
{
	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (IS_CAL_FACTORY (factory), NULL);
	g_return_val_if_fail (!corba_object_is_nil (corba_factory), NULL);

	gnome_object_construct (GNOME_OBJECT (factory), corba_factory);
	return factory;
}

/**
 * cal_factory_corba_object_create:
 * @object: #GnomeObject that will wrap the CORBA object.
 *
 * Creates and activates the CORBA object that is wrapped by the specified
 * calendar factory @object.
 *
 * Return value: An activated object reference or #CORBA_OBJECT_NIL in case of
 * failure.
 **/
GNOME_Calendar_CalFactory
cal_factory_corba_object_create (GnomeObject *object)
{
	POA_GNOME_Calendar_CalFactory *servant;
	CORBA_Environment ev;

	g_return_val_if_fail (object != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_CAL_FACTORY (object), CORBA_OBJECT_NIL);

	servant = (POA_GNOME_Calendar_CalFactory *) g_new0 (GnomeObjectServant, 1);
	servant->vepv = &cal_factory_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Calendar_CalFactory__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_Calendar_CalFactory) gnome_object_activate_servant (object, servant);
}

/**
 * cal_factory_new:
 * @void: 
 * 
 * Creates a new #CalFactory object.
 * 
 * Return value: A newly-created #CalFactory, or NULL if its corresponding CORBA
 * object could not be created.
 **/
CalFactory *
cal_factory_new (void)
{
	CalFactory *factory;
	GNOME_Calendar_CalFactory corba_factory;

	factory = gtk_type_new (CAL_FACTORY_TYPE);
	corba_factory = cal_factory_corba_object_create (GNOME_OBJECT (factory));
	if (corba_object_is_nil (corba_factory)) {
		gtk_object_destroy (factory);
		return NULL;
	}

	return cal_factory_construct (factory, corba_factory);
}
