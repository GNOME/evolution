/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 */

#ifndef __Cal_BACKEND_SYNC_H__
#define __Cal_BACKEND_SYNC_H__

#include <glib.h>
#include <pcs/cal-backend.h>
#include <pcs/evolution-calendar.h>

G_BEGIN_DECLS

#define CAL_TYPE_BACKEND_SYNC         (cal_backend_sync_get_type ())
#define CAL_BACKEND_SYNC(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CAL_TYPE_BACKEND_SYNC, CalBackendSync))
#define CAL_BACKEND_SYNC_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CAL_TYPE_BACKEND_SYNC, CalBackendSyncClass))
#define CAL_IS_BACKEND_SYNC(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAL_TYPE_BACKEND_SYNC))
#define CAL_IS_BACKEND_SYNC_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAL_TYPE_BACKEND_SYNC))
#define CAL_BACKEND_SYNC_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((k), CAL_TYPE_BACKEND_SYNC, CalBackendSyncClass))
typedef struct _CalBackendSync CalBackendSync;
typedef struct _CalBackendSyncClass CalBackendSyncClass;
typedef struct _CalBackendSyncPrivate CalBackendSyncPrivate;

typedef GNOME_Evolution_Calendar_CallStatus CalBackendSyncStatus;

struct _CalBackendSync {
	CalBackend parent_object;

	CalBackendSyncPrivate *priv;
};

struct _CalBackendSyncClass {
	CalBackendClass parent_class;

	/* Virtual methods */
	CalBackendSyncStatus (*is_read_only_sync)  (CalBackendSync *backend, Cal *cal, gboolean *read_only);
	CalBackendSyncStatus (*get_cal_address_sync)  (CalBackendSync *backend, Cal *cal, char **address);
	CalBackendSyncStatus (*get_alarm_email_address_sync)  (CalBackendSync *backend, Cal *cal, char **address);
	CalBackendSyncStatus (*get_ldap_attribute_sync)  (CalBackendSync *backend, Cal *cal, char **attribute);
	CalBackendSyncStatus (*get_static_capabilities_sync)  (CalBackendSync *backend, Cal *cal, char **capabilities);

	CalBackendSyncStatus (*open_sync)  (CalBackendSync *backend, Cal *cal, gboolean only_if_exists);
	CalBackendSyncStatus (*remove_sync)  (CalBackendSync *backend, Cal *cal);

	CalBackendSyncStatus (*create_object_sync)  (CalBackendSync *backend, Cal *cal, const char *calobj, char **uid);
	CalBackendSyncStatus (*modify_object_sync)  (CalBackendSync *backend, Cal *cal, const char *calobj, CalObjModType mod, char **old_object);
	CalBackendSyncStatus (*remove_object_sync)  (CalBackendSync *backend, Cal *cal, const char *uid, const char *rid, CalObjModType mod, char **object);

	CalBackendSyncStatus (*discard_alarm_sync)  (CalBackendSync *backend, Cal *cal, const char *uid, const char *auid);

	CalBackendSyncStatus (*receive_objects_sync)  (CalBackendSync *backend, Cal *cal, const char *calobj, GList **created, GList **modified, GList **removed);
	CalBackendSyncStatus (*send_objects_sync)  (CalBackendSync *backend, Cal *cal, const char *calobj);

	CalBackendSyncStatus (*get_default_object_sync)  (CalBackendSync *backend, Cal *cal, char **object);
	CalBackendSyncStatus (*get_object_sync)  (CalBackendSync *backend, Cal *cal, const char *uid, const char *rid, char **object);
	CalBackendSyncStatus (*get_object_list_sync)  (CalBackendSync *backend, Cal *cal, const char *sexp, GList **objects);

	CalBackendSyncStatus (*get_timezone_sync) (CalBackendSync *backend, Cal *cal, const char *tzid, char **object);
	CalBackendSyncStatus (*add_timezone_sync) (CalBackendSync *backend, Cal *cal, const char *tzobj);
	CalBackendSyncStatus (*set_default_timezone_sync) (CalBackendSync *backend, Cal *cal, const char *tzid);

