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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libical/ical.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-exception.h>
#include "cal-backend.h"
#include "cal.h"

#define PARENT_TYPE         BONOBO_TYPE_OBJECT

static BonoboObjectClass *parent_class;

/* Private part of the Cal structure */
struct _CalPrivate {
	/* Our backend */
	CalBackend *backend;

	/* Listener on the client we notify */
	GNOME_Evolution_Calendar_Listener listener;
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

static void
impl_Cal_open (PortableServer_Servant servant,
	       CORBA_boolean only_if_exists,
	       CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	
	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_open (priv->backend, cal, only_if_exists);
}

static void
impl_Cal_remove (PortableServer_Servant servant,
		 CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	
	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_remove (priv->backend, cal);
}

/* Cal::isReadOnly method */
static void
impl_Cal_isReadOnly (PortableServer_Servant servant,
		     CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_is_read_only (priv->backend, cal);
}
		       
/* Cal::getEmailAddress method */
static void
impl_Cal_getCalAddress (PortableServer_Servant servant,
			CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_get_cal_address (priv->backend, cal);
}
		       
/* Cal::get_alarm_email_address method */
static void
impl_Cal_getAlarmEmailAddress (PortableServer_Servant servant,
			       CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_get_alarm_email_address (priv->backend, cal);
}
		       
/* Cal::get_ldap_attribute method */
static void
impl_Cal_getLdapAttribute (PortableServer_Servant servant,
			   CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_get_ldap_attribute (priv->backend, cal);
}

/* Cal::getSchedulingInformation method */
static void
impl_Cal_getStaticCapabilities (PortableServer_Servant servant,
				CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_get_static_capabilities (priv->backend, cal);
}

/* Cal::setMode method */
static void
impl_Cal_setMode (PortableServer_Servant servant,
		  GNOME_Evolution_Calendar_CalMode mode,
		  CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	
	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_set_mode (priv->backend, mode);
}

static void
impl_Cal_getDefaultObject (PortableServer_Servant servant,
			   CORBA_Environment *ev)
{
 	Cal *cal;
 	CalPrivate *priv;
 
 	cal = CAL (bonobo_object_from_servant (servant));
 	priv = cal->priv;
 
 	cal_backend_get_default_object (priv->backend, cal);
}

/* Cal::getObject method */
static void
impl_Cal_getObject (PortableServer_Servant servant,
		    const CORBA_char *uid,
		    const CORBA_char *rid,
		    CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_get_object (priv->backend, cal, uid, rid);
}

/* Cal::getObjectsInRange method */
static void
impl_Cal_getObjectList (PortableServer_Servant servant,
			const CORBA_char *query,
			CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	
	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_get_object_list (priv->backend, cal, query);
}

/* Cal::getChanges method */
static void
impl_Cal_getChanges (PortableServer_Servant servant,
		     GNOME_Evolution_Calendar_CalObjType type,
		     const CORBA_char *change_id,
		     CORBA_Environment *ev)
{
       Cal *cal;
       CalPrivate *priv;

       cal = CAL (bonobo_object_from_servant (servant));
       priv = cal->priv;

       cal_backend_get_changes (priv->backend, cal, type, change_id);
}

/* Cal::getFreeBusy method */
static void
impl_Cal_getFreeBusy (PortableServer_Servant servant,
		      const GNOME_Evolution_Calendar_UserList *user_list,
		      const GNOME_Evolution_Calendar_Time_t start,
		      const GNOME_Evolution_Calendar_Time_t end,
		      CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	GList *users = NULL;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	/* convert the CORBA user list to a GList */
	if (user_list) {
		int i;

		for (i = 0; i < user_list->_length; i++)
			users = g_list_append (users, user_list->_buffer[i]);
	}

	/* call the backend's get_free_busy method */
	cal_backend_get_free_busy (priv->backend, cal, users, start, end);
}

/* Cal::discardAlarm method */
static void
impl_Cal_discardAlarm (PortableServer_Servant servant,
		       const CORBA_char *uid,
		       const CORBA_char *auid,
		       CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_discard_alarm (priv->backend, cal, uid, auid);
}

static void
impl_Cal_createObject (PortableServer_Servant servant,
		       const CORBA_char *calobj,
		       CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_create_object (priv->backend, cal, calobj);
}

static void
impl_Cal_modifyObject (PortableServer_Servant servant,
		       const CORBA_char *calobj,
		       const GNOME_Evolution_Calendar_CalObjModType mod,
		       CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_modify_object (priv->backend, cal, calobj, mod);
}

/* Cal::removeObject method */
static void
impl_Cal_removeObject (PortableServer_Servant servant,
		       const CORBA_char *uid,
		       const CORBA_char *rid,
		       const GNOME_Evolution_Calendar_CalObjModType mod,
		       CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_remove_object (priv->backend, cal, uid, rid, mod);
}

static void
impl_Cal_receiveObjects (PortableServer_Servant servant, const CORBA_char *calobj, CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_receive_objects (priv->backend, cal, calobj);
}

static void
impl_Cal_sendObjects (PortableServer_Servant servant, const CORBA_char *calobj, CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_send_objects (priv->backend, cal, calobj);
}

/* Cal::getQuery implementation */
static void
impl_Cal_getQuery (PortableServer_Servant servant,
		   const CORBA_char *sexp,
		   GNOME_Evolution_Calendar_QueryListener ql,
		   CORBA_Environment *ev)
{

	Cal *cal;
	CalPrivate *priv;
	Query *query;
	CalBackendObjectSExp *obj_sexp;
	
	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	/* we handle this entirely here, since it doesn't require any
	   backend involvement now that we have pas_book_view_start to
	   actually kick off the search. */

	obj_sexp = cal_backend_object_sexp_new (sexp);
	if (!obj_sexp) {
		cal_notify_query (cal, GNOME_Evolution_Calendar_InvalidQuery, NULL);

		return;
	}

	query = query_new (priv->backend, ql, obj_sexp);
	if (!query) {
		g_object_unref (obj_sexp);
		cal_notify_query (cal, GNOME_Evolution_Calendar_OtherError, NULL);

		return;
	}

	cal_backend_add_query (priv->backend, query);

	cal_notify_query (cal, GNOME_Evolution_Calendar_Success, query);

	g_object_unref (query);
}


/* Cal::getTimezone method */
static void
impl_Cal_getTimezone (PortableServer_Servant servant,
		      const CORBA_char *tzid,
		      CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_get_timezone (priv->backend, cal, tzid);
}

/* Cal::addTimezone method */
static void
impl_Cal_addTimezone (PortableServer_Servant servant,
		      const CORBA_char *tz,
		      CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_add_timezone (priv->backend, cal, tz);
}

/* Cal::setDefaultTimezone method */
static void
impl_Cal_setDefaultTimezone (PortableServer_Servant servant,
			     const CORBA_char *tzid,
			     CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	cal_backend_set_default_timezone (priv->backend, cal, tzid);
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
cal_new (CalBackend *backend, const char *uri, GNOME_Evolution_Calendar_Listener listener)
{
	Cal *cal, *retval;

	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	cal = CAL (g_object_new (CAL_TYPE, 
				 "poa", bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL),
				 NULL));

	retval = cal_construct (cal, backend, listener);
	if (!retval) {
		g_message (G_STRLOC ": could not construct the calendar client interface");
		bonobo_object_unref (BONOBO_OBJECT (cal));
		return NULL;
	}

	return retval;
}

CalBackend *
cal_get_backend (Cal *cal)
{
	g_return_val_if_fail (cal != NULL, NULL);
	g_return_val_if_fail (IS_CAL (cal), NULL);

	return cal->priv->backend;
}

GNOME_Evolution_Calendar_Listener
cal_get_listener (Cal *cal)
{
	g_return_val_if_fail (cal != NULL, NULL);
	g_return_val_if_fail (IS_CAL (cal), NULL);

	return cal->priv->listener;
}

/* Destroy handler for the calendar */
static void
cal_finalize (GObject *object)
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
		g_message (G_STRLOC ": could not release the listener");

