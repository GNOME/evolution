/* Evolution calendar - iCalendar DB backend
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
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
#include <gtk/gtkobject.h>
#include "cal-util/cal-recur.h"
#include "cal-backend-db.h"
#include <db.h>
#if DB_MAJOR_VERSION < 3
#  error "You need libdb3 to compile the DB backend"
#endif

#define ENVIRONMENT_DIRECTORY "%s/evolution/local/Calendar/db.environment"

/* structure to identify an open cursor */
typedef struct {
	gint ref;
	DBC* dbc;
	DB*  parent_db;
	/* TODO: convert into a hash table */
	GList* keys;
	GList* data;
} CalBackendDBCursor;

/* private part of the CalBackendDB structure */
struct _CalBackendDBPrivate {
	/* URI where the calendar data is stored */
	GnomeVFSURI *uri;

	/* Berkeley DB's library handles */
	DB_ENV *environment;
	DB *objects_db;
	DB *history_db;

	/* list of open cursors */
	GList *cursors;

	/* list of clients using this backend */
	GList *clients;
};

static void cal_backend_db_class_init (CalBackendDBClass *klass);
static void cal_backend_db_init (CalBackendDB *cbdb);
static void cal_backend_db_destroy (GtkObject *object);

static GnomeVFSURI *cal_backend_db_get_uri (CalBackend *backend);
static void cal_backend_db_add_cal (CalBackend *backend, Cal *cal);
static CalBackendOpenStatus cal_backend_db_open (CalBackend *backend,
                                                 GnomeVFSURI *uri,
                                                 gboolean only_if_exists);

static int cal_backend_db_get_n_objects (CalBackend *backend, CalObjType type);
static char *cal_backend_db_get_object (CalBackend *backend, const char *uid);
static CalObjType cal_backend_db_get_type_by_uid (CalBackend *backend, const char *uid);
static GList* cal_backend_db_get_uids (CalBackend *backend, CalObjType type);
static GList* cal_backend_db_get_objects_in_range (CalBackend *backend,
                                                   CalObjType type,
                                                   time_t start,
                                                   time_t end);
static GNOME_Evolution_Calendar_CalObjChangeSeq *cal_backend_db_get_changes (
	CalBackend *backend, CalObjType type, const char *change_id);

static GNOME_Evolution_Calendar_CalComponentAlarmsSeq *cal_backend_db_get_alarms_in_range (
	CalBackend *backend, time_t start, time_t end);

static GNOME_Evolution_Calendar_CalComponentAlarms *cal_backend_db_get_alarms_for_object (
	CalBackend *backend, const char *uid, time_t start, time_t end, gboolean *object_found);

static gboolean cal_backend_db_update_object (CalBackend *backend,
                                              const char *uid,
                                              const char *calobj);
static gboolean cal_backend_db_remove_object (CalBackend *backend, const char *uid);

static void close_cursor (CalBackendDB *cbdb, CalBackendDBCursor *cursor);
static CalBackendDBCursor *open_cursor (CalBackendDB *cbdb, DB *db);
static CalBackendDBCursor *find_cursor_by_db (CalBackendDB *cbdb, DB *db);
static DBT *find_record_by_id (CalBackendDBCursor *cursor, const gchar *id);

static DB_TXN *begin_transaction (CalBackendDB *cbdb);
static void commit_transaction (DB_TXN *tid);
static void rollback_transaction (DB_TXN *tid);

static CalBackendClass *parent_class;

/**
 * cal_backend_db_get_type:
 * @void:
 *
 * Registers the #CalBackendDB class if necessary and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalBackendDB class.
 */
GtkType
cal_backend_db_get_type (void)
{
	static GtkType cal_backend_db_type = 0;

	if (!cal_backend_db_type) {
		static const GtkTypeInfo cal_backend_db_info = {
			"CalBackendDB",
			sizeof (CalBackendDB),
			sizeof (CalBackendDBClass),
			(GtkClassInitFunc) cal_backend_db_class_init,
			(GtkObjectInitFunc) cal_backend_db_init,
			NULL,
			NULL,
			(GtkClassInitFunc) NULL
		};

		cal_backend_db_type = gtk_type_unique(CAL_BACKEND_TYPE, &cal_backend_db_info);
	}

	return cal_backend_db_type;
}

