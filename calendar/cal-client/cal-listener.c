/* Evolution calendar listener
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#include <bonobo/bonobo-main.h>
#include "cal-marshal.h"
#include "cal-listener.h"



/* Private part of the CalListener structure */
struct CalListenerPrivate {
	/* Notification functions and their closure data */
	CalListenerCalSetModeFn cal_set_mode_fn;
	CalListenerErrorOccurredFn error_occurred_fn;
	CalListenerCategoriesChangedFn categories_changed_fn;
	gpointer fn_data;

	/* Whether notification is desired */
	gboolean notify : 1;
};

/* Signal IDs */
enum {
	READ_ONLY,
	CAL_ADDRESS,
	ALARM_ADDRESS,
	LDAP_ATTRIBUTE,
	STATIC_CAPABILITIES,
	OPEN,
	REMOVE,
	CREATE_OBJECT,
	MODIFY_OBJECT,
	REMOVE_OBJECT,
	DISCARD_ALARM,
	RECEIVE_OBJECTS,
	SEND_OBJECTS,
	DEFAULT_OBJECT,
	OBJECT,
	OBJECT_LIST,
	GET_TIMEZONE,
	ADD_TIMEZONE,
	SET_DEFAULT_TIMEZONE,
	GET_CHANGES,
	GET_FREE_BUSY,
	QUERY,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static BonoboObjectClass *parent_class;

static ECalendarStatus
convert_status (const GNOME_Evolution_Calendar_CallStatus status)
{
	switch (status) {
	case GNOME_Evolution_Calendar_Success:
		return E_CALENDAR_STATUS_OK;
	case GNOME_Evolution_Calendar_RepositoryOffline:
		return E_CALENDAR_STATUS_REPOSITORY_OFFLINE;
	case GNOME_Evolution_Calendar_PermissionDenied:
		return E_CALENDAR_STATUS_PERMISSION_DENIED;
	case GNOME_Evolution_Calendar_ObjectNotFound:
		return E_CALENDAR_STATUS_OBJECT_NOT_FOUND;
	case GNOME_Evolution_Calendar_InvalidObject:
		return E_CALENDAR_STATUS_INVALID_OBJECT;
	case GNOME_Evolution_Calendar_CardIdAlreadyExists:
		return E_CALENDAR_STATUS_CARD_ID_ALREADY_EXISTS;
	case GNOME_Evolution_Calendar_AuthenticationFailed:
		return E_CALENDAR_STATUS_AUTHENTICATION_FAILED;
	case GNOME_Evolution_Calendar_AuthenticationRequired:
		return E_CALENDAR_STATUS_AUTHENTICATION_REQUIRED;
	case GNOME_Evolution_Calendar_OtherError:
	default:
		return E_CALENDAR_STATUS_OTHER_ERROR;
	}
}

static void
impl_notifyReadOnly (PortableServer_Servant servant,
		     GNOME_Evolution_Calendar_CallStatus status,
		     const CORBA_boolean read_only,
		     CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[READ_ONLY], 0, convert_status (status), read_only);
}

static void
impl_notifyCalAddress (PortableServer_Servant servant,
		       GNOME_Evolution_Calendar_CallStatus status,
		       const CORBA_char *address,
		       CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[CAL_ADDRESS], 0, convert_status (status), address);
}

static void
impl_notifyAlarmEmailAddress (PortableServer_Servant servant,
			      GNOME_Evolution_Calendar_CallStatus status,
			      const CORBA_char *address,
			      CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[ALARM_ADDRESS], 0, convert_status (status), address);
}

static void
impl_notifyLDAPAttribute (PortableServer_Servant servant,
			  GNOME_Evolution_Calendar_CallStatus status,
			  const CORBA_char *ldap_attribute,
			  CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[LDAP_ATTRIBUTE], 0, convert_status (status), ldap_attribute);
}

static void
impl_notifyStaticCapabilities (PortableServer_Servant servant,
			       GNOME_Evolution_Calendar_CallStatus status,
			       const CORBA_char *capabilities,
			       CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[STATIC_CAPABILITIES], 0, convert_status (status));
}

/* ::notifyCalOpened method */
static void
impl_notifyCalOpened (PortableServer_Servant servant,
		      GNOME_Evolution_Calendar_CallStatus status,
		      CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[OPEN], 0, convert_status (status));
}

static void
impl_notifyCalRemoved (PortableServer_Servant servant,
		      GNOME_Evolution_Calendar_CallStatus status,
		      CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[REMOVE], 0, convert_status (status));
}

