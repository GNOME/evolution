/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   Chris Toshok (toshok@ximian.com)
 *
 * Copyright (C) 2003, Ximian, Inc.
 */

#ifdef CONFIG_H
#include <config.h>
#endif

#include "cal-backend-sync.h"

struct _CalBackendSyncPrivate {
  int mumble;
};

static GObjectClass *parent_class;

G_LOCK_DEFINE_STATIC (cal_sync_mutex);
#define	SYNC_LOCK()		G_LOCK (cal_sync_mutex)
#define	SYNC_UNLOCK()		G_UNLOCK (cal_sync_mutex)

CalBackendSyncStatus
cal_backend_sync_is_read_only  (CalBackendSync *backend, Cal *cal, gboolean *read_only)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);
	g_return_val_if_fail (read_only, GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->is_read_only_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->is_read_only_sync) (backend, cal, read_only);
}

CalBackendSyncStatus
cal_backend_sync_get_cal_address  (CalBackendSync *backend, Cal *cal, char **address)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);
	g_return_val_if_fail (address, GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_cal_address_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_cal_address_sync) (backend, cal, address);
}

CalBackendSyncStatus
cal_backend_sync_get_alarm_email_address  (CalBackendSync *backend, Cal *cal, char **address)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);
	g_return_val_if_fail (address, GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_alarm_email_address_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_alarm_email_address_sync) (backend, cal, address);
}

CalBackendSyncStatus
cal_backend_sync_get_ldap_attribute  (CalBackendSync *backend, Cal *cal, char **attribute)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);
	g_return_val_if_fail (attribute, GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_ldap_attribute_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_ldap_attribute_sync) (backend, cal, attribute);
}

CalBackendSyncStatus
cal_backend_sync_get_static_capabilities  (CalBackendSync *backend, Cal *cal, char **capabilities)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);
	g_return_val_if_fail (capabilities, GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_static_capabilities_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_static_capabilities_sync) (backend, cal, capabilities);
}

CalBackendSyncStatus
cal_backend_sync_open  (CalBackendSync *backend, Cal *cal, gboolean only_if_exists)
{
	CalBackendSyncStatus status;
	
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->open_sync);

	SYNC_LOCK ();
	
	status = (* CAL_BACKEND_SYNC_GET_CLASS (backend)->open_sync) (backend, cal, only_if_exists);

	SYNC_UNLOCK ();

	return status;
}

CalBackendSyncStatus
cal_backend_sync_remove  (CalBackendSync *backend, Cal *cal)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->remove_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->remove_sync) (backend, cal);
}

CalBackendSyncStatus
cal_backend_sync_create_object (CalBackendSync *backend, Cal *cal, const char *calobj, char **uid)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->create_object_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->create_object_sync) (backend, cal, calobj, uid);
}

CalBackendSyncStatus
cal_backend_sync_modify_object (CalBackendSync *backend, Cal *cal, const char *calobj, 
				CalObjModType mod, char **old_object)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->modify_object_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->modify_object_sync) (backend, cal, 
									     calobj, mod, old_object);
}

CalBackendSyncStatus
cal_backend_sync_remove_object (CalBackendSync *backend, Cal *cal, const char *uid, const char *rid,
				CalObjModType mod, char **object)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->remove_object_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->remove_object_sync) (backend, cal, uid, rid, mod, object);
}

CalBackendSyncStatus
cal_backend_sync_discard_alarm (CalBackendSync *backend, Cal *cal, const char *uid, const char *auid)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->discard_alarm_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->discard_alarm_sync) (backend, cal, uid, auid);
}

CalBackendSyncStatus
cal_backend_sync_receive_objects (CalBackendSync *backend, Cal *cal, const char *calobj,
				  GList **created, GList **modified, GList **removed)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->receive_objects_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->receive_objects_sync) (backend, cal, calobj, 
									       created, modified, removed);
}

CalBackendSyncStatus
cal_backend_sync_send_objects (CalBackendSync *backend, Cal *cal, const char *calobj)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->send_objects_sync);
	
	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->send_objects_sync) (backend, cal, calobj);
}

CalBackendSyncStatus
cal_backend_sync_get_default_object (CalBackendSync *backend, Cal *cal, char **object)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);
	g_return_val_if_fail (object, GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_default_object_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_default_object_sync) (backend, cal, object);
}

