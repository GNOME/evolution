/* Evolution calendar client interface object
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
#include <bonobo/bonobo-exception.h>
#include "cal.h"
#include "cal-backend.h"
#include "query.h"
#include "Evolution-Wombat.h"

#define PARENT_TYPE         BONOBO_X_OBJECT_TYPE

static BonoboXObjectClass *parent_class;

/* Private part of the Cal structure */
struct _CalPrivate {
	/* Our backend */
	CalBackend *backend;

	/* Listener on the client we notify */
	GNOME_Evolution_Calendar_Listener listener;

	/* A reference to the WombatClient interface */
	GNOME_Evolution_WombatClient wombat_client;
};


/* Cal::get_uri method */
static CORBA_char *
impl_Cal_get_uri (PortableServer_Servant servant,
		  CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	const char *str_uri;
	CORBA_char *str_uri_copy;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	str_uri = cal_backend_get_uri (priv->backend);
	str_uri_copy = CORBA_string_dup (str_uri);

	return str_uri_copy;
}

/* Cal::is_read_only method */
static CORBA_boolean
impl_Cal_is_read_only (PortableServer_Servant servant,
		       CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	return cal_backend_is_read_only (priv->backend);
}
		       
/* Cal::get_cal_address method */
static CORBA_char *
impl_Cal_get_cal_address (PortableServer_Servant servant,
			    CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	const char *str_cal_address;
	CORBA_char *str_cal_address_copy;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	str_cal_address = cal_backend_get_cal_address (priv->backend);
	if (str_cal_address == NULL) {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);
		return CORBA_OBJECT_NIL;
	}

	str_cal_address_copy = CORBA_string_dup (str_cal_address);

	return str_cal_address_copy;
}
		       
/* Cal::get_alarm_email_address method */
static CORBA_char *
impl_Cal_get_alarm_email_address (PortableServer_Servant servant,
				  CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	const char *str_email_address;
	CORBA_char *str_email_address_copy;
	
	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;
	
	str_email_address = cal_backend_get_alarm_email_address (priv->backend);
	if (str_email_address == NULL) {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);
		return CORBA_OBJECT_NIL;
	}

	str_email_address_copy = CORBA_string_dup (str_email_address);

	return str_email_address_copy;
}
		       
/* Cal::get_ldap_attribute method */
static CORBA_char *
impl_Cal_get_ldap_attribute (PortableServer_Servant servant,
			     CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	const char *str_ldap_attr;
	CORBA_char *str_ldap_attr_copy;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	str_ldap_attr = cal_backend_get_ldap_attribute (priv->backend);
	if (str_ldap_attr == NULL) {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);
		return CORBA_OBJECT_NIL;
	}

	str_ldap_attr_copy = CORBA_string_dup (str_ldap_attr);

	return str_ldap_attr_copy;
}

/* Cal::getSchedulingInformation method */
static CORBA_char *
impl_Cal_get_static_capabilities (PortableServer_Servant servant,
				  CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	const char *cap;
	CORBA_char *cap_copy;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cap = cal_backend_get_static_capabilities (priv->backend);
	cap_copy = CORBA_string_dup (cap == NULL ? "" : cap);

	return cap_copy;
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

static void
impl_Cal_set_mode (PortableServer_Servant servant,
		   GNOME_Evolution_Calendar_CalMode mode,
		   CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	
	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_set_mode (priv->backend, mode);
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
impl_Cal_get_default_object (PortableServer_Servant servant,
			     GNOME_Evolution_Calendar_CalObjType type,
			     CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	GNOME_Evolution_Calendar_CalObj calobj_copy;
	char *calobj;
	

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	calobj = cal_backend_get_default_object (priv->backend, type);
	calobj_copy = CORBA_string_dup (calobj);
	g_free (calobj);

	return calobj_copy;
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
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);

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
		bonobo_exception_set (ev,  ex_GNOME_Evolution_Calendar_Cal_InvalidRange);
		return NULL;
	}

	uids = cal_backend_get_objects_in_range (priv->backend, t, t_start, t_end);
	seq = build_uid_seq (uids);

	cal_obj_uid_list_free (uids);

	return seq;
}