static void
impl_notifyObjectCreated (PortableServer_Servant servant,
			  GNOME_Evolution_Calendar_CallStatus status,
			  const CORBA_char *uid,
			  CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[CREATE_OBJECT], 0, convert_status (status), uid);
}

static void
impl_notifyObjectModified (PortableServer_Servant servant,
			   GNOME_Evolution_Calendar_CallStatus status,
			   CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[MODIFY_OBJECT], 0, convert_status (status));
}

static void
impl_notifyObjectRemoved (PortableServer_Servant servant,
			  GNOME_Evolution_Calendar_CallStatus status,
			  CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[REMOVE_OBJECT], 0, convert_status (status));
}

static void
impl_notifyAlarmDiscarded (PortableServer_Servant servant,
			   GNOME_Evolution_Calendar_CallStatus status,
			   CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[DISCARD_ALARM], 0, convert_status (status));
}

static void
impl_notifyObjectsReceived (PortableServer_Servant servant,
			    GNOME_Evolution_Calendar_CallStatus status,
			    CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[RECEIVE_OBJECTS], 0, convert_status (status));
}

static void
impl_notifyObjectsSent (PortableServer_Servant servant,
			GNOME_Evolution_Calendar_CallStatus status,
			CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[SEND_OBJECTS], 0, convert_status (status));
}

static void 
impl_notifyDefaultObjectRequested (PortableServer_Servant servant,
				   const GNOME_Evolution_Calendar_CallStatus status,
				   const CORBA_char *object,
				   CORBA_Environment *ev) 
{
	CalListener *listener;
	CalListenerPrivate *priv;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[DEFAULT_OBJECT], 0, convert_status (status), object);
}

static void 
impl_notifyObjectRequested (PortableServer_Servant servant,
			    const GNOME_Evolution_Calendar_CallStatus status,
			    const CORBA_char *object,
			    CORBA_Environment *ev) 
{
	CalListener *listener;
	CalListenerPrivate *priv;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_signal_emit (G_OBJECT (listener), signals[OBJECT], 0, convert_status (status), object);
}

static GList *
build_object_list (const GNOME_Evolution_Calendar_stringlist *seq)
{
	GList *list;
	int i;

	list = NULL;
	for (i = 0; i < seq->_length; i++) {
		icalcomponent *comp;
		
		comp = icalcomponent_new_from_string (seq->_buffer[i]);
		if (!comp)
			continue;
		
		list = g_list_prepend (list, comp);
	}

	return list;
}

static void 
impl_notifyObjectListRequested (PortableServer_Servant servant,
				const GNOME_Evolution_Calendar_CallStatus status,
				const GNOME_Evolution_Calendar_stringlist *objects,
				CORBA_Environment *ev) 
{
	CalListener *listener;
	CalListenerPrivate *priv;
	GList *object_list, *l;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	object_list = build_object_list (objects);
	
	g_signal_emit (G_OBJECT (listener), signals[OBJECT_LIST], 0, convert_status (status), object_list);

	for (l = object_list; l; l = l->next)
		icalcomponent_free (l->data);
	g_list_free (object_list);
}

static void
impl_notifyTimezoneRequested (PortableServer_Servant servant,
			      const GNOME_Evolution_Calendar_CallStatus status,
			      const CORBA_char *object,
			      CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;
	
	g_signal_emit (G_OBJECT (listener), signals[GET_TIMEZONE], 0, convert_status (status), object);
}

static void
impl_notifyTimezoneAdded (PortableServer_Servant servant,
			  const GNOME_Evolution_Calendar_CallStatus status,
			  const CORBA_char *tzid,
			  CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;
	
	g_signal_emit (G_OBJECT (listener), signals[ADD_TIMEZONE], 0, convert_status (status), tzid);
}

static void
impl_notifyDefaultTimezoneSet (PortableServer_Servant servant,
			       const GNOME_Evolution_Calendar_CallStatus status,
			       CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;
	
	g_signal_emit (G_OBJECT (listener), signals[SET_DEFAULT_TIMEZONE], 0, convert_status (status));
}

static GList *
build_change_list (const GNOME_Evolution_Calendar_CalObjChangeSeq *seq)
{
	GList *list = NULL;
	icalcomponent *icalcomp;
	int i;

	/* Create the list in reverse order */
	for (i = 0; i < seq->_length; i++) {
		GNOME_Evolution_Calendar_CalObjChange *corba_coc;
		CalClientChange *ccc;

		corba_coc = &seq->_buffer[i];
		ccc = g_new (CalClientChange, 1);

		icalcomp = icalparser_parse_string (corba_coc->calobj);
		if (!icalcomp)
			continue;

		ccc->comp = cal_component_new ();
		if (!cal_component_set_icalcomponent (ccc->comp, icalcomp)) {
			icalcomponent_free (icalcomp);
			g_object_unref (G_OBJECT (ccc->comp));
			continue;
		}
		ccc->type = corba_coc->type;

		list = g_list_prepend (list, ccc);
	}

	list = g_list_reverse (list);

	return list;
}