CalBackendSyncStatus
cal_backend_sync_get_object (CalBackendSync *backend, Cal *cal, const char *uid, const char *rid, char **object)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);
	g_return_val_if_fail (object, GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_object_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_object_sync) (backend, cal, uid, rid, object);
}

CalBackendSyncStatus
cal_backend_sync_get_object_list (CalBackendSync *backend, Cal *cal, const char *sexp, GList **objects)
{
	g_return_val_if_fail (backend && CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);
	g_return_val_if_fail (objects, GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_object_list_sync);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_object_list_sync) (backend, cal, sexp, objects);
}

CalBackendSyncStatus
cal_backend_sync_get_timezone (CalBackendSync *backend, Cal *cal, const char *tzid, char **object)
{
	g_return_val_if_fail (CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_timezone_sync != NULL);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_timezone_sync) (backend, cal, tzid, object);
}

CalBackendSyncStatus
cal_backend_sync_add_timezone (CalBackendSync *backend, Cal *cal, const char *tzobj)
{
	g_return_val_if_fail (CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->add_timezone_sync != NULL);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->add_timezone_sync) (backend, cal, tzobj);
}

CalBackendSyncStatus
cal_backend_sync_set_default_timezone (CalBackendSync *backend, Cal *cal, const char *tzid)
{
	g_return_val_if_fail (CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->set_default_timezone_sync != NULL);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->set_default_timezone_sync) (backend, cal, tzid);
}


CalBackendSyncStatus
cal_backend_sync_get_changes (CalBackendSync *backend, Cal *cal, CalObjType type, const char *change_id,
			      GList **adds, GList **modifies, GList **deletes)
{
	g_return_val_if_fail (CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_changes_sync != NULL);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_changes_sync) (backend, cal, type, change_id, 
									   adds, modifies, deletes);
}

CalBackendSyncStatus
cal_backend_sync_get_free_busy (CalBackendSync *backend, Cal *cal, GList *users, 
				time_t start, time_t end, GList **freebusy)
{
	g_return_val_if_fail (CAL_IS_BACKEND_SYNC (backend), GNOME_Evolution_Calendar_OtherError);

	g_assert (CAL_BACKEND_SYNC_GET_CLASS (backend)->get_freebusy_sync != NULL);

	return (* CAL_BACKEND_SYNC_GET_CLASS (backend)->get_freebusy_sync) (backend, cal, users, 
									    start, end, freebusy);
}


static void
_cal_backend_is_read_only (CalBackend *backend, Cal *cal)
{
	CalBackendSyncStatus status;
	gboolean read_only;

	status = cal_backend_sync_is_read_only (CAL_BACKEND_SYNC (backend), cal, &read_only);

	cal_notify_read_only (cal, status, read_only);
}

static void
_cal_backend_get_cal_address (CalBackend *backend, Cal *cal)
{
	CalBackendSyncStatus status;
	char *address;

	status = cal_backend_sync_get_cal_address (CAL_BACKEND_SYNC (backend), cal, &address);

	cal_notify_cal_address (cal, status, address);

	g_free (address);
}

static void
_cal_backend_get_alarm_email_address (CalBackend *backend, Cal *cal)
{
	CalBackendSyncStatus status;
	char *address;

	status = cal_backend_sync_get_cal_address (CAL_BACKEND_SYNC (backend), cal, &address);

	cal_notify_alarm_email_address (cal, status, address);

	g_free (address);
}

static void
_cal_backend_get_ldap_attribute (CalBackend *backend, Cal *cal)
{
	CalBackendSyncStatus status;
	char *attribute;

	status = cal_backend_sync_get_cal_address (CAL_BACKEND_SYNC (backend), cal, &attribute);

	cal_notify_ldap_attribute (cal, status, attribute);

	g_free (attribute);
}

static void
_cal_backend_get_static_capabilities (CalBackend *backend, Cal *cal)
{
	CalBackendSyncStatus status;
	char *capabilities;

	status = cal_backend_sync_get_cal_address (CAL_BACKEND_SYNC (backend), cal, &capabilities);

	cal_notify_static_capabilities (cal, status, capabilities);

	g_free (capabilities);
}

