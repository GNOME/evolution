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
#include <ical.h>
#include "cal.h"
#include "query.h"
#include "wombat.h"

#define PARENT_TYPE         BONOBO_X_OBJECT_TYPE

static BonoboXObjectClass *parent_class;

/* Private part of the Cal structure */
struct _CalPrivate {
	/* Our backend */
	CalBackend *backend;

	/* Listener on the client we notify */
	GNOME_Evolution_Calendar_Listener listener;

	/* and a reference to the WombatClient interface */
	GNOME_Evolution_WombatClient wombat_client;
};


/* Cal::get_uri method */
static CORBA_char *
impl_Cal_get_uri (PortableServer_Servant servant,
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
impl_Cal_get_n_objects (PortableServer_Servant servant,
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
impl_Cal_get_object (PortableServer_Servant servant,
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
impl_Cal_get_uids (PortableServer_Servant servant,
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
impl_Cal_get_changes (PortableServer_Servant servant,
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
impl_Cal_get_objects_in_range (PortableServer_Servant servant,
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

/* Cal::get_free_busy method */
static GNOME_Evolution_Calendar_CalObj
impl_Cal_get_free_busy (PortableServer_Servant servant,
			GNOME_Evolution_Calendar_Time_t start,
			GNOME_Evolution_Calendar_Time_t end,
			CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	time_t t_start, t_end;
	char *calobj;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t_start = (time_t) start;
	t_end = (time_t) end;

	if (t_start > t_end || t_start == -1 || t_end == -1) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_InvalidRange,
				     NULL);
		return NULL;
	}

	calobj = cal_backend_get_free_busy (priv->backend, t_start, t_end);
        if (calobj) {
		CORBA_char *calobj_copy;

		calobj_copy = CORBA_string_dup (calobj);
		g_free (calobj);
		return calobj_copy;
	}

	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_GNOME_Evolution_Calendar_Cal_NotFound,
			     NULL);

	return NULL;
}

/* Cal::get_alarms_in_range method */
static GNOME_Evolution_Calendar_CalComponentAlarmsSeq *
impl_Cal_get_alarms_in_range (PortableServer_Servant servant,
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
impl_Cal_get_alarms_for_object (PortableServer_Servant servant,
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
impl_Cal_update_object (PortableServer_Servant servant,
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
impl_Cal_remove_object (PortableServer_Servant servant,
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

/* Cal::getQuery implementation */
static GNOME_Evolution_Calendar_Query
impl_Cal_get_query (PortableServer_Servant servant,
		    const CORBA_char *sexp,
		    GNOME_Evolution_Calendar_QueryListener ql,
		    CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	Query *query;
	CORBA_Environment ev2;
	GNOME_Evolution_Calendar_Query query_copy;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	query = query_new (priv->backend, ql, sexp);
	if (!query) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_CouldNotCreate,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_init (&ev2);
	query_copy = CORBA_Object_duplicate (BONOBO_OBJREF (query), &ev2);
	if (ev2._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev2);
		g_message ("Cal_get_query(): Could not duplicate the query reference");
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_Cal_CouldNotCreate,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev2);

	return query_copy;
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

	/* obtain the WombatClient interface */
	priv->wombat_client = Bonobo_Unknown_queryInterface (
		priv->listener,
		"IDL:GNOME/Evolution/WombatClient:1.0",
		&ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_construct: could not get the WombatClient interface");
		priv->wombat_client = CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);

	priv->backend = backend;

	return cal;
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

	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	cal = CAL (gtk_type_new (CAL_TYPE));

	retval = cal_construct (cal, backend, listener);
	if (!retval) {
		g_message ("cal_new(): could not construct the calendar client interface");
		bonobo_object_unref (BONOBO_OBJECT (cal));
		return NULL;
	}

	return retval;
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



/* Class initialization function for the calendar */
static void
cal_class_init (CalClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	POA_GNOME_Evolution_Calendar_Cal__epv *epv = &klass->epv;

	parent_class = gtk_type_class (BONOBO_OBJECT_TYPE);

	/* Class method overrides */
	object_class->destroy = cal_destroy;

	/* Epv methods */
	epv->_get_uri = impl_Cal_get_uri;
	epv->countObjects = impl_Cal_get_n_objects;
	epv->getObject = impl_Cal_get_object;
	epv->getUIDs = impl_Cal_get_uids;
	epv->getChanges = impl_Cal_get_changes;
	epv->getObjectsInRange = impl_Cal_get_objects_in_range;
	epv->getFreeBusy = impl_Cal_get_free_busy;
	epv->getAlarmsInRange = impl_Cal_get_alarms_in_range;
	epv->getAlarmsForObject = impl_Cal_get_alarms_for_object;
	epv->updateObject = impl_Cal_update_object;
	epv->removeObject = impl_Cal_remove_object;
	epv->getQuery = impl_Cal_get_query;
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

BONOBO_X_TYPE_FUNC_FULL (Cal, GNOME_Evolution_Calendar_Cal, PARENT_TYPE, cal);

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