/* class initialization function for the DB backend */
static void
cal_backend_db_class_init (CalBackendDBClass *klass)
{
	GtkObjectClass *object_class;
	CalBackendClass *backend_class;

	object_class = (GtkObjectClass *) klass;
	backend_class = (CalBackendClass *) klass;

	parent_class = gtk_type_class(CAL_BACKEND_TYPE);

	object_class->destroy = cal_backend_db_destroy;

	backend_class->get_uri = cal_backend_db_get_uri;
	backend_class->add_cal = cal_backend_db_add_cal;
	backend_class->open = cal_backend_db_open;
	backend_class->get_n_objects = cal_backend_db_get_n_objects;
	backend_class->get_object = cal_backend_db_get_object;
	backend_class->get_type_by_uid = cal_backend_db_get_type_by_uid;
	backend_class->get_uids = cal_backend_db_get_uids;
	backend_class->get_objects_in_range = cal_backend_db_get_objects_in_range;
	backend_class->get_changes = cal_backend_db_get_changes;
	backend_class->get_alarms_in_range = cal_backend_db_get_alarms_in_range;
	backend_class->get_alarms_for_object = cal_backend_db_get_alarms_for_object;
	backend_class->update_object = cal_backend_db_update_object;
	backend_class->remove_object = cal_backend_db_remove_object;
}

/* object initialization function for the DB backend */
static void
cal_backend_db_init (CalBackendDB *cbdb)
{
	CalBackendDBPrivate *priv;

	priv = g_new0(CalBackendDBPrivate, 1);
	cbdb->priv = priv;
}

/* Destroy handler for the DB backend */
static void
cal_backend_db_destroy (GtkObject *object)
{
	CalBackendDB *cbdb;
	CalBackendDBPrivate *priv;
	GList *node;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_CAL_BACKEND_DB(object));

	cbdb = CAL_BACKEND_DB(object);
	priv = cbdb->priv;

	g_assert(cbdb->priv->clients == NULL);

	/* clean up */
	if (priv->uri) {
		gnome_vfs_uri_unref(priv->uri);
		priv->uri = NULL;
	}

	/* close open cursors */
	while ((node = g_list_first(cbdb->priv->cursors))) {
		close_cursor(cbdb, (CalBackendDBCursor *) node->data);
	}

	/* close open databases */
	if (cbdb->priv->objects_db)
		cbdb->priv->objects_db->close(cbdb->priv->objects_db, 0);
	if (cbdb->priv->history_db)
		cbdb->priv->history_db->close(cbdb->priv->history_db, 0);

	g_free((gpointer) priv);
	cbdb->priv = NULL;

	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
}

/*
 * Private functions
 */

/* close an open cursor and frees all associated memory */
static void
close_cursor (CalBackendDB *cbdb, CalBackendDBCursor *cursor)
{
	GList *node;
	DBT *dbt;

	g_return_if_fail(cursor != NULL);

	cursor->ref--;
	if (cursor->ref > 0)
		return;

	/* free all keys and data */
	while ((node = g_list_first(cursor->keys))) {
		dbt = (DBT *) node->data;
		cursor->keys = g_list_remove(cursor->keys, (gpointer) dbt);
		g_free((gpointer) dbt);
	}
	while ((node = g_list_first(cursor->data))) {
		dbt = (DBT *) node->data;
		cursor->data = g_list_remove(cursor->data, (gpointer) dbt);
		g_free((gpointer) dbt);
	}

	/* finally, close the cursor */
	cursor->dbc->c_close(cursor->dbc);

	cbdb->priv->cursors = g_list_remove(cbdb->priv->cursors, (gpointer) cursor);
	g_free((gpointer) cursor);
}