static void
impl_notifyChanges (PortableServer_Servant servant,
		    const GNOME_Evolution_Calendar_CallStatus status,
		    const GNOME_Evolution_Calendar_CalObjChangeSeq *seq,
		    CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	GList *changes, *l;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;
	
	changes = build_change_list (seq);
	
	g_signal_emit (G_OBJECT (listener), signals[GET_CHANGES], 0, convert_status (status), changes);

	for (l = changes; l; l = l->next)
		g_free (l->data);
	g_list_free (changes);
}

static GList *
build_free_busy_list (const GNOME_Evolution_Calendar_CalObjSeq *seq)
{
	GList *list = NULL;
	int i;

	/* Create the list in reverse order */
	for (i = 0; i < seq->_length; i++) {
		CalComponent *comp;
		icalcomponent *icalcomp;
		icalcomponent_kind kind;

		icalcomp = icalcomponent_new_from_string (seq->_buffer[i]);
		if (!icalcomp)
			continue;

		kind = icalcomponent_isa (icalcomp);
		if (kind == ICAL_VFREEBUSY_COMPONENT) {
			comp = cal_component_new ();
			if (!cal_component_set_icalcomponent (comp, icalcomp)) {
				icalcomponent_free (icalcomp);
				g_object_unref (G_OBJECT (comp));
				continue;
			}
			
			list = g_list_append (list, comp);
		} else {
			icalcomponent_free (icalcomp);
		}
	}

	return list;
}

static void
impl_notifyFreeBusy (PortableServer_Servant servant,
		     const GNOME_Evolution_Calendar_CallStatus status,
		     const GNOME_Evolution_Calendar_CalObjSeq *seq,
		     CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	GList *freebusy, *l;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;
	
	freebusy = build_free_busy_list (seq);
	
	g_signal_emit (G_OBJECT (listener), signals[GET_FREE_BUSY], 0, convert_status (status), freebusy);

	for (l = freebusy; l; l = l->next)
		g_free (l->data);
	g_list_free (freebusy);
}

static void 
impl_notifyQuery (PortableServer_Servant servant,
		  const GNOME_Evolution_Calendar_CallStatus status,
		  const GNOME_Evolution_Calendar_Query query,
		  CORBA_Environment *ev) 
{
	CalListener *listener;
	CalListenerPrivate *priv;
	
	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;
	
	g_signal_emit (G_OBJECT (listener), signals[QUERY], 0, convert_status (status), query);
}

/* ::notifyCalSetMode method */
static void
impl_notifyCalSetMode (PortableServer_Servant servant,
		       GNOME_Evolution_Calendar_Listener_SetModeStatus status,
		       GNOME_Evolution_Calendar_CalMode mode,
		       CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_message ("notify_set_mode");

	g_assert (priv->cal_set_mode_fn != NULL);
	(* priv->cal_set_mode_fn) (listener, status, mode, priv->fn_data);
}


/* ::notifyErrorOccurred method */
static void
impl_notifyErrorOccurred (PortableServer_Servant servant,
			  const CORBA_char *message,
			  CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_message ("notify_error");

	g_assert (priv->error_occurred_fn != NULL);
	(* priv->error_occurred_fn) (listener, message, priv->fn_data);
}

/* ::notifyCategoriesChanged method */
static void
impl_notifyCategoriesChanged (PortableServer_Servant servant,
			      const GNOME_Evolution_Calendar_StringSeq *categories,
			      CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (!priv->notify)
		return;

	g_message ("notify_categories");

	g_assert (priv->categories_changed_fn != NULL);
	(* priv->categories_changed_fn) (listener, categories, priv->fn_data);
}



/* Object initialization function for the calendar listener */
static void
cal_listener_init (CalListener *listener, CalListenerClass *klass)
{
	CalListenerPrivate *priv;

	priv = g_new0 (CalListenerPrivate, 1);
	listener->priv = priv;

	priv->error_occurred_fn = NULL;
	priv->categories_changed_fn = NULL;

	priv->notify = TRUE;
}