static GNOME_Evolution_Calendar_CalObjSeq *
build_fb_seq (GList *obj_list)
{
	GNOME_Evolution_Calendar_CalObjSeq *seq;
	GList *l;
	int n, i;

	n = g_list_length (obj_list);

	seq = GNOME_Evolution_Calendar_CalObjSeq__alloc ();
	CORBA_sequence_set_release (seq, TRUE);
	seq->_maximum = n;
	seq->_length = n;
	seq->_buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalObj_allocbuf (n);

	/* Fill the sequence */

	for (i = 0, l = obj_list; l; i++, l = l->next) {
		char *calobj;

		calobj = l->data;
		seq->_buffer[i] = CORBA_string_dup (calobj);
	}

	return seq;
}

/* Cal::get_free_busy method */
static GNOME_Evolution_Calendar_CalObjSeq *
impl_Cal_get_free_busy (PortableServer_Servant servant,
			const GNOME_Evolution_Calendar_UserList *user_list,
			const GNOME_Evolution_Calendar_Time_t start,
			const GNOME_Evolution_Calendar_Time_t end,
			CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	time_t t_start, t_end;
	GList *users = NULL;
	GList *obj_list;
	GNOME_Evolution_Calendar_CalObjSeq *seq;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	t_start = (time_t) start;
	t_end = (time_t) end;

	if (t_start > t_end || t_start == -1 || t_end == -1) {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_InvalidRange);
		return build_fb_seq (NULL);
	}

	/* convert the CORBA user list to a GList */
	if (user_list) {
		int i;

		for (i = 0; i < user_list->_length; i++)
			users = g_list_append (users, user_list->_buffer[i]);
	}

	/* call the backend's get_free_busy method */
	obj_list = cal_backend_get_free_busy (priv->backend, users, t_start, t_end);
	seq = build_fb_seq (obj_list);	
	g_list_free (users);

        if (obj_list == NULL)
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);

        return seq;
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
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_InvalidRange);
		return NULL;
	}

	if (!seq) {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);
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
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);
		return NULL;

	case CAL_BACKEND_GET_ALARMS_INVALID_RANGE:
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_InvalidRange);
		return NULL;

	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/* Cal::update_objects method */
static void
impl_Cal_update_objects (PortableServer_Servant servant,
			 const GNOME_Evolution_Calendar_CalObj calobj,
			 const GNOME_Evolution_Calendar_CalObjModType mod,
			 CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	CalBackendResult result;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	result = cal_backend_update_objects (priv->backend, calobj, mod);
	switch (result) {
	case CAL_BACKEND_RESULT_INVALID_OBJECT :
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_InvalidObject);
		break;
	case CAL_BACKEND_RESULT_NOT_FOUND :
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);
		break;
	case CAL_BACKEND_RESULT_PERMISSION_DENIED :
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_PermissionDenied);
		break;
	default :
		break;
	}
}

/* Cal::remove_object method */
static void
impl_Cal_remove_object (PortableServer_Servant servant,
			const GNOME_Evolution_Calendar_CalObjUID uid,
			const GNOME_Evolution_Calendar_CalObjModType mod,
			CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	CalBackendResult result;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	result = cal_backend_remove_object (priv->backend, uid, mod);
	switch (result) {
	case CAL_BACKEND_RESULT_INVALID_OBJECT :
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_InvalidObject);
		break;
	case CAL_BACKEND_RESULT_NOT_FOUND :
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);
		break;
	case CAL_BACKEND_RESULT_PERMISSION_DENIED :
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_PermissionDenied);
		break;
	default :
		break;
	}
}