static void
_cal_backend_open (CalBackend *backend, Cal *cal, gboolean only_if_exists)
{
	CalBackendSyncStatus status;

	status = cal_backend_sync_open (CAL_BACKEND_SYNC (backend), cal, only_if_exists);

	cal_notify_open (cal, status);
}

static void
_cal_backend_remove (CalBackend *backend, Cal *cal)
{
	CalBackendSyncStatus status;

	status = cal_backend_sync_remove (CAL_BACKEND_SYNC (backend), cal);

	cal_notify_remove (cal, status);
}

static void
_cal_backend_create_object (CalBackend *backend, Cal *cal, const char *calobj)
{
	CalBackendSyncStatus status;
	char *uid = NULL;
	
	status = cal_backend_sync_create_object (CAL_BACKEND_SYNC (backend), cal, calobj, &uid);

	cal_notify_object_created (cal, status, uid, calobj);

	if (uid)
		g_free (uid);
}

static void
_cal_backend_modify_object (CalBackend *backend, Cal *cal, const char *calobj, CalObjModType mod)
{
	CalBackendSyncStatus status;
	char *old_object;
	
	status = cal_backend_sync_modify_object (CAL_BACKEND_SYNC (backend), cal, 
						 calobj, mod, &old_object);

	cal_notify_object_modified (cal, status, old_object, calobj);
}

static void
_cal_backend_remove_object (CalBackend *backend, Cal *cal, const char *uid, const char *rid, CalObjModType mod)
{
	CalBackendSyncStatus status;
	char *object;
	
	status = cal_backend_sync_remove_object (CAL_BACKEND_SYNC (backend), cal, uid, rid, mod, &object);

	cal_notify_object_removed (cal, status, uid, object);
}

static void
_cal_backend_discard_alarm (CalBackend *backend, Cal *cal, const char *uid, const char *auid)
{
	CalBackendSyncStatus status;
	
	status = cal_backend_sync_discard_alarm (CAL_BACKEND_SYNC (backend), cal, uid, auid);

	cal_notify_alarm_discarded (cal, status);
}

static void
_cal_backend_receive_objects (CalBackend *backend, Cal *cal, const char *calobj)
{
	CalBackendSyncStatus status;
	GList *created = NULL, *modified = NULL, *removed = NULL;
	
	status = cal_backend_sync_receive_objects (CAL_BACKEND_SYNC (backend), cal, calobj, 
						   &created, &modified, &removed);

	cal_notify_objects_received (cal, status, created, modified, removed);
}

static void
_cal_backend_send_objects (CalBackend *backend, Cal *cal, const char *calobj)
{
	CalBackendSyncStatus status;

	status = cal_backend_sync_send_objects (CAL_BACKEND_SYNC (backend), cal, calobj);

	cal_notify_objects_sent (cal, status);
}

static void
_cal_backend_get_default_object (CalBackend *backend, Cal *cal)
{
	CalBackendSyncStatus status;
	char *object = NULL;

	status = cal_backend_sync_get_default_object (CAL_BACKEND_SYNC (backend), cal, &object);

	cal_notify_default_object (cal, status, object);

	g_free (object);
}

static void
_cal_backend_get_object (CalBackend *backend, Cal *cal, const char *uid, const char *rid)
{
	CalBackendSyncStatus status;
	char *object = NULL;

	status = cal_backend_sync_get_object (CAL_BACKEND_SYNC (backend), cal, uid, rid, &object);

	cal_notify_object (cal, status, object);
	
	g_free (object);
}

static void
_cal_backend_get_object_list (CalBackend *backend, Cal *cal, const char *sexp)
{
	CalBackendSyncStatus status;
	GList *objects, *l;

	status = cal_backend_sync_get_object_list (CAL_BACKEND_SYNC (backend), cal, sexp, &objects);

	cal_notify_object_list (cal, status, objects);

	for (l = objects; l; l = l->next)
		g_free (l->data);
	g_list_free (objects);
}

static void
_cal_backend_get_timezone (CalBackend *backend, Cal *cal, const char *tzid)
{
	CalBackendSyncStatus status;
	char *object = NULL;
	
	status = cal_backend_sync_get_timezone (CAL_BACKEND_SYNC (backend), cal, tzid, &object);

	cal_notify_timezone_requested (cal, status, object);
}