/* open a cursor for the given database */
static CalBackendDBCursor *
open_cursor (CalBackendDB *cbdb, DB *db)
{
	CalBackendDBCursor *cursor;
	gint ret;

	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(db != NULL, NULL);

	/* search for the cursor in our list of cursors */
	cursor = find_cursor_by_db(cbdb, db);
	if (cursor) {
		cursor->ref++;
		return cursor;
	}

	/* create the cursor */
	cursor = g_new0(CalBackendDBCursor, 1);
	cursor->parent_db = db;

	ret = db->cursor(db, NULL, &cursor->dbc, 0);
	if (ret == 0) {
		DBT key;
		DBT data;

		/* read data */
		while ((ret = cursor->dbc->c_get(cursor->dbc, &key, &data, DB_NEXT)) == 0) {
			cursor->keys = g_list_append(cursor->keys, g_memdup(&key, sizeof(key)));
			cursor->data = g_list_append(cursor->data, g_memdup(&data, sizeof(data)));
		}
		if (ret == DB_NOTFOUND) {
			cbdb->priv->cursors = g_list_prepend(cbdb->priv->cursors, (gpointer) cursor);
			return cursor;
		}

		/* close cursor on error */
		close_cursor(cbdb, cursor);
	}

	return NULL;
}

/* search for a cursor in the given backend */
static CalBackendDBCursor *
find_cursor_by_db (CalBackendDB *cbdb, DB *db)
{
	GList *node;

	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(cbdb->priv != NULL, NULL);
	g_return_val_if_fail(db != NULL, NULL);

	for (node = g_list_first(cbdb->priv->cursors); node != NULL; node = g_list_next(node)) {
		CalBackendDBCursor* cursor = (CalBackendDBCursor *) node->data;
		if (cursor && cursor->parent_db == db)
			return cursor;
	}

	return NULL; /* not found */
}

/* finds a record in a cursor by its ID */
static DBT *
find_record_by_id (CalBackendDBCursor *cursor, const gchar *id)
{
	GList *node;
	gint pos = 0;

	g_return_val_if_fail(cursor != NULL, NULL);
	g_return_val_if_fail(id != NULL, NULL);

	for (node = g_list_first(cursor->keys); node != NULL; node = g_list_next(node)) {
		DBT* key = (DBT *) node->data;
		if (key) {
			if (!strcmp((gchar *) key->data, id)) {
				GList* tmp = g_list_nth(cursor->data, pos);
				if (tmp)
					return (DBT *) node->data;
			}
		}
		pos++;
	}

	return NULL; /* not found */
}

static DB_TXN *
begin_transaction (CalBackendDB *cbdb)
{
	DB_TXN *tid;
	gint ret;
	
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(cbdb->priv != NULL, NULL);
	
	if ((ret = txn_begin(cbdb->priv->environment, NULL, &tid, 0)) != 0) {
		/* TODO: error logging */
		return NULL;
	}
	
	return tid;
}

static void
commit_transaction (DB_TXN *tid)
{
	gint ret;
	
	g_return_if_fail(tid != NULL);
	
	if ((ret = txn_begin(tid, 0)) != 0) {
		/* TODO: error logging? */
	}
}

static void
rollback_transaction (DB_TXN *tid)
{
	gint ret;
	
	g_return_if_fail(tid != NULL);
	
	if ((ret = txn_abort(tid)) != 0) {
		/* TODO: error logging? */
	}
}

/*
 * Calendar backend methods
 */

/* get_uri handler for the DB backend */
static GnomeVFSURI *
cal_backend_db_get_uri (CalBackend *backend)
{
	CalBackendDB *cbdb;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(cbdb->priv != NULL, NULL);

	return cbdb->priv->uri;
}

/* callback used when a Cal is destroyed */
static void
destroy_cal_cb (GtkObject *object, gpointer data)
{
	Cal *cal;
	Cal *tmp_cal;
	CalBackendDB *cbdb;
	GList *node;

	cal = CAL(object);
	cbdb = CAL_BACKEND_DB(data);
	
	g_return_if_fail(IS_CAL_BACKEND_DB(cbdb));
	g_return_if_fail(cbdb->priv != NULL);

	/* find the Cal in the list of clients */
	for (node = cbdb->priv->clients; node != NULL; node = g_list_next(node)) {
		tmp_cal = CAL(node->data);
		if (tmp_cal == cal)
			break;
	}

	if (node) {
		/* disconnect this Cal */
		cbdb->priv->clients = g_list_remove_link(cbdb->priv->clients, node);
		g_list_free_1(node);

		/* when all clients go away, notify the parent factory about it so that
		 * it may decide to kill the backend or not.
		 */
		if (!cbdb->priv->clients)
			cal_backend_last_client_gone(CAL_BACKEND(cbdb));
	}
}