/* Cal::send_object method */
static GNOME_Evolution_Calendar_CalObj
impl_Cal_send_object (PortableServer_Servant servant,
		      const GNOME_Evolution_Calendar_CalObj calobj,
		      GNOME_Evolution_Calendar_UserList **user_list,
		      CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	CORBA_char *calobj_copy;
	char *new_calobj;
	GNOME_Evolution_Calendar_Cal_Busy *err;
	CalBackendSendResult result;
	char error_msg[256];
	
	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	result = cal_backend_send_object (priv->backend, calobj, &new_calobj, user_list, error_msg);
	switch (result) {
	case CAL_BACKEND_SEND_SUCCESS:
		calobj_copy = CORBA_string_dup (new_calobj);
		g_free (new_calobj);

		return calobj_copy;

	case CAL_BACKEND_SEND_INVALID_OBJECT:
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_InvalidObject);
		break;

	case CAL_BACKEND_SEND_BUSY:
		err = GNOME_Evolution_Calendar_Cal_Busy__alloc ();
		err->errorMsg = CORBA_string_dup (error_msg);
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Calendar_Cal_Busy, err);
		break;

	case CAL_BACKEND_SEND_PERMISSION_DENIED:
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_PermissionDenied);
		break;

	default :
		g_assert_not_reached ();
	}

	return NULL;
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

	query = cal_backend_get_query (priv->backend, ql, sexp);
	if (!query) {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_CouldNotCreate);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_init (&ev2);
	query_copy = CORBA_Object_duplicate (BONOBO_OBJREF (query), &ev2);
	if (BONOBO_EX (&ev2)) {
		CORBA_exception_free (&ev2);
		g_message ("Cal_get_query(): Could not duplicate the query reference");
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_CouldNotCreate);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev2);

	return query_copy;
}

/* Cal::set_default_timezone method */
static void
impl_Cal_set_default_timezone (PortableServer_Servant servant,
			       const GNOME_Evolution_Calendar_CalTimezoneObjUID tzid,
			       CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	gboolean zone_set;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	zone_set = cal_backend_set_default_timezone (priv->backend, tzid);

	if (!zone_set) {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);
	}
}

/* Cal::get_timezone_object method */
static GNOME_Evolution_Calendar_CalObj
impl_Cal_get_timezone_object (PortableServer_Servant servant,
			      const GNOME_Evolution_Calendar_CalTimezoneObjUID tzid,
			      CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	char *calobj;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	calobj = cal_backend_get_timezone_object (priv->backend, tzid);

	if (calobj) {
		CORBA_char *calobj_copy;

		calobj_copy = CORBA_string_dup (calobj);
		g_free (calobj);
		return calobj_copy;
	} else {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_Cal_NotFound);
		return NULL;
	}
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
	if (BONOBO_EX (&ev)) {
		g_message ("cal_construct: could not duplicate the listener");
		priv->listener = CORBA_OBJECT_NIL;
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	/* obtain the WombatClient interface */
	CORBA_exception_init (&ev);
	priv->wombat_client = Bonobo_Unknown_queryInterface (
		priv->listener,
		"IDL:GNOME/Evolution/WombatClient:1.0",
		&ev);
	if (BONOBO_EX (&ev)) {
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
	bonobo_object_release_unref (priv->listener, &ev);
	if (BONOBO_EX (&ev))
		g_message ("cal_destroy(): could not release the listener");

	priv->listener = NULL;
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

	parent_class = gtk_type_class (PARENT_TYPE);

	/* Class method overrides */
	object_class->destroy = cal_destroy;

	/* Epv methods */
	epv->_get_uri = impl_Cal_get_uri;
	epv->isReadOnly = impl_Cal_is_read_only;
	epv->getCalAddress = impl_Cal_get_cal_address;
	epv->getAlarmEmailAddress = impl_Cal_get_alarm_email_address;
	epv->getLdapAttribute = impl_Cal_get_ldap_attribute;
	epv->getStaticCapabilities = impl_Cal_get_static_capabilities;
	epv->setMode = impl_Cal_set_mode;
	epv->countObjects = impl_Cal_get_n_objects;
	epv->getDefaultObject = impl_Cal_get_default_object;
	epv->getObject = impl_Cal_get_object;
	epv->setDefaultTimezone = impl_Cal_set_default_timezone;
	epv->getTimezoneObject = impl_Cal_get_timezone_object;
	epv->getUIDs = impl_Cal_get_uids;
	epv->getChanges = impl_Cal_get_changes;
	epv->getObjectsInRange = impl_Cal_get_objects_in_range;
	epv->getFreeBusy = impl_Cal_get_free_busy;
	epv->getAlarmsInRange = impl_Cal_get_alarms_in_range;
	epv->getAlarmsForObject = impl_Cal_get_alarms_for_object;
	epv->updateObjects = impl_Cal_update_objects;
	epv->removeObject = impl_Cal_remove_object;
	epv->sendObject = impl_Cal_send_object; 
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
 * cal_notify_mode:
 * @cal: A calendar client interface.
 * @status: Status of the mode set.
 * @mode: The current mode.
 * 
 * Notifys the listener of the results of a setMode call.
 **/
void
cal_notify_mode (Cal *cal,
		 GNOME_Evolution_Calendar_Listener_SetModeStatus status,
		 GNOME_Evolution_Calendar_CalMode mode)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyCalSetMode (priv->listener, status, mode, &ev);

	if (BONOBO_EX (&ev))
		g_message ("cal_notify_mode(): could not notify the listener "
			   "about a mode change");

	CORBA_exception_free (&ev);	
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

	if (BONOBO_EX (&ev))
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

	if (BONOBO_EX (&ev))
		g_message ("cal_notify_remove(): could not notify the listener "
			   "about a removed object");

	CORBA_exception_free (&ev);
}

/**
 * cal_notify_error
 * @cal: A calendar client interface.
 * @message: Error message.
 *
 * Notify a calendar client of an error occurred in the backend.
 */
void
cal_notify_error (Cal *cal, const char *message)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));
	g_return_if_fail (message != NULL);

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyErrorOccurred (priv->listener, (char *) message, &ev);

	if (BONOBO_EX (&ev))
		g_message ("cal_notify_remove(): could not notify the listener "
			   "about a removed object");

	CORBA_exception_free (&ev);
}