/* Finalize handler for the calendar listener */
static void
cal_listener_finalize (GObject *object)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_LISTENER (object));

	listener = CAL_LISTENER (object);
	priv = listener->priv;

	priv->error_occurred_fn = NULL;
	priv->categories_changed_fn = NULL;
	priv->fn_data = NULL;

	priv->notify = FALSE;

	g_free (priv);
	listener->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Class initialization function for the calendar listener */
static void
cal_listener_class_init (CalListenerClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	klass->epv.notifyReadOnly = impl_notifyReadOnly;
	klass->epv.notifyCalAddress = impl_notifyCalAddress;
	klass->epv.notifyAlarmEmailAddress = impl_notifyAlarmEmailAddress;
	klass->epv.notifyLDAPAttribute = impl_notifyLDAPAttribute;
	klass->epv.notifyStaticCapabilities = impl_notifyStaticCapabilities;
	klass->epv.notifyCalOpened = impl_notifyCalOpened;
	klass->epv.notifyCalRemoved = impl_notifyCalRemoved;
	klass->epv.notifyObjectCreated = impl_notifyObjectCreated;
	klass->epv.notifyObjectModified = impl_notifyObjectModified;
	klass->epv.notifyObjectRemoved = impl_notifyObjectRemoved;
	klass->epv.notifyAlarmDiscarded = impl_notifyAlarmDiscarded;
	klass->epv.notifyObjectsReceived = impl_notifyObjectsReceived;
	klass->epv.notifyObjectsSent = impl_notifyObjectsSent;
	klass->epv.notifyDefaultObjectRequested = impl_notifyDefaultObjectRequested;
	klass->epv.notifyObjectRequested = impl_notifyObjectRequested;
	klass->epv.notifyObjectListRequested = impl_notifyObjectListRequested;
	klass->epv.notifyTimezoneRequested = impl_notifyTimezoneRequested;
	klass->epv.notifyTimezoneAdded = impl_notifyTimezoneAdded;
	klass->epv.notifyDefaultTimezoneSet = impl_notifyDefaultTimezoneSet;
	klass->epv.notifyChanges = impl_notifyChanges;
	klass->epv.notifyFreeBusy = impl_notifyFreeBusy;
	klass->epv.notifyQuery = impl_notifyQuery;
	klass->epv.notifyCalSetMode = impl_notifyCalSetMode;
	klass->epv.notifyErrorOccurred = impl_notifyErrorOccurred;
	klass->epv.notifyCategoriesChanged = impl_notifyCategoriesChanged;

	object_class->finalize = cal_listener_finalize;

	signals[READ_ONLY] =
		g_signal_new ("read_only",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, read_only),
			      NULL, NULL,
			      cal_marshal_VOID__INT_BOOLEAN,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_BOOLEAN);
	signals[CAL_ADDRESS] =
		g_signal_new ("cal_address",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, cal_address),
			      NULL, NULL,
			      cal_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	signals[ALARM_ADDRESS] =
		g_signal_new ("alarm_address",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, alarm_address),
			      NULL, NULL,
			      cal_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	signals[LDAP_ATTRIBUTE] =
		g_signal_new ("ldap_attribute",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, ldap_attribute),
			      NULL, NULL,
			      cal_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);	
	signals[STATIC_CAPABILITIES] =
		g_signal_new ("static_capabilities",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, static_capabilities),
			      NULL, NULL,
			      cal_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	signals[OPEN] =
		g_signal_new ("open",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, open),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);	
	signals[REMOVE] =
		g_signal_new ("remove",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, remove),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals[CREATE_OBJECT] =
		g_signal_new ("create_object",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, create_object),
			      NULL, NULL,
			      cal_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	signals[MODIFY_OBJECT] =
		g_signal_new ("modify_object",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, modify_object),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);	
	signals[REMOVE_OBJECT] =
		g_signal_new ("remove_object",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, remove_object),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals[DISCARD_ALARM] =
		g_signal_new ("discard_alarm",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, discard_alarm),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals[RECEIVE_OBJECTS] =
		g_signal_new ("receive_objects",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, receive_objects),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals[SEND_OBJECTS] =
		g_signal_new ("send_objects",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, send_objects),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals[DEFAULT_OBJECT] =
		g_signal_new ("default_object",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, default_object),
			      NULL, NULL,
			      cal_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	signals[OBJECT] =
		g_signal_new ("object",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, object),
			      NULL, NULL,
			      cal_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	signals[OBJECT_LIST] =
		g_signal_new ("object_list",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, object_list),
			      NULL, NULL,
			      cal_marshal_VOID__INT_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_POINTER);
	signals[GET_TIMEZONE] =
		g_signal_new ("get_timezone",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, get_timezone),
			      NULL, NULL,
			      cal_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	signals[ADD_TIMEZONE] =
		g_signal_new ("add_timezone",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, add_timezone),
			      NULL, NULL,
			      cal_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	signals[SET_DEFAULT_TIMEZONE] =
		g_signal_new ("set_default_timezone",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, set_default_timezone),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	signals[GET_CHANGES] =
		g_signal_new ("get_changes",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, get_changes),
			      NULL, NULL,
			      cal_marshal_VOID__INT_POINTER,
			      G_TYPE_NONE, 1, G_TYPE_INT, G_TYPE_POINTER);
	signals[GET_FREE_BUSY] =
		g_signal_new ("get_free_busy",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, get_free_busy),
			      NULL, NULL,
			      cal_marshal_VOID__INT_POINTER,
			      G_TYPE_NONE, 1, G_TYPE_INT, G_TYPE_POINTER);
	signals[QUERY] =
		g_signal_new ("query",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CalListenerClass, query),
			      NULL, NULL,
			      cal_marshal_VOID__INT_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_POINTER);
}