/* add_cal_handler for the DB backend */
static void
cal_backend_db_add_cal (CalBackend *backend, Cal *cal)
{
	CalBackendDB *cbdb;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_if_fail(IS_CAL_BACKEND_DB(cbdb));
	g_return_if_fail(cbdb->priv != NULL);
	g_return_if_fail(IS_CAL(cal));

	/* we do not keep a reference to the Cal since the calendar user agent
	 * owns it
	 */
	gtk_signal_connect(GTK_OBJECT(cal),
	                   "destroy",
	                   GTK_SIGNAL_FUNC(destroy_cal_cb),
	                   backend);

	cbdb->priv->clients = g_list_prepend(cbdb->priv->clients, (gpointer) cal);
}

/* database file initialization */
static gboolean
open_database_file (CalBackendDB *cbdb, const gchar *str_uri, gboolean only_if_exists)
{
	gint ret;
	struct stat sb;

	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), FALSE);
	g_return_val_if_fail(cbdb->priv != NULL, FALSE);
	g_return_val_if_fail(cbdb->priv->objects_db != NULL, FALSE);
	g_return_val_if_fail(cbdb->priv->history_db != NULL, FALSE);
	g_return_val_if_fail(str_uri != NULL, FALSE);

	/* initialize DB environment (for transactions) */
	if (stat(ENVIRONMENT_DIRECTORY, &sb) != 0) {
		gchar *dir;
		
		/* if the directory exists, we're done, since DB will fail if it's the
		 * wrong one. If it does not exist, create the environment */
		dir = g_strdup_printf(ENVIRONMENT_DIRECTORY, g_get_home_dir());
		if (mkdir(dir, I_RWXU) != 0) {
			g_free((gpointer) dir);
			return FALSE;
		}

		g_free((gpointer) dir);
		
		/* create the environment handle */
		if ((ret = db_env_create(&cbdb->priv->environment, 0)) != 0) {
			return FALSE;
		}
		
		cbdb->priv->environment->set_errorpfx(cbdb->priv->environment, "cal-backend-db");
		
		/* open the transactional environment */
		if ((ret = cbdb->priv->environment->open(cbdb->priv->environment,
		                                         ENVIRONMENT_DIRECTORY,
		                                         DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG |
		                                         DB_INIT_MPOOL | DB_INIT_TXN |
		                                         DB_RECOVER | DB_THREAD,
		                                         S_IRUSR | S_IWUSR)) != 0) {
			return FALSE;
		}
	}
	
	/* open/create objects database into given file */
	if ((ret = db_create(&cbdb->priv->objects_db, cbdb->priv->environment, 0)) != 0
	    || (ret = db_create(&cbdb->priv->history_db, cbdb->priv->environment, 0)) != 0) {
		return FALSE;
	}

	if (only_if_exists) {
		ret = cbdb->priv->objects_db->open(cbdb->priv->objects_db,
		                                   str_uri,
		                                   "calendar_objects",
		                                   DB_HASH,
		                                   DB_THREAD,
		                                   0644);
	}
	else {
		ret = cbdb->priv->objects_db->open(cbdb->priv->objects_db,
		                                   str_uri,
		                                   "calendar_objects",
		                                   DB_HASH,
		                                   DB_CREATE | DB_THREAD,
		                                   0644);
	}
	if (ret == 0) {
		/* now, open the history database */
		ret = cbdb->priv->history_db->open(cbdb->priv->history_db,
		                                   str_uri,
		                                   "calendar_history",
		                                   DB_BTREE,
		                                   DB_CREATE,
		                                   0644);
		if (ret == 0) return TRUE;

		/* close objects database on error */
		cbdb->priv->objects_db->close(cbdb->priv->objects_db, 0);
	}

	return FALSE;
}