static void
_cal_backend_add_timezone (CalBackend *backend, Cal *cal, const char *tzobj)
{
	CalBackendSyncStatus status;

	status = cal_backend_sync_add_timezone (CAL_BACKEND_SYNC (backend), cal, tzobj);

	cal_notify_timezone_added (cal, status, tzobj);
}

static void
_cal_backend_set_default_timezone (CalBackend *backend, Cal *cal, const char *tzid)
{
	CalBackendSyncStatus status;

	status = cal_backend_sync_set_default_timezone (CAL_BACKEND_SYNC (backend), cal, tzid);

	cal_notify_default_timezone_set (cal, status);
}

static void
_cal_backend_get_changes (CalBackend *backend, Cal *cal, CalObjType type, const char *change_id)
{
	CalBackendSyncStatus status;
	GList *adds = NULL, *modifies = NULL, *deletes = NULL, *l;
	
	status = cal_backend_sync_get_changes (CAL_BACKEND_SYNC (backend), cal, type, change_id, 
					       &adds, &modifies, &deletes);

	cal_notify_changes (cal, status, adds, modifies, deletes);

	for (l = adds; l; l = l->next)
		g_free (l->data);
	g_list_free (adds);

	for (l = modifies; l; l = l->next)
		g_free (l->data);
	g_list_free (modifies);

	for (l = deletes; l; l = l->next)
		g_free (l->data);
	g_list_free (deletes);
}

static void
_cal_backend_get_free_busy (CalBackend *backend, Cal *cal, GList *users, time_t start, time_t end)
{
	CalBackendSyncStatus status;
	GList *freebusy = NULL, *l;
	
	status = cal_backend_sync_get_free_busy (CAL_BACKEND_SYNC (backend), cal, users, start, end, &freebusy);

	cal_notify_free_busy (cal, status, freebusy);

	for (l = freebusy; l; l = l->next)
		g_free (l->data);
	g_list_free (freebusy);
}

static void
cal_backend_sync_init (CalBackendSync *backend)
{
	CalBackendSyncPrivate *priv;

	priv          = g_new0 (CalBackendSyncPrivate, 1);

	backend->priv = priv;
}

static void
cal_backend_sync_dispose (GObject *object)
{
	CalBackendSync *backend;

	backend = CAL_BACKEND_SYNC (object);

	if (backend->priv) {
		g_free (backend->priv);

		backend->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_backend_sync_class_init (CalBackendSyncClass *klass)
{
	GObjectClass *object_class;
	CalBackendClass *backend_class = CAL_BACKEND_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class = (GObjectClass *) klass;

	backend_class->is_read_only = _cal_backend_is_read_only;
	backend_class->get_cal_address = _cal_backend_get_cal_address;
	backend_class->get_alarm_email_address = _cal_backend_get_alarm_email_address;
	backend_class->get_ldap_attribute = _cal_backend_get_ldap_attribute;
	backend_class->get_static_capabilities = _cal_backend_get_static_capabilities;
	backend_class->open = _cal_backend_open;
	backend_class->remove = _cal_backend_remove;
	backend_class->create_object = _cal_backend_create_object;
	backend_class->modify_object = _cal_backend_modify_object;
	backend_class->remove_object = _cal_backend_remove_object;
	backend_class->discard_alarm = _cal_backend_discard_alarm;
	backend_class->receive_objects = _cal_backend_receive_objects;
	backend_class->send_objects = _cal_backend_send_objects;
	backend_class->get_default_object = _cal_backend_get_default_object;
	backend_class->get_object = _cal_backend_get_object;
	backend_class->get_object_list = _cal_backend_get_object_list;
	backend_class->get_timezone = _cal_backend_get_timezone;
	backend_class->add_timezone = _cal_backend_add_timezone;
	backend_class->set_default_timezone = _cal_backend_set_default_timezone;
 	backend_class->get_changes = _cal_backend_get_changes;
 	backend_class->get_free_busy = _cal_backend_get_free_busy;

	object_class->dispose = cal_backend_sync_dispose;
}

/**
 * cal_backend_get_type:
 */
GType
cal_backend_sync_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (CalBackendSyncClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  cal_backend_sync_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (CalBackendSync),
			0,    /* n_preallocs */
			(GInstanceInitFunc) cal_backend_sync_init
		};

		type = g_type_register_static (CAL_BACKEND_TYPE, "CalBackendSync", &info, 0);
	}

	return type;
}