/**
 * cal_notify_categories_changed:
 * @cal: A calendar client interface.
 * @categories: List of categories.
 * 
 * Notifies a listener attached to a calendar client interface object about the
 * current set of categories in a calendar backend.
 **/
void
cal_notify_categories_changed (Cal *cal, GNOME_Evolution_Calendar_StringSeq *categories)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));
	g_return_if_fail (categories != NULL);

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyCategoriesChanged (priv->listener, categories, &ev);

	if (BONOBO_EX (&ev))
		g_message ("cal_notify_categories_changed(): Could not notify the listener "
			   "about the current set of categories");

	CORBA_exception_free (&ev);
}

/**
 * cal_get_password:
 * @cal: A calendar client interface.
 * @prompt: The message to show to the user when asking for the password.
 * @key: A key associated with the password being asked.
 *
 * Gets a password from the calendar client this Cal knows about. It does
 * so by using the WombatClient interface being used by the corresponding
 * CalClient.
 *
 * Returns: a password entered by the user.
 */
char *
cal_get_password (Cal *cal, const char *prompt, const char *key)
{
	CalPrivate *priv;
	CORBA_Environment ev;
	CORBA_char *pwd;

	g_return_val_if_fail (cal != NULL, NULL);
	g_return_val_if_fail (IS_CAL (cal), NULL);

	priv = cal->priv;
	g_return_val_if_fail (priv->wombat_client != CORBA_OBJECT_NIL, NULL);

	CORBA_exception_init (&ev);
	pwd = GNOME_Evolution_WombatClient_getPassword (
		priv->wombat_client,
		(const CORBA_char *) prompt,
		(const CORBA_char *) key,
		&ev);
	if (BONOBO_EX (&ev)) {
		g_message ("cal_get_password: could not get password from associated WombatClient");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return pwd;
}

/**
 * cal_forget_password:
 * @cal: A calendar client interface.
 * @key: A key associated with the password to be forgotten.
 *
 * Notifies the associated calendar client that it should forget
 * about the password identified by @key, so that next time the backend
 * asks the client about it, the client would ask again the user for it.
 * This is done in cases where the password supplied the first time
 * was not a valid password and the backend needs the user to enter
 * a new one.
 */
void
cal_forget_password (Cal *cal, const char *key)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->wombat_client != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_WombatClient_forgetPassword (
		priv->wombat_client,
		(const CORBA_char *) key,
		&ev);

	if (BONOBO_EX (&ev)) {
		g_message ("cal_forget_password: could not notify WombatClient about "
			   "password to be forgotten");
	}

	CORBA_exception_free (&ev);
}