/* open handler for the DB backend */
static CalBackendOpenStatus
cal_backend_db_open (CalBackend *backend, GnomeVFSURI *uri, gboolean only_if_exists)
{
	CalBackendDB *cbdb;
	gchar *str_uri;
	gint ret;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), CAL_BACKEND_OPEN_ERROR);
	g_return_val_if_fail(cbdb->priv != NULL, CAL_BACKEND_OPEN_ERROR);
	g_return_val_if_fail(uri != NULL, CAL_BACKEND_OPEN_ERROR);
	g_return_val_if_fail(cbdb->priv->objects_db == NULL, CAL_BACKEND_OPEN_ERROR);

	/* open the given URI */
	if (!gnome_vfs_uri_is_local(uri))
		return CAL_BACKEND_OPEN_ERROR;
	str_uri = gnome_vfs_uri_to_string(uri,
					  (GNOME_VFS_URI_HIDE_USER_NAME
					   | GNOME_VFS_URI_HIDE_PASSWORD
					   | GNOME_VFS_URI_HIDE_HOST_NAME
					   | GNOME_VFS_URI_HIDE_HOST_PORT
					   | GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD));

	/* open database file */
	if (!open_database_file(cbdb, (const gchar *) str_uri, only_if_exists)) {
		g_free((gpointer) str_uri);
		return CAL_BACKEND_OPEN_ERROR;
	}

	gnome_vfs_uri_ref(uri);
	cbdb->priv->uri = uri;
	g_free((gpointer) str_uri);

	return CAL_BACKEND_OPEN_SUCCESS;
}

/* get_n_objects handler for the DB backend */
static int
cal_backend_db_get_n_objects (CalBackend *backend, CalObjType type)
{
	CalBackendDB *cbdb;
	CalBackendDBCursor *cursor;
	int total_count = 0;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), -1);
	g_return_val_if_fail(cbdb->priv != NULL, -1);

	/* open the cursor */
	cursor = open_cursor(cbdb, cbdb->priv->objects_db);
	if (cursor) {
		GList *node;

		/* we traverse all data, to check for each object's type */
		for (node = g_list_first(cursor->data); node != NULL; node = g_list_next(node)) {
			icalcomponent *icalcomp;
			DBT *data = (DBT *) node->data;

			icalcomp = icalparser_parse_string((char *) data->data);
			if (icalcomp) {
				switch (icalcomponent_isa(icalcomp)) {
				case ICAL_VEVENT_COMPONENT :
					if (type & CALOBJ_TYPE_EVENT)
						total_count++;
					break;
				case ICAL_VTODO_COMPONENTS :
					if (type & CALOBJ_TYPE_TODO)
						total_count++;
					break;
				case ICAL_VJOURNAL_COMPONENT :
					if (type & CALOBJ_TYPE_JOURNAL)
						total_count++;
					break;
				}
				icalcomponent_free(icalcomp);
			}
		}
		close_cursor(cbdb, cursor);
	}

	return total_count;
}

/* get_object handler for the DB backend */
static char *
cal_backend_db_get_object (CalBackend *backend, const char *uid)
{
	CalBackendDB *cbdb;
	CalBackendDBCursor *cursor;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(cbdb->priv != NULL, NULL);
	g_return_val_if_fail(cbdb->priv->objects_db != NULL, NULL);
	g_return_val_if_fail(uid != NULL, NULL);

	/* open cursor */
	cursor = open_cursor(cbdb, cbdb->priv->objects_db);
	if (cursor) {
		gint ret;
		DBT *data;

		data = find_record_by_id(cursor, uid);
		if (data) {
			gchar *str = g_strdup((char *) data->data);
			close_cursor(cbdb, cursor);
			return str;
		}
		
		close_cursor(cbdb, cursor);
	}

	return NULL;
}

/* get_type_by_uid handler for the DB backend */
static CalObjType
cal_backend_db_get_type_by_uid (CalBackend *backend, const char *uid)
{
	CalBackendDB *cbdb;
	CalBackendDBCursor *cursor;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), CAL_COMPONENT_NO_TYPE);
	g_return_val_if_fail(cbdb->priv != NULL, CAL_COMPONENT_NO_TYPE);
	g_return_val_if_fail(cbdb->priv->objects_db != NULL, CAL_COMPONENT_NO_TYPE);
	g_return_val_if_fail(uid != NULL, CAL_COMPONENT_NO_TYPE);

	/* open the cursor */
	cursor = open_cursor(cbdb, cbdb->priv->objects_db);
	if (cursor) {
		DBT *data = find_record_by_id(cursor, uid);

		if (data) {
			icalcomponent icalcomp = icalparser_parse_string((char *) data->data);
			if (icalcomp) {
				CalObjType type;

				switch (icalcomponent_isa(icalcomp)) {
				case ICAL_VEVENT_COMPONENT :
					type = CALOBJ_TYPE_EVENT;
					break;
				case ICAL_VTODO_COMPONENT :
					type = CALOBJ_TYPE_TODO;
					break;
				case ICAL_VJOURNAL_COMPONENT :
					type = CALOBJ_TYPE_JOURNAL;
					break;
				default :
					type CAL_COMPONENT_NO_TYPE;
				}

				icalcomponent_free(icalcomp);
				close_cursor(cbdb, cursor);
				return type;
			}
		}
		close_cursor(cbdb, cursor);
	}

	return CAL_COMPONENT_NO_TYPE;
}

