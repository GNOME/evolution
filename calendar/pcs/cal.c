/* Evolution calendar client interface object
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
#include "cal-backend.h"



/* Private part of the Cal structure */
typedef struct {
	/* Our backend */
	CalBackend *backend;

	/* Listener on the client we notify */
	Evolution_Calendar_Listener listener;
} CalPrivate;



static void cal_class_init (CalClass *class);
static void cal_init (Cal *cal);
static void cal_destroy (GtkObject *object);

static POA_Evolution_Calendar_Cal__vepv cal_vepv;

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
	cal_vepv.Evolution_Calendar_Cal_epv = cal_get_epv ();
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
uncorba_obj_type (Evolution_Calendar_CalObjType type)
{
	return (((type & Evolution_Calendar_TYPE_EVENT) ? CALOBJ_TYPE_EVENT : 0)
		| ((type & Evolution_Calendar_TYPE_TODO) ? CALOBJ_TYPE_TODO : 0)
		| ((type & Evolution_Calendar_TYPE_JOURNAL) ? CALOBJ_TYPE_JOURNAL : 0));
}

/* Cal::get_n_objects method */
static CORBA_long
Cal_get_n_objects (PortableServer_Servant servant,
		   Evolution_Calendar_CalObjType type,
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
static Evolution_Calendar_CalObj
Cal_get_object (PortableServer_Servant servant,
		const Evolution_Calendar_CalObjUID uid,
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
				     ex_Evolution_Calendar_Cal_NotFound,
				     NULL);
		return NULL;
	}
}

static Evolution_Calendar_CalObjUIDSeq *
build_uid_seq (GList *uids)
{
	Evolution_Calendar_CalObjUIDSeq *seq;
	GList *l;
	int n, i;

	n = g_list_length (uids);

	seq = Evolution_Calendar_CalObjUIDSeq__alloc ();
	CORBA_sequence_set_release (seq, TRUE);
	seq->_length = n;
	seq->_buffer = CORBA_sequence_Evolution_Calendar_CalObjUID_allocbuf (n);

	/* Fill the sequence */

	for (i = 0, l = uids; l; i++, l = l->next) {
		char *uid;

		uid = l->data;
		seq->_buffer[i] = CORBA_string_dup (uid);
	}

	return seq;
}

/* Cal::get_uids method */
static Evolution_Calendar_CalObjUIDSeq *
Cal_get_uids (PortableServer_Servant servant,
	      Evolution_Calendar_CalObjType type,
	      CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	GList *uids;
	Evolution_Calendar_CalObjUIDSeq *seq;
	int t;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t = uncorba_obj_type (type);

	uids = cal_backend_get_uids (priv->backend, t);
	seq = build_uid_seq (uids);

	cal_obj_uid_list_free (uids);

	return seq;
}

/* Cal::get_objects_in_range method */
static Evolution_Calendar_CalObjUIDSeq *
Cal_get_objects_in_range (PortableServer_Servant servant,
			  Evolution_Calendar_CalObjType type,
			  Evolution_Calendar_Time_t start,
			  Evolution_Calendar_Time_t end,
			  CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	int t;
	time_t t_start, t_end;
	Evolution_Calendar_CalObjUIDSeq *seq;
	GList *uids;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t = uncorba_obj_type (type);
	t_start = (time_t) start;
	t_end = (time_t) end;

	if (t_start > t_end || t_start == -1 || t_end == -1) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_Calendar_Cal_InvalidRange,
				     NULL);
		return NULL;
	}

	uids = cal_backend_get_objects_in_range (priv->backend, t, t_start, t_end);
	seq = build_uid_seq (uids);

	cal_obj_uid_list_free (uids);

	return seq;
}

#if 0
/* Translates an enum AlarmType to its CORBA representation */
static Evolution_Calendar_AlarmType
corba_alarm_type (enum AlarmType type)
{
	switch (type) {
	case ALARM_MAIL:
		return Evolution_Calendar_MAIL;

	case ALARM_PROGRAM:
		return Evolution_Calendar_PROGRAM;

	case ALARM_DISPLAY:
		return Evolution_Calendar_DISPLAY;

	case ALARM_AUDIO:
		return Evolution_Calendar_AUDIO;

	default:
		g_assert_not_reached ();
		return Evolution_Calendar_DISPLAY;
	}
}
#endif

/* Builds a CORBA sequence of alarm instances from a CalAlarmInstance list. */
static Evolution_Calendar_CalAlarmInstanceSeq *
build_alarm_instance_seq (GList *alarms)
{
	GList *l;
	int n, i;
	Evolution_Calendar_CalAlarmInstanceSeq *seq;

	n = g_list_length (alarms);

	seq = Evolution_Calendar_CalAlarmInstanceSeq__alloc ();
	CORBA_sequence_set_release (seq, TRUE);
	seq->_length = n;
	seq->_buffer = CORBA_sequence_Evolution_Calendar_CalAlarmInstance_allocbuf (n);

	/* Fill the sequence */

	for (i = 0, l = alarms; l; i++, l = l->next) {
		CalAlarmInstance *ai;
		Evolution_Calendar_CalAlarmInstance *corba_ai;

		ai = l->data;
		corba_ai = &seq->_buffer[i];

		corba_ai->uid = CORBA_string_dup (ai->uid);
#if 0
		corba_ai->type = corba_alarm_type (ai->type);
#endif
		corba_ai->trigger = ai->trigger;
		corba_ai->occur = ai->occur;
	}

	return seq;
}

