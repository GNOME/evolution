/* Evolution calendar client interface object
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
#include "cal-backend.h"



/* Private part of the Cal structure */
struct _CalPrivate {
	/* Our backend */
	CalBackend *backend;

	/* Listener on the client we notify */
	GNOME_Evolution_Calendar_Listener listener;
};



static void cal_class_init (CalClass *class);
static void cal_init (Cal *cal);
static void cal_destroy (GtkObject *object);

static POA_GNOME_Evolution_Calendar_Cal__vepv cal_vepv;

static BonoboObjectClass *parent_class;



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

		cal_type = gtk_type_unique (BONOBO_OBJECT_TYPE, &cal_info);
	}

	return cal_type;
}

/* CORBA class initialzation function for the calendar */
static void
init_cal_corba_class (void)
{
	cal_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	cal_vepv.GNOME_Evolution_Calendar_Cal_epv = cal_get_epv ();
}

/* Class initialization function for the calendar */
static void
cal_class_init (CalClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (BONOBO_OBJECT_TYPE);

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

	priv->listener = CORBA_OBJECT_NIL;
}

/* Destroy handler for the calendar */
static void
cal_destroy (GtkObject *object)
{
	Cal *cal;
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL (object));

	cal = CAL (object);
	priv = cal->priv;

	priv->backend = NULL;

	CORBA_exception_init (&ev);
	CORBA_Object_release (priv->listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("cal_destroy(): could not release the listener");

	CORBA_exception_free (&ev);

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
	GnomeVFSURI *uri;
	char *str_uri;
	CORBA_char *str_uri_copy;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	uri = cal_backend_get_uri (priv->backend);
	str_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	str_uri_copy = CORBA_string_dup (str_uri);
	g_free (str_uri);

	return str_uri_copy;
}

/* Converts a calendar object type from its CORBA representation to our own
 * representation.
 */
static CalObjType
uncorba_obj_type (GNOME_Evolution_Calendar_CalObjType type)
{
	return (((type & GNOME_Evolution_Calendar_TYPE_EVENT) ? CALOBJ_TYPE_EVENT : 0)
		| ((type & GNOME_Evolution_Calendar_TYPE_TODO) ? CALOBJ_TYPE_TODO : 0)
		| ((type & GNOME_Evolution_Calendar_TYPE_JOURNAL) ? CALOBJ_TYPE_JOURNAL : 0));
}

/* Cal::get_n_objects method */
static CORBA_long
Cal_get_n_objects (PortableServer_Servant servant,
		   GNOME_Evolution_Calendar_CalObjType type,
		   CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	int t;
	int n;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t = uncorba_obj_type (type);
	n = cal_backend_get_n_objects (priv->backend, t);
	return n;
}

/* Cal::get_object method */
static GNOME_Evolution_Calendar_CalObj
Cal_get_object (PortableServer_Servant servant,
		const GNOME_Evolution_Calendar_CalObjUID uid,
		CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	char *calobj;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	calobj = cal_backend_get_object (priv->backend, uid);

	if (calobj) {
		CORBA_char *calobj_copy;

		calobj_copy = CORBA_string_dup (calobj);
		g_free (calobj);
		return calobj_copy;
	} else {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_NotFound,
				     NULL);
		return NULL;
	}
}

static GNOME_Evolution_Calendar_CalObjUIDSeq *
build_uid_seq (GList *uids)
{
	GNOME_Evolution_Calendar_CalObjUIDSeq *seq;
	GList *l;
	int n, i;

	n = g_list_length (uids);

	seq = GNOME_Evolution_Calendar_CalObjUIDSeq__alloc ();
	CORBA_sequence_set_release (seq, TRUE);
	seq->_length = n;
	seq->_buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalObjUID_allocbuf (n);

	/* Fill the sequence */

	for (i = 0, l = uids; l; i++, l = l->next) {
		char *uid;

		uid = l->data;
		seq->_buffer[i] = CORBA_string_dup (uid);
	}

	return seq;
}

/* Cal::get_uids method */
static GNOME_Evolution_Calendar_CalObjUIDSeq *
Cal_get_uids (PortableServer_Servant servant,
	      GNOME_Evolution_Calendar_CalObjType type,
	      CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	GList *uids;
	GNOME_Evolution_Calendar_CalObjUIDSeq *seq;
	int t;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t = uncorba_obj_type (type);

	uids = cal_backend_get_uids (priv->backend, t);
	seq = build_uid_seq (uids);

	cal_obj_uid_list_free (uids);

	return seq;
}