static GList *
add_uid_if_match (GList *list, CalBackendDBCursor *cursor, GList *data_node, CalObjType type)
{
	DBT *data;

	g_return_val_if_fail(cursor != NULL, list);
	g_return_val_if_fail(data_node != NULL, list);
	
	data = (DBT *) data_node->data;
	if (data) {
		icalcomponent *icalcomp;
		gchar *uid;

		icalcomp = icalparser_parse_string(data->data);
		if (!icalcomp) return list;
		switch (icalcomponent_isa(icalcomp)) {
		case ICAL_VEVENT_COMPONENT :
			if (type & CALOBJ_TYPE_EVENT)
				uid = icalcomponent_get_uid(icalcomp);
			break;
		case ICAL_VTODO_COMPONENT :
			if (type & CALOBJ_TYPE_TODO)
				uid = icalcomponent_get_uid(icalcomp);
			break;
		case ICAL_VJOURNAL_COMPONENT :
			if (type & CALOBJ_TYPE_JOURNAL)
				uid = icalcomponent_get_uid(icalcomp);
			break;
		default :
			uid = NULL;
		}

		if (uid)
			list = g_list_prepend(list, g_strdup(uid));
		icalcomponent_free(icalcomp);
	}

	return list;
}

/* get_uids handler for the DB backend */
static GList *
cal_backend_db_get_uids (CalBackend *backend, CalObjType type)
{
	CalBackendDB *cbdb;
	GList *list = NULL;
	CalBackendDBCursor *cursor;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(cbdb->priv != NULL, NULL);
	g_return_val_if_fail(cbdb->priv->objects_db != NULL, NULL);

	/* open cursor */
	cursor = open_cursor(cbdb, cbdb->priv->objects_db);
	if (cursor) {
		GList *node;

		/* we traverse all data, to check for each object's type */
		for (node = g_list_first(cursor->data); node != NULL; node = g_list_next(node)) {
			list = add_uid_if_match(list, cursor, node, type);
		}
		close_cursor(cbdb, cursor);
	}

	return list;
}

/* callback used from cal_recur_generate_instances(): adds the component's UID to
 * our hash table
 */
static gboolean
add_instance (CalComponent *comp, time_t start, time_t end, gpointer data)
{
	GHashTable *uid_hash;
	const char *uid;
	const char *old_uid;

	uid_hash = data;

	cal_component_get_uid(comp, &uid);

	old_uid = g_hash_table_lookup(uid_hash, uid);
	if (old_uid)
		return FALSE;

	g_hash_table_insert(uid_hash, (char *) uid, NULL);
	return FALSE;
}

/* creates the list of UIDs in the given range */
static void
get_instances_in_range (GHashTable *uid_hash,
                        CalBackendDBCursor *cursor,
                        CalObjType type,
                        time_t start,
                        time_t end)
{
	GList *node;

	g_return_if_fail(uid_hash != NULL);
	g_return_if_fail(cursor != NULL);

	for (node = g_list_first(cursor->data); node != NULL; node = g_list_next(node)) {
		DBT *data;
		icalcomponent *icalcomp;

		data = (DBT *) node->data;
		if (data) {
			icalcomp = icalparser_parse_string((char *) data->data);
			if (icalcomp) {
				CalComponent *comp = cal_component_new();
				cal_component_set_icalcomponent(comp, icalcomp);

				switch (icalcomponent_isa(icalcomp)) {
				case ICAL_VEVENT_COMPONENT :
					if (type & CALOBJ_TYPE_EVENT)
						cal_recur_generate_instances(comp,
						                             start,
						                             end,
						                             add_instance,
						                             uid_hash);
					break;
				case ICAL_VTODO_COMPONENT :
					if (type & CALOBJ_TYPE_TODO)
						cal_recur_generate_instances(comp,
						                             start,
						                             end,
						                             add_instance,
						                             uid_hash);
					break;
				case ICAL_VJOURNAL_COMPONENT :
					if (type & CALOBJ_TYPE_JOURNAL)
						cal_recur_generate_instances(comp,
						                             start,
						                             end,
						                             add_instance,
						                             uid_hash);
					break;
				}

				gtk_object_unref(comp);
				icalcomponent_free(icalcomp);
			}
		}
	}
}