/* Cal::get_alarms_in_range method */
static Evolution_Calendar_CalAlarmInstanceSeq *
Cal_get_alarms_in_range (PortableServer_Servant servant,
			 Evolution_Calendar_Time_t start,
			 Evolution_Calendar_Time_t end,
			 CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	time_t t_start, t_end;
	Evolution_Calendar_CalAlarmInstanceSeq *seq;
	GList *alarms;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t_start = (time_t) start;
	t_end = (time_t) end;

	if (t_start > t_end || t_start == -1 || t_end == -1) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_Calendar_Cal_InvalidRange,
				     NULL);
		return NULL;
	}

	/* Figure out the list and allocate the sequence */

	alarms = cal_backend_get_alarms_in_range (priv->backend, t_start, t_end);
	seq = build_alarm_instance_seq (alarms);
	cal_alarm_instance_list_free (alarms);

	return seq;
}

/* Cal::get_alarms_for_object method */
static Evolution_Calendar_CalAlarmInstanceSeq *
Cal_get_alarms_for_object (PortableServer_Servant servant,
			   const Evolution_Calendar_CalObjUID uid,
			   Evolution_Calendar_Time_t start,
			   Evolution_Calendar_Time_t end,
			   CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	time_t t_start, t_end;
	Evolution_Calendar_CalAlarmInstanceSeq *seq;
	GList *alarms;
	gboolean result;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t_start = (time_t) start;
	t_end = (time_t) end;

	if (t_start > t_end || t_start == -1 || t_end == -1) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_Calendar_Cal_InvalidRange,
				     NULL);
		return NULL;
	}

	result = cal_backend_get_alarms_for_object (priv->backend, uid, t_start, t_end, &alarms);
	if (!result) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_Calendar_Cal_NotFound,
				     NULL);
		return NULL;
	}

	seq = build_alarm_instance_seq (alarms);
	cal_alarm_instance_list_free (alarms);

	return seq;
}

/* Cal::update_object method */
static void
Cal_update_object (PortableServer_Servant servant,
		   const Evolution_Calendar_CalObjUID uid,
		   const Evolution_Calendar_CalObj calobj,
		   CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	if (!cal_backend_update_object (priv->backend, uid, calobj))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_Calendar_Cal_InvalidObject,
				     NULL);
}

/* Cal::remove_object method */
static void
Cal_remove_object (PortableServer_Servant servant,
		   const Evolution_Calendar_CalObjUID uid,
		   CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	if (!cal_backend_remove_object (priv->backend, uid))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_Calendar_Cal_NotFound,
				     NULL);
}



/* Cal::get_uid_by_pilot_id method */
static Evolution_Calendar_CalObjUID
Cal_get_uid_by_pilot_id (PortableServer_Servant servant,
			 const Evolution_Calendar_PilotID pilot_id,
			 CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	char *uid;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	uid = cal_backend_get_uid_by_pilot_id (priv->backend, pilot_id);

	if (uid) {
		CORBA_char *uid_copy;

		uid_copy = CORBA_string_dup (uid);
		g_free (uid);
		return uid_copy;
	} else {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_Calendar_Cal_NotFound,
				     NULL);
		return NULL;
	}
}


/* Cal::update_pilot_id method */
static void
Cal_update_pilot_id (PortableServer_Servant servant,
		     const Evolution_Calendar_CalObjUID uid,
		     const Evolution_Calendar_PilotID pilot_id,
		     const CORBA_unsigned_long pilot_status,
		     CORBA_Environment * ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_update_pilot_id (priv->backend, uid,
				     pilot_id, pilot_status);
}


/**
 * cal_get_epv:
 * @void:
 *
 * Creates an EPV for the Cal CORBA class.
 *
 * Return value: A newly-allocated EPV.
 **/
POA_Evolution_Calendar_Cal__epv *
cal_get_epv (void)
{
	POA_Evolution_Calendar_Cal__epv *epv;

	epv = g_new0 (POA_Evolution_Calendar_Cal__epv, 1);
	epv->_get_uri = Cal_get_uri;
	epv->get_n_objects = Cal_get_n_objects;
	epv->get_object = Cal_get_object;
	epv->get_uids = Cal_get_uids;
	epv->get_objects_in_range = Cal_get_objects_in_range;
	epv->get_alarms_in_range = Cal_get_alarms_in_range;
	epv->get_alarms_for_object = Cal_get_alarms_for_object;
	epv->update_object = Cal_update_object;
	epv->remove_object = Cal_remove_object;
	epv->get_uid_by_pilot_id = Cal_get_uid_by_pilot_id;
	epv->update_pilot_id = Cal_update_pilot_id;

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
	       Evolution_Calendar_Cal corba_cal,
	       CalBackend *backend,
	       Evolution_Calendar_Listener listener)
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
Evolution_Calendar_Cal
cal_corba_object_create (BonoboObject *object)
{
	POA_Evolution_Calendar_Cal *servant;
	CORBA_Environment ev;

	g_return_val_if_fail (object != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_CAL (object), CORBA_OBJECT_NIL);

	servant = (POA_Evolution_Calendar_Cal *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &cal_vepv;

	CORBA_exception_init (&ev);
	POA_Evolution_Calendar_Cal__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_corba_object_create(): could not init the servant");
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Evolution_Calendar_Cal) bonobo_object_activate_servant (object, servant);
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
cal_new (CalBackend *backend, Evolution_Calendar_Listener listener)
{
	Cal *cal, *retval;
	Evolution_Calendar_Cal corba_cal;
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
	Evolution_Calendar_Listener_obj_updated (priv->listener, (char *) uid, &ev);

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
	Evolution_Calendar_Listener_obj_removed (priv->listener, (char *) uid, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("cal_notify_remove(): could not notify the listener "
			   "about a removed object");

	CORBA_exception_free (&ev);
}
