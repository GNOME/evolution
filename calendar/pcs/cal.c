/* GNOME calendar object
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
#include "cal.h"



/* Private part of the Cal structure */
typedef struct {
	/* The URI where this calendar is stored */
	char *uri;

	/* List of listeners for this calendar */
	GList *listeners;
} CalPrivate;



static void cal_class_init (CalClass *class);
static void cal_init (Cal *cal);
static void cal_destroy (GtkObject *object);

static POA_GNOME_Calendar_Cal__vepv cal_vepv;

static GnomeObjectClass *parent_class;



/**
 * cal_get_type:
 * @void:
 *
 * Registers the #Cal class if necessary, and returns the type ID associated to
 * it.
 *
 * Return value: The type ID of the #Cal class.
 **/
GtkType
cal_get_type (void)
{
	static GtkType cal_type = 0;

	if (!cal_type) {
		static const GtkTypeInfo cal_info = {
			"Cal",
			sizeof (Cal),
			sizeof (CalClass),
			(GtkClassInitFunc) cal_class_init,
			(GtkObjectInitFunc) cal_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_type = gtk_type_unique (gnome_object_get_type (), &cal_info);
	}

	return cal_type;
}

/* CORBA class initialzation function for the calendar */
static void
init_cal_corba_class (void)
{
	cal_vepv.GNOME_Unknown_epv = gnome_object_get_epv ();
	cal_vepv.GNOME_Calendar_Cal_epv = cal_get_epv ();
}

/* Class initialization function for the calendar */
static void
cal_class_init (CalClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gnome_object_get_type ());

	object_class->destroy = cal_destroy;

	init_cal_corba_class ();
}

/* Object initialization function for the calendar */
static void
cal_init (Cal *cal)
{
	CalPrivate *priv;

	priv = g_new0 (CalPrivate, 1);
	cal->priv = priv;
}

/* Destroy handler for the calendar */
static void
cal_destroy (GtkObject *object)
{
	Cal *cal;
	CalPrivate *priv;
	GList *l;
	CORBA_Environment *ev;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL (object));

	cal = CAL (object);
	priv = cal->priv;

	if (priv->uri)
		g_free (priv->uri);

	CORBA_exception_init (&ev);

	for (l = priv->listeners; l; l = l->next) {
		GNOME_Unknown_unref (l->data, &ev);
		CORBA_Object_release (l->data, &ev);
	}

	g_list_free (priv->listeners);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}




/* CORBA servant implementation */

/* Cal::get_uri method */
static CORBA_char *
Cal_get_uri (PortableServer_Servant servant,
	     CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (gnome_object_from_servant (servant));
	priv = cal->priv;

	return CORBA_string_dup (priv->uri);
}

/**
 * cal_get_epv:
 * @void:
 *
 * Creates an EPV for the Cal CORBA class.
 *
 * Return value: A newly-allocated EPV.
 **/
POA_GNOME_Calendar_Cal__epv *
cal_get_epv (void)
{
	POA_GNOME_Calendar_Cal__epv *epv;

	epv = g_new0 (POA_GNOME_Calendar_Cal__epv, 1);
	epv->get_uri = Cal_get_uri;

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
 * cal_construct:
 * @cal: A calendar.
 * @corba_cal: CORBA object for the calendar.
 *
 * Constructs a calendar by binding the corresponding CORBA object to it.
 *
 * Return value: The same object as the @cal argument.
 **/
Cal *
cal_construct (Cal *cal, GNOME_Calendar_Cal corba_cal)
{
	g_return_val_if_fail (cal != NULL, NULL);
	g_return_val_if_fail (IS_CAL (cal), NULL);
	g_return_val_if_fail (!corba_object_is_nil (corba_cal), NULL);

	gnome_object_construct (GNOME_OBJECT (cal), corba_cal);
	return cal;
}

/**
 * cal_corba_object_create:
 * @object: #GnomeObject that will wrap the CORBA object.
 *
 * Creates and activates the CORBA object that is wrapped by the specified
 * calendar @object.
 *
 * Return value: An activated object reference or #CORBA_OBJECT_NIL in case of
 * failure.
 **/
GNOME_Calendar_Cal
cal_corba_object_create (GnomeObject *object)
{
	POA_GNOME_Calendar_Cal *servant;
	CORBA_Environment ev;

	g_return_val_if_fail (object != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_CAL (object), CORBA_OBJECT_NIL);

	servant = (POA_GNOME_Calendar_Cal *) g_new0 (GnomeObjectServant, 1);
	servant->vepv = &cal_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Calendar_Cal__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_Calendar_Cal) gnome_object_activate_servant (object, servant);
}

/**
 * cal_add_listener:
 * @cal: A calendar.
 * @listener: A listener.
 *
 * Adds a listener for changes to a calendar.  The specified listener object
 * will be used for notification when objects are added, removed, or changed in
 * the calendar.
 **/
void
cal_add_listener (Cal *cal, GNOME_Calendar_Listener listener)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));
	g_return_if_fail (!corba_object_is_nil (listener));

	priv = cal->priv;

	CORBA_exception_init (&ev);

	GNOME_Unknown_ref (listener, &ev);
	priv->listeners = g_list_prepend (priv->listeners, CORBA_Object_duplicate (listener, &ev));

	CORBA_exception_free (&ev);
}

/**
 * cal_remove_listener:
 * @cal: A calendar.
 * @listener: A listener.
 *
 * Removes a listener from a calendar so that no more notification events will
 * be sent to the listener.
 **/
void
cal_remove_listener (Cal *cal, GNOME_Calendar_Listener listener)
{
	CalPrivate *priv;
	CORBA_Environment ev;
	GList *l;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;

	CORBA_exception_init (&ev);

	/* FIXME: CORBA_Object_is_equivalent() is not what one thinks.  This
	 * code could fail in situtations subtle enough that I don't understand
	 * them.  Someone has to figure out the standard CORBA idiom for
	 * listeners or notification.
	 */
	for (l = priv->listeners; l; l = l->next)
		if (CORBA_Object_is_equivalent (listener, l->data)) {
			GNOME_Unknown_unref (listener, &ev);
			CORBA_Object_release (listener, &ev);
			priv->listeners = g_list_remove_link (priv->listeners, l);
			g_list_free_1 (l);
			break;
		}

	CORBA_exception_free (&ev);
}