/* Cal::get_changes method */
static GNOME_Evolution_Calendar_CalObjChangeSeq *
Cal_get_changes (PortableServer_Servant servant,
		 GNOME_Evolution_Calendar_CalObjType type,
		 const CORBA_char *change_id,
		 CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	int t;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t = uncorba_obj_type (type);
	
	return cal_backend_get_changes (priv->backend, t, change_id);
}

/* Cal::get_objects_in_range method */
static GNOME_Evolution_Calendar_CalObjUIDSeq *
Cal_get_objects_in_range (PortableServer_Servant servant,
			  GNOME_Evolution_Calendar_CalObjType type,
			  GNOME_Evolution_Calendar_Time_t start,
			  GNOME_Evolution_Calendar_Time_t end,
			  CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	int t;
	time_t t_start, t_end;
	GNOME_Evolution_Calendar_CalObjUIDSeq *seq;
	GList *uids;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t = uncorba_obj_type (type);
	t_start = (time_t) start;
	t_end = (time_t) end;

	if (t_start > t_end || t_start == -1 || t_end == -1) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_InvalidRange,
				     NULL);
		return NULL;
	}

	uids = cal_backend_get_objects_in_range (priv->backend, t, t_start, t_end);
	seq = build_uid_seq (uids);

	cal_obj_uid_list_free (uids);

	return seq;
}

/* Cal::get_alarms_in_range method */
static GNOME_Evolution_Calendar_CalComponentAlarmsSeq *
Cal_get_alarms_in_range (PortableServer_Servant servant,
			 GNOME_Evolution_Calendar_Time_t start,
			 GNOME_Evolution_Calendar_Time_t end,
			 CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	time_t t_start, t_end;
	gboolean valid_range;
	GNOME_Evolution_Calendar_CalComponentAlarmsSeq *seq;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t_start = (time_t) start;
	t_end = (time_t) end;

	seq = cal_backend_get_alarms_in_range (priv->backend, t_start, t_end, &valid_range);
	if (!valid_range) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_InvalidRange,
				     NULL);
		return NULL;
	}

	return seq;
}

/* Cal::get_alarms_for_object method */
static GNOME_Evolution_Calendar_CalComponentAlarms *
Cal_get_alarms_for_object (PortableServer_Servant servant,
			   const GNOME_Evolution_Calendar_CalObjUID uid,
			   GNOME_Evolution_Calendar_Time_t start,
			   GNOME_Evolution_Calendar_Time_t end,
			   CORBA_Environment * ev)
{
	Cal *cal;
	CalPrivate *priv;
	time_t t_start, t_end;
	GNOME_Evolution_Calendar_CalComponentAlarms *alarms;
	CalBackendGetAlarmsForObjectResult result;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t_start = (time_t) start;
	t_end = (time_t) end;

	alarms = cal_backend_get_alarms_for_object (priv->backend, uid, t_start, t_end, &result);

	switch (result) {
	case CAL_BACKEND_GET_ALARMS_SUCCESS:
		return alarms;

	case CAL_BACKEND_GET_ALARMS_NOT_FOUND:
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_NotFound,
				     NULL);
		return NULL;

	case CAL_BACKEND_GET_ALARMS_INVALID_RANGE:
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_InvalidRange,
				     NULL);
		return NULL;

	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/* Cal::update_object method */
static void
Cal_update_object (PortableServer_Servant servant,
		   const GNOME_Evolution_Calendar_CalObjUID uid,
		   const GNOME_Evolution_Calendar_CalObj calobj,
		   CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	if (!cal_backend_update_object (priv->backend, uid, calobj))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_InvalidObject,
				     NULL);
}

/* Cal::remove_object method */
static void
Cal_remove_object (PortableServer_Servant servant,
		   const GNOME_Evolution_Calendar_CalObjUID uid,
		   CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	if (!cal_backend_remove_object (priv->backend, uid))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_NotFound,
				     NULL);
}

/**
 * cal_get_epv:
 * @void:
 *
 * Creates an EPV for the Cal CORBA class.
 *
 * Return value: A newly-allocated EPV.
 **/
POA_GNOME_Evolution_Calendar_Cal__epv *
cal_get_epv (void)
{
	POA_GNOME_Evolution_Calendar_Cal__epv *epv;

	epv = g_new0 (POA_GNOME_Evolution_Calendar_Cal__epv, 1);
	epv->_get_uri     = Cal_get_uri;
	epv->countObjects = Cal_get_n_objects;
	epv->getObject    = Cal_get_object;
	epv->getUIds      = Cal_get_uids;
	epv->getChanges   = Cal_get_changes;
	epv->getObjectsInRange  = Cal_get_objects_in_range;
	epv->getAlarmsInRange   = Cal_get_alarms_in_range;
	epv->getAlarmsForObject = Cal_get_alarms_for_object;
	epv->updateObject = Cal_update_object;
	epv->removeObject = Cal_remove_object;

	return epv;
}



