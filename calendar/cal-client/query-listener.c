/* Evolution calendar - Live search query listener convenience object
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cal-marshal.h"
#include "query-listener.h"



/* Private part of the QueryListener structure */

struct _QueryListenerPrivate {
	int dummy;
};

/* Signal IDs */
enum {
	OBJECTS_ADDED,
	OBJECTS_MODIFIED,
	OBJECTS_REMOVED,
	QUERY_PROGRESS,
	QUERY_DONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static BonoboObjectClass *parent_class;

/* CORBA method implementations */
/* FIXME This is duplicated from cal-listener.c */
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

/* FIXME This is duplicated from cal-listener.c */
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

static GList *
build_uid_list (const GNOME_Evolution_Calendar_CalObjUIDSeq *seq)
{
	GList *list;
	int i;

	list = NULL;
	for (i = 0; i < seq->_length; i++)
		list = g_list_prepend (list, g_strdup (seq->_buffer[i]));

	return list;
}

static void
impl_notifyObjectsAdded (PortableServer_Servant servant,
			 const GNOME_Evolution_Calendar_stringlist *objects,
			 CORBA_Environment *ev)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;
	GList *object_list, *l;
	
	ql = QUERY_LISTENER (bonobo_object_from_servant (servant));
	priv = ql->priv;

	object_list = build_object_list (objects);
	
	g_signal_emit (G_OBJECT (ql), signals[OBJECTS_ADDED], 0, object_list);

	for (l = object_list; l; l = l->next)
		icalcomponent_free (l->data);
	g_list_free (object_list);
}

static void
impl_notifyObjectsModified (PortableServer_Servant servant,
			    const GNOME_Evolution_Calendar_stringlist *objects,
			    CORBA_Environment *ev)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;
	GList *object_list, *l;
	
	ql = QUERY_LISTENER (bonobo_object_from_servant (servant));
	priv = ql->priv;

	object_list = build_object_list (objects);
	
	g_signal_emit (G_OBJECT (ql), signals[OBJECTS_MODIFIED], 0, object_list);

	for (l = object_list; l; l = l->next)
		icalcomponent_free (l->data);
	g_list_free (object_list);
}

static void
impl_notifyObjectsRemoved (PortableServer_Servant servant,
			   const GNOME_Evolution_Calendar_CalObjUIDSeq *uids,
			   CORBA_Environment *ev)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;
	GList *uid_list, *l;
	
	ql = QUERY_LISTENER (bonobo_object_from_servant (servant));
	priv = ql->priv;

	uid_list = build_uid_list (uids);
	
	g_signal_emit (G_OBJECT (ql), signals[OBJECTS_REMOVED], 0, uid_list);

	for (l = uid_list; l; l = l->next)
		g_free (l->data);
	g_list_free (uid_list);
}

static void
impl_notifyQueryProgress (PortableServer_Servant servant,
			  const CORBA_char *message,
			  const CORBA_short percent,
			  CORBA_Environment *ev)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;

	ql = QUERY_LISTENER (bonobo_object_from_servant (servant));
	priv = ql->priv;
	
	g_signal_emit (G_OBJECT (ql), signals[QUERY_PROGRESS], 0, message, percent);
}

static void
impl_notifyQueryDone (PortableServer_Servant servant,
		      const GNOME_Evolution_Calendar_CallStatus status,
		      CORBA_Environment *ev)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;

	ql = QUERY_LISTENER (bonobo_object_from_servant (servant));
	priv = ql->priv;
	
	g_signal_emit (G_OBJECT (ql), signals[QUERY_DONE], 0, convert_status (status));
}

/* Object initialization function for the live search query listener */
static void
query_listener_init (QueryListener *ql, QueryListenerClass *class)
{
	QueryListenerPrivate *priv;

	priv = g_new0 (QueryListenerPrivate, 1);
	ql->priv = priv;
}

/* Finalize handler for the live search query listener */
static void
query_listener_finalize (GObject *object)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_QUERY_LISTENER (object));

	ql = QUERY_LISTENER (object);
	priv = ql->priv;

	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Class initialization function for the live search query listener */
static void
query_listener_class_init (QueryListenerClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = query_listener_finalize;

	klass->epv.notifyObjectsAdded = impl_notifyObjectsAdded;
	klass->epv.notifyObjectsModified = impl_notifyObjectsModified;
	klass->epv.notifyObjectsRemoved = impl_notifyObjectsRemoved;
	klass->epv.notifyQueryProgress = impl_notifyQueryProgress;
	klass->epv.notifyQueryDone = impl_notifyQueryDone;

	signals[OBJECTS_ADDED] =
		g_signal_new ("objects_added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (QueryListenerClass, objects_added),
			      NULL, NULL,
			      cal_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[OBJECTS_MODIFIED] =
		g_signal_new ("objects_modified",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (QueryListenerClass, objects_modified),
			      NULL, NULL,
			      cal_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[OBJECTS_REMOVED] =
		g_signal_new ("objects_removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (QueryListenerClass, objects_removed),
			      NULL, NULL,
			      cal_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[QUERY_PROGRESS] =
		g_signal_new ("query_progress",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (QueryListenerClass, query_progress),
			      NULL, NULL,
			      cal_marshal_VOID__POINTER,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_INT);
	signals[QUERY_DONE] =
		g_signal_new ("query_done",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (QueryListenerClass, query_done),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
}

BONOBO_TYPE_FUNC_FULL (QueryListener,
		       GNOME_Evolution_Calendar_QueryListener,
		       BONOBO_TYPE_OBJECT,
		       query_listener);

QueryListener *
query_listener_new (void)
{
	QueryListener *ql;

	ql = g_object_new (QUERY_LISTENER_TYPE, NULL);

	return ql;
}