/* callback used from g_hash_table_foreach: adds a UID from the hash table to our list */
static void
add_uid_to_list (gpointer key, gpointer value, gpointer data)
{
	GList **list;
	const char *uid;

	list = (GList **) data;

	uid = (const char *) key;
	*list = g_list_prepend(*list, (gpointer) g_strdup(uid));
}

/* get_objects_in_range handler for the DB backend */
static GList *
cal_backend_db_get_objects_in_range (CalBackend *backend,
                                     CalObjType type,
                                     time_t start,
                                     time_t end)
{
	CalBackendDB *cbdb;
	GList *list = NULL;
	CalBackendDBCursor *cursor;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(cbdb->priv != NULL, NULL);

	/* open cursor */
	cursor = open_cursor(cbdb, cbdb->priv->objects_db);
	if (cursor) {
		GHashTable *uid_hash;

		/* build the hash table */
		uid_hash = g_hash_table_new(g_str_hash, g_str_equal);
		get_instances_in_range(uid_hash, cursor, type, start, end);

		/* build the list to be returned from the hash table */
		g_hash_table_foreach(uid_hash, add_uid_to_list, &list);
		g_hash_table_destroy(uid_hash);
		
		close_cursor(cbdb, cursor);
	}
	
	return list;
}

/* get_changes handler for the DB backend */
static GNOME_Evolution_Calendar_CalObjChangeSeq *
cal_backend_db_get_changes (CalBackend *backend, CalObjType type, const char *change_id)
{
	CalBackendDB *cbdb;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(cbdb->priv != NULL, NULL);

	return NULL;
}

/* get_alarms_in_range handler for the DB backend */
static GNOME_Evolution_Calendar_CalComponentAlarmsSeq *
cal_backend_db_get_alarms_in_range (CalBackend *backend, time_t start, time_t end)
{
	CalBackendDB *cbdb;
	CalBackendDBCursor *cursor;
	gint number_of_alarms;
	GList *alarm_list;
	GList *node;
	gint i;
	GNOME_Evolution_Calendar_CalComponentAlarmsSeq *seq = NULL;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(cbdb->priv != NULL, NULL);
	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	/* open cursor */
	cursor = open_cursor(cbdb, cbdb->priv->objects_db);
	if (cursor) {
		/* TODO: get list of alarms */
		
		/* create the CORBA sequence */
		seq = GNOME_Evolution_Calendar_CalComponentAlarmsSeq__alloc();
		CORBA_sequence_set_release(seq, TRUE);
		seq->_length = number_of_alarms;
		seq->_buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalComponentAlarms_allocbuf(number_of_alarms);
		
		/* TODO: populate CORBA sequence */
		
		close_cursor(cbdb, cursor);
	}
	
	g_list_free(alarm_list);

	return seq;
}

/* get_alarms_for_object handler for the DB backend */
static GNOME_Evolution_Calendar_CalComponentAlarms *
cal_backend_db_get_alarms_for_object (CalBackend *backend,
                                      const char *uid,
                                      time_t start,
                                      time_t end,
                                      gboolean *object_found)
{
	CalBackendDB *cbdb;
	CalBackendDBCursor *cursor;
	GNOME_Evolution_Calendar_CalComponentAlarms *corba_alarms = NULL;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), NULL);
	g_return_val_if_fail(cbdb->priv != NULL, NULL);
	g_return_val_if_fail(uid != NULL, NULL);
	g_return_val_if_fail(start != -1 && end != -1, NULL);
	g_return_val_if_fail(start <= end, NULL);
	g_return_val_if_fail(object_found != NULL, NULL);

	/* open the cursor */
	cursor = open_cursor(cbdb, cbdb->priv->object_db);
	if (cursor) {
		/* TODO: retrieve list of alarms for this object */
		
		/* create the CORBA alarms */
		corba_alarms = GNOME_Evolution_Calendar_CalComponentAlarms__alloc();
		
		/* TODO: populate the CORBA alarms */
		
		close_cursor(cbdb, cursor);
	}

	return corba_alarms;
}