/**
 * cal_construct:
 * @cal: A calendar client interface.
 * @corba_cal: CORBA object for the calendar.
 * @backend: Calendar backend that this @cal presents an interface to.
 * @listener: Calendar listener for notification.
 *
 * Constructs a calendar client interface object by binding the corresponding
 * CORBA object to it.  The calendar interface is bound to the specified
 * @backend, and will notify the @listener about changes to the calendar.
 *
 * Return value: The same object as the @cal argument.
 **/
Cal *
cal_construct (Cal *cal,
	       GNOME_Evolution_Calendar_Cal corba_cal,
	       CalBackend *backend,
	       GNOME_Evolution_Calendar_Listener listener)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_val_if_fail (cal != NULL, NULL);
	g_return_val_if_fail (IS_CAL (cal), NULL);
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	priv = cal->priv;

	CORBA_exception_init (&ev);
	priv->listener = CORBA_Object_duplicate (listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_construct: could not duplicate the listener");
		priv->listener = CORBA_OBJECT_NIL;
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	priv->backend = backend;

	bonobo_object_construct (BONOBO_OBJECT (cal), corba_cal);
	return cal;
}

/**
 * cal_corba_object_create:
 * @object: #BonoboObject that will wrap the CORBA object.
 *
 * Creates and activates the CORBA object that is wrapped by the specified
 * calendar client interface @object.
 *
 * Return value: An activated object reference or #CORBA_OBJECT_NIL in case of
 * failure.
 **/
GNOME_Evolution_Calendar_Cal
cal_corba_object_create (BonoboObject *object)
{
	POA_GNOME_Evolution_Calendar_Cal *servant;
	CORBA_Environment ev;

	g_return_val_if_fail (object != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_CAL (object), CORBA_OBJECT_NIL);

	servant = (POA_GNOME_Evolution_Calendar_Cal *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &cal_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_Calendar_Cal__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_corba_object_create(): could not init the servant");
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_Evolution_Calendar_Cal) bonobo_object_activate_servant (object, servant);
}

/**
 * cal_new:
 * @backend: A calendar backend.
 * @listener: A calendar listener.
 *
 * Creates a new calendar client interface object and binds it to the specified
 * @backend and @listener objects.
 *
 * Return value: A newly-created #Cal calendar client interface object, or NULL
 * if its corresponding CORBA object could not be created.
 **/
Cal *
cal_new (CalBackend *backend, GNOME_Evolution_Calendar_Listener listener)
{
	Cal *cal, *retval;
	GNOME_Evolution_Calendar_Cal corba_cal;
	CORBA_Environment ev;
	gboolean ret;

	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	cal = CAL (gtk_type_new (CAL_TYPE));
	corba_cal = cal_corba_object_create (BONOBO_OBJECT (cal));

	CORBA_exception_init (&ev);
	ret = CORBA_Object_is_nil ((CORBA_Object) corba_cal, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || ret) {
		g_message ("cal_new(): could not create the CORBA object");
		bonobo_object_unref (BONOBO_OBJECT (cal));
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	retval = cal_construct (cal, corba_cal, backend, listener);
	if (!retval) {
		g_message ("cal_new(): could not construct the calendar client interface");
		bonobo_object_unref (BONOBO_OBJECT (cal));
		return NULL;
	}

	return retval;
}

/**
 * cal_notify_update:
 * @cal: A calendar client interface.
 * @uid: UID of object that was updated.
 * 
 * Notifies a listener attached to a calendar client interface object about an
 * update to a calendar object.
 **/
void
cal_notify_update (Cal *cal, const char *uid)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));
	g_return_if_fail (uid != NULL);

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyObjUpdated (priv->listener, (char *) uid, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("cal_notify_update(): could not notify the listener "
			   "about an updated object");

	CORBA_exception_free (&ev);
}

/**
 * cal_notify_remove:
 * @cal: A calendar client interface.
 * @uid: UID of object that was removed.
 * 
 * Notifies a listener attached to a calendar client interface object about a
 * calendar object that was removed.
 **/
void
cal_notify_remove (Cal *cal, const char *uid)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));
	g_return_if_fail (uid != NULL);

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyObjRemoved (priv->listener, (char *) uid, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("cal_notify_remove(): could not notify the listener "
			   "about a removed object");

	CORBA_exception_free (&ev);
}