	priv->listener = NULL;
	CORBA_exception_free (&ev);

	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* Class initialization function for the calendar */
static void
cal_class_init (CalClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	POA_GNOME_Evolution_Calendar_Cal__epv *epv = &klass->epv;

	parent_class = g_type_class_peek_parent (klass);

	/* Class method overrides */
	object_class->finalize = cal_finalize;

	/* Epv methods */
	epv->_get_uri = impl_Cal_get_uri;
	epv->open = impl_Cal_open;
	epv->remove = impl_Cal_remove;
	epv->isReadOnly = impl_Cal_isReadOnly;
	epv->getCalAddress = impl_Cal_getCalAddress;
 	epv->getAlarmEmailAddress = impl_Cal_getAlarmEmailAddress;
 	epv->getLdapAttribute = impl_Cal_getLdapAttribute;
 	epv->getStaticCapabilities = impl_Cal_getStaticCapabilities;
	epv->setMode = impl_Cal_setMode;
	epv->getDefaultObject = impl_Cal_getDefaultObject;
	epv->getObject = impl_Cal_getObject;
	epv->getTimezone = impl_Cal_getTimezone;
	epv->addTimezone = impl_Cal_addTimezone;
	epv->setDefaultTimezone = impl_Cal_setDefaultTimezone;
	epv->getObjectList = impl_Cal_getObjectList;
	epv->getChanges = impl_Cal_getChanges;
	epv->getFreeBusy = impl_Cal_getFreeBusy;
	epv->discardAlarm = impl_Cal_discardAlarm;
	epv->createObject = impl_Cal_createObject;
	epv->modifyObject = impl_Cal_modifyObject;
	epv->removeObject = impl_Cal_removeObject;
	epv->receiveObjects = impl_Cal_receiveObjects;
	epv->sendObjects = impl_Cal_sendObjects;
	epv->getQuery = impl_Cal_getQuery;
}


/* Object initialization function for the calendar */
static void
cal_init (Cal *cal, CalClass *klass)
{
	CalPrivate *priv;

	priv = g_new0 (CalPrivate, 1);
	cal->priv = priv;

	priv->listener = CORBA_OBJECT_NIL;
}

BONOBO_TYPE_FUNC_FULL (Cal, GNOME_Evolution_Calendar_Cal, PARENT_TYPE, cal);

void 
cal_notify_read_only (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, gboolean read_only)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyReadOnly (priv->listener, status, read_only, &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of read only");

	CORBA_exception_free (&ev);	
}

void 
cal_notify_cal_address (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *address)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyCalAddress (priv->listener, status, address ? address : "", &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of cal address");

	CORBA_exception_free (&ev);	
}

void
cal_notify_alarm_email_address (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *address)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyAlarmEmailAddress (priv->listener, status, address ? address : "", &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of alarm address");

	CORBA_exception_free (&ev);
}

void
cal_notify_ldap_attribute (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *attribute)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyLDAPAttribute (priv->listener, status, attribute ? attribute : "", &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of ldap attribute");

	CORBA_exception_free (&ev);
}

void
cal_notify_static_capabilities (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *capabilities)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyStaticCapabilities (priv->listener, status,
								    capabilities ? capabilities : "", &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of static capabilities");

	CORBA_exception_free (&ev);
}

void 
cal_notify_open (Cal *cal, GNOME_Evolution_Calendar_CallStatus status)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyCalOpened (priv->listener, status, &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of open");

	CORBA_exception_free (&ev);
}

void
cal_notify_remove (Cal *cal, GNOME_Evolution_Calendar_CallStatus status)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyCalRemoved (priv->listener, status, &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of remove");

	CORBA_exception_free (&ev);
}

void
cal_notify_object_created (Cal *cal, GNOME_Evolution_Calendar_CallStatus status,
			   const char *uid, const char *object)
{
	CalPrivate *priv;
	EList *queries;
	EIterator *iter;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	queries = cal_backend_get_queries (priv->backend);
	iter = e_list_get_iterator (queries);

	while (e_iterator_is_valid (iter)) {
		Query *query = QUERY (e_iterator_get (iter));
		
		bonobo_object_dup_ref (BONOBO_OBJREF (query), NULL);
		
		if (!query_object_matches (query, object))
			continue;
		
		query_notify_objects_added_1 (query, object);

		bonobo_object_release_unref (BONOBO_OBJREF (query), NULL);

		e_iterator_next (iter);
	}
	g_object_unref (iter);
	g_object_unref (queries);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyObjectCreated (priv->listener, status, uid ? uid : "", &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of object creation");

	CORBA_exception_free (&ev);
}

void
cal_notify_object_modified (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, 
			    const char *old_object, const char *object)
{
	CalPrivate *priv;
	EList *queries;
	EIterator *iter;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	queries = cal_backend_get_queries (priv->backend);
	iter = e_list_get_iterator (queries);

	while (object && old_object && e_iterator_is_valid (iter)) {
		Query *query = QUERY (e_iterator_get (iter));
		gboolean old_match, new_match;
		
		bonobo_object_dup_ref (BONOBO_OBJREF (query), NULL);

		old_match = query_object_matches (query, old_object);
		new_match = query_object_matches (query, object);
		if (old_match && new_match)
			query_notify_objects_modified_1 (query, object);
		else if (new_match)
			query_notify_objects_added_1 (query, object);
		else /* if (old_match) */ {
			icalcomponent *comp;

			comp = icalcomponent_new_from_string ((char *)old_object);
			query_notify_objects_removed_1 (query, icalcomponent_get_uid (comp));
			icalcomponent_free (comp);
		}
		query_notify_query_done (query, GNOME_Evolution_Calendar_Success);

		bonobo_object_release_unref (BONOBO_OBJREF (query), NULL);

		e_iterator_next (iter);
	}
	g_object_unref (iter);
	g_object_unref (queries);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyObjectModified (priv->listener, status, &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of object creation");

	CORBA_exception_free (&ev);
}

void
cal_notify_object_removed (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, 
			   const char *uid, const char *object)
{
	CalPrivate *priv;
	EList *queries;
	EIterator *iter;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	queries = cal_backend_get_queries (priv->backend);
	iter = e_list_get_iterator (queries);

	while (uid && object && e_iterator_is_valid (iter)) {
		Query *query = QUERY (e_iterator_get (iter));

		bonobo_object_dup_ref (BONOBO_OBJREF (query), NULL);

		if (!query_object_matches (query, object))
			continue;

		query_notify_objects_removed_1 (query, uid);

		bonobo_object_release_unref (BONOBO_OBJREF (query), NULL);

		e_iterator_next (iter);
	}
	g_object_unref (iter);
	g_object_unref (queries);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyObjectRemoved (priv->listener, status, &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of object removal");

	CORBA_exception_free (&ev);
}

void
cal_notify_objects_received (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, 
			     GList *created, GList *modified, GList *removed)
{
	CalPrivate *priv;
	EList *queries;
	EIterator *iter;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	queries = cal_backend_get_queries (priv->backend);
	iter = e_list_get_iterator (queries);

	while (e_iterator_is_valid (iter)) {
		Query *query = QUERY (e_iterator_get (iter));

		bonobo_object_dup_ref (BONOBO_OBJREF (query), NULL);

		query_notify_objects_added (query, created);
		query_notify_objects_modified (query, modified);
		query_notify_objects_removed (query, removed);

		bonobo_object_release_unref (BONOBO_OBJREF (query), NULL);

		e_iterator_next (iter);
	}
	g_object_unref (iter);
	g_object_unref (queries);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyObjectsReceived (priv->listener, status, &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of objects received");

	CORBA_exception_free (&ev);
}

void
cal_notify_alarm_discarded (Cal *cal, GNOME_Evolution_Calendar_CallStatus status)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyAlarmDiscarded (priv->listener, status, &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of alarm discarded");

	CORBA_exception_free (&ev);	
}

void
cal_notify_objects_sent (Cal *cal, GNOME_Evolution_Calendar_CallStatus status)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyObjectsSent (priv->listener, status, &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of objects sent");

	CORBA_exception_free (&ev);	
}

void
cal_notify_default_object (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, char *object)
{
	CalPrivate *priv;
	CORBA_Environment ev;
	
	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	GNOME_Evolution_Calendar_Listener_notifyDefaultObjectRequested (priv->listener, status, 
									object ? object : "", &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of default object");

	CORBA_exception_free (&ev);
}

void
cal_notify_object (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, char *object)
{
	CalPrivate *priv;
	CORBA_Environment ev;
	
	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	GNOME_Evolution_Calendar_Listener_notifyObjectRequested (priv->listener, status,
								 object ? object : "", &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of object");

	CORBA_exception_free (&ev);
}

void
cal_notify_object_list (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, GList *objects)
{
	CalPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_stringlist seq;
	GList *l;
	int i;
	
	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	seq._maximum = g_list_length (objects);
	seq._length = 0;
	seq._buffer = GNOME_Evolution_Calendar_stringlist_allocbuf (seq._maximum);

	for (l = objects, i = 0; l; l = l->next, i++) {
		seq._buffer[i] = CORBA_string_dup (l->data);
		seq._length++;
	}

	GNOME_Evolution_Calendar_Listener_notifyObjectListRequested (priv->listener, status, &seq, &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of object list");

	CORBA_exception_free (&ev);	

	CORBA_free(seq._buffer);
}

void
cal_notify_query (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, Query *query)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyQuery (priv->listener, status, BONOBO_OBJREF (query), &ev);

	if (BONOBO_EX (&ev))
		g_message (G_STRLOC ": could not notify the listener of query");

	CORBA_exception_free (&ev);	
}

void
cal_notify_timezone_requested (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *object)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyTimezoneRequested (priv->listener, status, object ? object : "", &ev);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of timezone requested");

	CORBA_exception_free (&ev);
}

void
cal_notify_timezone_added (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *tzid)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyTimezoneAdded (priv->listener, status, tzid ? tzid : "", &ev);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of timezone added");

	CORBA_exception_free (&ev);
}

void
cal_notify_default_timezone_set (Cal *cal, GNOME_Evolution_Calendar_CallStatus status)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyDefaultTimezoneSet (priv->listener, status, &ev);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of default timezone set");

	CORBA_exception_free (&ev);
}

void
cal_notify_changes (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, 
		    GList *adds, GList *modifies, GList *deletes)
{
	CalPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalObjChangeSeq seq;
	GList *l;	
	int n, i;

	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	n = g_list_length (adds) + g_list_length (modifies) + g_list_length (deletes);
	seq._maximum = n;
	seq._length = n;
	seq._buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalObjChange_allocbuf (n);

	i = 0;
	for (l = adds; l; i++, l = l->next) {
		GNOME_Evolution_Calendar_CalObjChange *change = &seq._buffer[i];
		
		change->calobj = CORBA_string_dup (l->data);
		change->type = GNOME_Evolution_Calendar_ADDED;
	}

	for (l = modifies; l; i++, l = l->next) {
		GNOME_Evolution_Calendar_CalObjChange *change = &seq._buffer[i];

		change->calobj = CORBA_string_dup (l->data);
		change->type = GNOME_Evolution_Calendar_MODIFIED;
	}

	for (l = deletes; l; i++, l = l->next) {
		GNOME_Evolution_Calendar_CalObjChange *change = &seq._buffer[i];

		change->calobj = CORBA_string_dup (l->data);
		change->type = GNOME_Evolution_Calendar_DELETED;
	}
	
	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyDefaultTimezoneSet (priv->listener, status, &ev);

	CORBA_free (seq._buffer);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of default timezone set");

	CORBA_exception_free (&ev);
}

void
cal_notify_free_busy (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, GList *freebusy)
{
	CalPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalObjSeq seq;
	GList *l;
	int n, i;
	
	g_return_if_fail (IS_CAL (cal));

	priv = cal->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	n = g_list_length (freebusy);
	seq._maximum = n;
	seq._length = n;
	seq._buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalObj_allocbuf (n);

	for (i = 0, l = freebusy; l; i++, l = l->next)
		seq._buffer[i] = CORBA_string_dup (l->data);
	
	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyDefaultTimezoneSet (priv->listener, status, &ev);

	CORBA_free (seq._buffer);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of freebusy");

	CORBA_exception_free (&ev);
}

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