/* update_object handler for the DB backend */
static gboolean
cal_backend_db_update_object (CalBackend *backend, const char *uid, const char *calobj)
{
	CalBackendDB *cbdb;
	CalBackendDBCursor *cursor;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), FALSE);
	g_return_val_if_fail(cbdb->priv != NULL, FALSE);
	g_return_val_if_fail(uid != NULL, FALSE);
	g_return_val_if_fail(calobj != NULL, FALSE);

	/* open the cursor */
	cursor = open_cursor(cbdb, cbdb->priv->objects_db);
	if (cursor) {
		DBT *data;
		
		data = find_record_by_id(cursor, uid);
		if (data) {
			DBT key;
			DBT new_data;
			int ret;
			DB_TXN *tid;
			
			/* try to change the value in the cursor */
			memset(&key, 0, sizeof(key));
			key.data = (void *) uid;
			key.size = strlen(uid); // + 1
			
			memset(&new_data, 0, sizeof(new_data));
			new_data.data = (void *) calobj;
			new_data.size = strlen(calobj); // + 1
			
			/* start transaction */
			tid = begin_transaction(cbdb);
			if (!tid) {
				close_cursor(cbdb, cursor);
				return FALSE;
			}
			
			if ((ret = cursor->parent_db->put(cursor->parent_db,
			                                  tid,
			                                  &key,
			                                  &new_data,
			                                  0)) != 0) {
				rollback_transaction(tid);
				close_cursor(cbdb, cursor);
				return FALSE;
			}
			
			/* TODO: update history database */
			commit_transaction(tid);
			close_cursor(cbdb, cursor);
			
			memcpy(data, &new_data, sizeof(new_data));
			
			return TRUE;
		}
		close_cursor(cbdb, cursor);
	}
	
	return FALSE;
}

/* remove_object handler for the DB backend */
static gboolean
cal_backend_db_remove_object (CalBackend *backend, const char *uid)
{
	CalBackendDB *cbdb;
	CalBackendDBCursor *cursor;

	cbdb = CAL_BACKEND_DB(backend);
	g_return_val_if_fail(IS_CAL_BACKEND_DB(cbdb), FALSE);
	g_return_val_if_fail(cbdb->priv != NULL, FALSE);
	g_return_val_if_fail(uid != NULL, FALSE);
	
	/* open cursor */
	cursor = open_cursor(cbdb, cbdb->priv->objects_db);
	if (cursor) {
		DBT *data = find_record_by_id(cursor, uid);
		if (data) {
			GList *l;
			int ret;
			DBT key;
			DB_TXN *tid;
			
			memset(&key, 0, sizeof(key);
			key.data = (void *) uid;
			key.size = strlen(uid); // + 1
			
			/* start transaction */
			tid = begin_transaction(cbdb);
			if (!tid) {
				close_cursor(cbdb, cursor);
				return FALSE;
			}
			
			/* remove record from cursor */
			if ((ret = cursor->parent_db->del(cursor->parent_db, tid, key, 0)) != 0) {
				rollback_transaction(tid);
				close_cursor(cbdb, cursor);
				return FALSE;
			}
			
			/* TODO: update history database */
			commit_transaction(tid);
			
			/* remove record from in-memory lists */
			l = g_list_nth(cursor->keys,
			               g_list_index(cursor->data, (gpointer) data));
			if (l) {
				DBT *key_to_free = (DBT *) l->data;
				
				cursor->keys = g_list_remove(cursor->keys, (gpointer) key_to_free);
				g_free((gpointer) key_to_free);
				
				cursor->data = g_list_remove(cursor->data, (gpointer) data);
				g_free((gpointer) data);
			}
			close_cursor(cbdb, cursor);
			
			return TRUE;
		}
		close_cursor(cbdb, cursor);
	}
	
	return FALSE;
}