	CalBackendSyncStatus (*get_changes_sync) (CalBackendSync *backend, Cal *cal, CalObjType type, const char *change_id, GList **adds, GList **modifies, GList **deletes);
	CalBackendSyncStatus (*get_freebusy_sync) (CalBackendSync *backend, Cal *cal, GList *users, time_t start, time_t end, GList **freebusy);

	/* Padding for future expansion */
	void (*_cal_reserved0) (void);
	void (*_cal_reserved1) (void);
	void (*_cal_reserved2) (void);
	void (*_cal_reserved3) (void);
	void (*_cal_reserved4) (void);

};

typedef CalBackendSync * (*CalBackendSyncFactoryFn) (void);
GType                cal_backend_sync_get_type                (void);
CalBackendSyncStatus cal_backend_sync_is_read_only            (CalBackendSync  *backend,
							       Cal             *cal,
							       gboolean        *read_only);
CalBackendSyncStatus cal_backend_sync_get_cal_address         (CalBackendSync  *backend,
							       Cal             *cal,
							       char           **address);
CalBackendSyncStatus cal_backend_sync_get_alarm_email_address (CalBackendSync  *backend,
							       Cal             *cal,
							       char           **address);
CalBackendSyncStatus cal_backend_sync_get_ldap_attribute      (CalBackendSync  *backend,
							       Cal             *cal,
							       char           **attribute);
CalBackendSyncStatus cal_backend_sync_get_static_capabilities (CalBackendSync  *backend,
							       Cal             *cal,
							       char           **capabiliites);
CalBackendSyncStatus cal_backend_sync_open                    (CalBackendSync  *backend,
							       Cal             *cal,
							       gboolean         only_if_exists);
CalBackendSyncStatus cal_backend_sync_remove                  (CalBackendSync  *backend,
							       Cal             *cal);
CalBackendSyncStatus cal_backend_sync_create_object           (CalBackendSync  *backend,
							       Cal             *cal,
							       const char      *calobj,
							       char           **uid);
CalBackendSyncStatus cal_backend_sync_modify_object           (CalBackendSync  *backend,
							       Cal             *cal,
							       const char      *calobj,
							       CalObjModType    mod,
							       char           **old_object);
CalBackendSyncStatus cal_backend_sync_remove_object           (CalBackendSync  *backend,
							       Cal             *cal,
							       const char      *uid,
							       const char      *rid,
							       CalObjModType    mod,
							       char **object);
CalBackendSyncStatus cal_backend_sync_discard_alarm (CalBackendSync *backend, Cal *cal, const char *uid, const char *auid);

CalBackendSyncStatus cal_backend_sync_receive_objects         (CalBackendSync  *backend,
							       Cal             *cal,
							       const char      *calobj,
							       GList          **created,
							       GList          **modified,
							       GList          **removed);
CalBackendSyncStatus cal_backend_sync_send_objects            (CalBackendSync  *backend,
							       Cal             *cal,
							       const char      *calobj);
CalBackendSyncStatus cal_backend_sync_get_default_object         (CalBackendSync  *backend,
								 Cal             *cal,
								 char           **object);

CalBackendSyncStatus cal_backend_sync_get_object         (CalBackendSync  *backend,
							  Cal             *cal,
							  const char *uid,
							  const char *rid,
							  char           **object);

CalBackendSyncStatus cal_backend_sync_get_object_list         (CalBackendSync  *backend,
							       Cal             *cal,
							       const char      *sexp,
							       GList          **objects);

CalBackendSyncStatus cal_backend_sync_get_timezone (CalBackendSync *backend, Cal *cal, const char *tzid, char **object);
CalBackendSyncStatus cal_backend_sync_add_timezone (CalBackendSync *backend, Cal *cal, const char *tzobj);
CalBackendSyncStatus cal_backend_sync_set_default_timezone (CalBackendSync *backend, Cal *cal, const char *tzid);

CalBackendSyncStatus cal_backend_sync_get_changes (CalBackendSync *backend, Cal *cal, CalObjType type, const char *change_id, GList **adds, GList **modifies, GList **deletes);
CalBackendSyncStatus cal_backend_sync_get_free_busy (CalBackendSync *backend, Cal *cal, GList *users, time_t start, time_t end, GList **freebusy);

G_END_DECLS

#endif /* ! __CAL_BACKEND_SYNC_H__ */