BONOBO_TYPE_FUNC_FULL (CalListener,
		       GNOME_Evolution_Calendar_Listener,
		       BONOBO_TYPE_OBJECT,
		       cal_listener);

/**
 * cal_listener_construct:
 * @listener: A calendar listener.
 * @cal_opened_fn: Function that will be called to notify that a calendar was
 * opened.
 * @cal_removed_fn: Function that will be called to notify that a calendar was
 * removed
 * @error_occurred_fn: Function that will be called to notify errors.
 * @categories_changed_fn: Function that will be called to notify that the list
 * of categories that are present in the calendar's objects has changed.
 * @fn_data: Closure data pointer that will be passed to the notification
 * functions.
 *
 * Constructs a calendar listener by setting the callbacks that it will use for
 * notification from the calendar server.
 *
 * Return value: the same object as the @listener argument.
 **/
CalListener *
cal_listener_construct (CalListener *listener,
			CalListenerCalSetModeFn cal_set_mode_fn,
			CalListenerErrorOccurredFn error_occurred_fn,
			CalListenerCategoriesChangedFn categories_changed_fn,
			gpointer fn_data)
{
	CalListenerPrivate *priv;

	g_return_val_if_fail (listener != NULL, NULL);
	g_return_val_if_fail (IS_CAL_LISTENER (listener), NULL);
 	g_return_val_if_fail (cal_set_mode_fn != NULL, NULL);
	g_return_val_if_fail (error_occurred_fn != NULL, NULL);
	g_return_val_if_fail (categories_changed_fn != NULL, NULL);

	priv = listener->priv;

	priv->cal_set_mode_fn = cal_set_mode_fn;
	priv->error_occurred_fn = error_occurred_fn;
	priv->categories_changed_fn = categories_changed_fn;
	priv->fn_data = fn_data;

	return listener;
}

/**
 * cal_listener_new:
 * @cal_opened_fn: Function that will be called to notify that a calendar was
 * opened.
 * @error_occurred_fn: Function that will be called to notify errors.
 * @categories_changed_fn: Function that will be called to notify that the list
 * of categories that are present in the calendar's objects has changed.
 * @fn_data: Closure data pointer that will be passed to the notification
 * functions.
 *
 * Creates a new #CalListener object.
 *
 * Return value: A newly-created #CalListener object.
 **/
CalListener *
cal_listener_new (CalListenerCalSetModeFn cal_set_mode_fn,
		  CalListenerErrorOccurredFn error_occurred_fn,
		  CalListenerCategoriesChangedFn categories_changed_fn,
		  gpointer fn_data)
{
	CalListener *listener;

	g_return_val_if_fail (error_occurred_fn != NULL, NULL);
	g_return_val_if_fail (categories_changed_fn != NULL, NULL);

	listener = g_object_new (CAL_LISTENER_TYPE, 
				 "poa", bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL),
				 NULL);

	return cal_listener_construct (listener,
				       cal_set_mode_fn,
				       error_occurred_fn,
				       categories_changed_fn,
				       fn_data);
}

/**
 * cal_listener_stop_notification:
 * @listener: A calendar listener.
 * 
 * Informs a calendar listener that no further notification is desired.  The
 * callbacks specified when the listener was created will no longer be invoked
 * after this function is called.
 **/
void
cal_listener_stop_notification (CalListener *listener)
{
	CalListenerPrivate *priv;

	g_return_if_fail (listener != NULL);
	g_return_if_fail (IS_CAL_LISTENER (listener));

	priv = listener->priv;
	g_return_if_fail (priv->notify != FALSE);

	priv->notify = FALSE;
}
