/* Evolution calendar client
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
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
#include "cal-util/cal-util-marshal.h"
#include "cal-client-multi.h"

/* Private part of the CalClientMulti structure */
struct _CalClientMultiPrivate {
	GHashTable *calendars;
	GList *uris;
};

static void cal_client_multi_class_init (CalClientMultiClass *klass);
static void cal_client_multi_init       (CalClientMulti *multi, CalClientMultiClass *klass);
static void cal_client_multi_finalize   (GObject *object);

/* signal IDs */
enum {
	CAL_OPENED,
	OBJ_UPDATED,
	OBJ_REMOVED,
	CATEGORIES_CHANGED,
	FORGET_PASSWORD,
	LAST_SIGNAL
};

static guint cal_multi_signals[LAST_SIGNAL];
static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

/**
 * cal_client_multi_get_type
 *
 * Registers the #CalClientMulti class if necessary, and returns the type ID
 * assigned to it.
 *
 * Returns: The type ID of the #CalClientMulti class
 */
GType
cal_client_multi_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
                        sizeof (CalClientMultiClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) cal_client_multi_class_init,
                        NULL, NULL,
                        sizeof (CalClientMulti),
                        0,
                        (GInstanceInitFunc) cal_client_multi_init
                };
		type = g_type_register_static (G_TYPE_OBJECT, "CalClientMulti", &info, 0);
	}

	return type;
}

/* class initialization function for the multi calendar client */
static void
cal_client_multi_class_init (CalClientMultiClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	
	cal_multi_signals[CAL_OPENED] =
		g_signal_new ("cal_opened",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalClientMultiClass, cal_opened),
			      NULL, NULL,
			      cal_util_marshal_VOID__POINTER_ENUM,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER, G_TYPE_ENUM);
	cal_multi_signals[OBJ_UPDATED] =
		g_signal_new ("obj_updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalClientMultiClass, obj_updated),
			      NULL, NULL,
			      cal_util_marshal_VOID__POINTER_STRING,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER, G_TYPE_STRING);
	cal_multi_signals[OBJ_REMOVED] =
		g_signal_new ("obj_removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalClientMultiClass, obj_removed),
			      NULL, NULL,
			      cal_util_marshal_VOID__POINTER_STRING,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER, G_TYPE_STRING);
	cal_multi_signals[CATEGORIES_CHANGED] =
		g_signal_new ("categories_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalClientMultiClass, categories_changed),
			      NULL, NULL,
			      cal_util_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER, G_TYPE_POINTER);
	cal_multi_signals[FORGET_PASSWORD] =
		g_signal_new ("forget_password",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalClientMultiClass, forget_password),
			      NULL, NULL,
			      cal_util_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING, G_TYPE_STRING);

	object_class->finalize = cal_client_multi_finalize;
}

/* object initialization function for the multi calendar client */
static void
cal_client_multi_init (CalClientMulti *multi, CalClientMultiClass *klass)
{
	multi->priv = g_new0 (CalClientMultiPrivate, 1);
	multi->priv->calendars = g_hash_table_new (g_str_hash, g_str_equal);
	multi->priv->uris = NULL;
}

static void
free_calendar (gpointer key, gpointer value, gpointer data)
{
	CalClientMulti *multi = (CalClientMulti *) data;

	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));

	multi->priv->uris = g_list_remove (multi->priv->uris, key);

	g_free (key);
	g_object_unref (G_OBJECT (value));
}

/* finalize handler for the multi calendar client */
static void
cal_client_multi_finalize (GObject *object)
{
	CalClientMulti *multi = (CalClientMulti *) object;

	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));

	/* free memory */
	g_hash_table_foreach (multi->priv->calendars, free_calendar, multi);
	g_hash_table_destroy (multi->priv->calendars);
	g_list_free (multi->priv->uris);

	g_free (multi->priv);
	multi->priv = NULL;

	/* chain to parent class' destroy handler */
	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/**
 * cal_client_multi_new
 *
 * Creates a new multi-calendar client. This allows you to merge several
 * #CalClient objects into one entity, making it easier to manage
 * multiple calendars.
 *
 * Returns: A newly-created multi-calendar client.
 */
CalClientMulti *
cal_client_multi_new (void)
{
	CalClientMulti *multi;

	multi = g_object_new (CAL_CLIENT_MULTI_TYPE, NULL);
	return multi;
}

/* CalClient's signal handlers */
static void
client_cal_opened_cb (CalClient *client, CalClientOpenStatus status, gpointer user_data)
{
	CalClientMulti *multi = (CalClientMulti *) user_data;

	g_return_if_fail (IS_CAL_CLIENT (client));
	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));

	g_signal_emit (G_OBJECT (multi),
		       cal_multi_signals[CAL_OPENED], 0,
		       client, status);
}

static void
client_obj_updated_cb (CalClient *client, const char *uid, gpointer user_data)
{
	CalClientMulti *multi = (CalClientMulti *) user_data;

	g_return_if_fail (IS_CAL_CLIENT (client));
	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));

	g_signal_emit (G_OBJECT (multi),
		       cal_multi_signals[OBJ_UPDATED], 0,
		       client, uid);
}

static void
client_obj_removed_cb (CalClient *client, const char *uid, gpointer user_data)
{
	CalClientMulti *multi = (CalClientMulti *) user_data;

	g_return_if_fail (IS_CAL_CLIENT (client));
	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));

	g_signal_emit (G_OBJECT (multi),
		       cal_multi_signals[OBJ_REMOVED], 0,
		       client, uid);
}

static void
client_categories_changed_cb (CalClient *client, GPtrArray *categories, gpointer user_data)
{
	CalClientMulti *multi = (CalClientMulti *) user_data;

	g_return_if_fail (IS_CAL_CLIENT (client));
	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));

	g_signal_emit (G_OBJECT (multi),
		       cal_multi_signals[CATEGORIES_CHANGED], 0,
		       client, categories);
}

static void
client_forget_password_cb (CalClient *client, const char *key, gpointer user_data)
{
	CalClientMulti *multi = (CalClientMulti *) user_data;

	g_return_if_fail (IS_CAL_CLIENT (client));
	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));

	g_signal_emit (G_OBJECT (multi),
		       cal_multi_signals[FORGET_PASSWORD], 0,
		       client, key);
}
/**
 * cal_client_multi_add_client
 * @multi: A #CalClientMulti object.
 * @client: The #CalClient object to be added.
 *
 * Aggregates the given #CalClient to a #CalClientMulti object,
 * thus adding it to the list of managed calendars.
 */
void
cal_client_multi_add_client (CalClientMulti *multi, CalClient *client)
{
	char *uri;
	CalClient *old_client;

	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));
	g_return_if_fail (IS_CAL_CLIENT (client));

	uri = g_strdup (cal_client_get_uri (client));
	old_client = g_hash_table_lookup (multi->priv->calendars, uri);
	if (old_client) {
		g_free (uri);
		return;
	}

	g_object_ref (G_OBJECT (client));
	multi->priv->uris = g_list_append (multi->priv->uris, uri);
	g_hash_table_insert (multi->priv->calendars, uri, client);

	/* set up CalClient's signal handlers */
	g_signal_handlers_disconnect_matched (G_OBJECT (client), 
					      G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, multi);
	g_signal_connect (G_OBJECT (client),
			  "cal_opened",
			  G_CALLBACK (client_cal_opened_cb),
			  multi);
	g_signal_connect (G_OBJECT (client),
			  "obj_updated",
			  G_CALLBACK (client_obj_updated_cb),
			  multi);
	g_signal_connect (G_OBJECT (client),
			  "obj_removed",
			  G_CALLBACK (client_obj_removed_cb),
			  multi);
	g_signal_connect (G_OBJECT (client),
			  "categories_changed",
			  G_CALLBACK (client_categories_changed_cb),
			  multi);
	g_signal_connect (G_OBJECT (client),
			  "forget_password",
			  G_CALLBACK (client_forget_password_cb),
			  multi);
}

typedef struct {
	CalClientAuthFunc func;
	gpointer user_data;
} AuthFuncData;

static void
set_auth_func (gpointer key, gpointer value, gpointer user_data)
{
	AuthFuncData *cb_data = (AuthFuncData *) user_data;
	CalClient *client = (CalClient *) value;

	g_return_if_fail (IS_CAL_CLIENT (client));
	g_return_if_fail (cb_data != NULL);

	cal_client_set_auth_func (client, cb_data->func, cb_data->user_data);
}

/**
 * cal_client_multi_set_auth_func
 * @multi: A #CalClientMulti object.
 * @func: The authentication function.
 * @user_data: Data to be passed to the authentication function.
 *
 * Sets the authentication function for all the clients in the
 * given #CalClientMulti.
 */
void
cal_client_multi_set_auth_func (CalClientMulti *multi,
				CalClientAuthFunc func,
				gpointer user_data)
{
	AuthFuncData *cb_data;

	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));

	cb_data = g_new0 (AuthFuncData, 1);
	cb_data->func = func;
	cb_data->user_data = user_data;
	g_hash_table_foreach (multi->priv->calendars, set_auth_func, cb_data);

	g_free (cb_data);
}

/**
 * cal_client_multi_open_calendar
 * @multi: A #CalClientMulti object.
 * @str_uri: The URI of the calendar to be open
 * @only_if_exists:
 *
 * Open a new calendar in the given #CalClientMulti object.
 *
 * Returns: a pointer to the new #CalClient
 */
CalClient *
cal_client_multi_open_calendar (CalClientMulti *multi,
				const char *str_uri,
				gboolean only_if_exists)
{
	CalClient *client;
	gboolean result;

	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), FALSE);

	client = cal_client_new ();

	result = cal_client_open_calendar (client, str_uri, only_if_exists);
	if (result) {
		cal_client_multi_add_client (multi, client);
		g_object_unref (G_OBJECT (client));
		return client;
	}

	g_object_unref (G_OBJECT (client));

	return NULL;
}

/**
 * cal_client_multi_get_client_for_uri
 * @multi: A #CalClientMulti object.
 * @uri: The URI for the client.
 *
 * Returns the #CalClient object associated with the given
 * @uri for the given #CalClientMulti object.
 *
 * Returns: a pointer to the client or NULL if no client is
 * associated with that URI.
 */
CalClient *
cal_client_multi_get_client_for_uri (CalClientMulti *multi, const char *uri)
{
	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	return g_hash_table_lookup (multi->priv->calendars, uri);
}

/**
 * cal_client_multi_get_n_objects
 * @multi: A #CalClientMulti object.
 * @type: Type for objects
 *
 * Get the count of objects of the given type(s) for a #CalClientMulti
 * object.
 *
 * Returns: The count of objects of the given type(s).
 */
int
cal_client_multi_get_n_objects (CalClientMulti *multi,
				CalObjType type)
{
	CalClient *client;
	GList *l;
	int count = 0;

	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), -1);

	for (l = multi->priv->uris; l; l = l->next) {
		client = cal_client_multi_get_client_for_uri (multi,
							      (const char *) l->data);
		if (IS_CAL_CLIENT (client))
			count += cal_client_get_n_objects (client, type);
	}

	return count;
}

/**
 * cal_client_multi_get_object
 */
CalClientGetStatus
cal_client_multi_get_object (CalClientMulti *multi,
			     const char *uid,
			     CalComponent **comp)
{
	CalClient *client;
	GList *l;

	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), CAL_CLIENT_GET_NOT_FOUND);
	g_return_val_if_fail (uid != NULL, CAL_CLIENT_GET_NOT_FOUND);

	for (l = multi->priv->uris; l; l = l->next) {
		client = cal_client_multi_get_client_for_uri (multi,
							      (const char *) l->data);
		if (IS_CAL_CLIENT (client)) {
			CalClientGetStatus status;

			status = cal_client_get_object (client, uid, comp);
			if (status == CAL_CLIENT_GET_SUCCESS)
				return status;
		}
	}

	return CAL_CLIENT_GET_NOT_FOUND;
}

/**
 * cal_client_multi_get_timezone
 * @multi: A #CalClientMulti object.
 * @tzid: ID for the timezone to be retrieved.
 * @zone: A pointer to where the icaltimezone object will be copied.
 */
CalClientGetStatus
cal_client_multi_get_timezone (CalClientMulti *multi,
			       const char *tzid,
			       icaltimezone **zone)
{
	CalClient *client;
	GList *l;

	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), CAL_CLIENT_GET_NOT_FOUND);
	g_return_val_if_fail (tzid != NULL, CAL_CLIENT_GET_NOT_FOUND);

	for (l = multi->priv->uris; l; l = l->next) {
		client = cal_client_multi_get_client_for_uri (multi,
							      (const char *) l->data);
		if (IS_CAL_CLIENT (client)) {
			CalClientGetStatus status;

			status = cal_client_get_timezone (client, tzid, zone);
			if (status == CAL_CLIENT_GET_SUCCESS)
				return status;
		}
	}

	return CAL_CLIENT_GET_NOT_FOUND;
}

/**
 * cal_client_multi_get_uids
 * @multi: A #CalClientMulti object.
 * @type: Type of objects whose IDs will be returned.
 *
 * Returns a list of UIDs for all the objects of the given
 * type(s) that are in the calendars managed by a
 * #CalClientMulti object
 *
 * Returns: a GList of UIDs.
 */
GList *
cal_client_multi_get_uids (CalClientMulti *multi, CalObjType type)
{
	CalClient *client;
	GList *l;
	GList *result = NULL;

	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), NULL);

	for (l = multi->priv->uris; l; l = l->next) {
		client = cal_client_multi_get_client_for_uri (multi,
							      (const char *) l->data);
		if (IS_CAL_CLIENT (client)) {
			GList *tmp;

			tmp = cal_client_get_uids (client, type);
			if (tmp)
				result = g_list_concat (result, tmp);
		}
	}

	return result;
}

/**
 * cal_client_multi_get_changes
 * @multi: A #CalClientMulti object.
 * @type: Object type.
 * @change_id: Change ID.
 *
 * Returns a list of changes for the given #CalClientMulti
 * object.
 */
GList *
cal_client_multi_get_changes (CalClientMulti *multi,
			      CalObjType type,
			      const char *change_id)
{
	CalClient *client;
	GList *l;
	GList *result = NULL;

	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), NULL);

	for (l = multi->priv->uris; l; l = l->next) {
		client = cal_client_multi_get_client_for_uri (multi,
							      (const char *) l->data);
		if (IS_CAL_CLIENT (client)) {
			GList *tmp;

			tmp = cal_client_get_changes (client, type, change_id);
			if (tmp)
				result = g_list_concat (result, tmp);
		}
	}

	return result;
}

/**
 * cal_client_multi_get_objects_in_range
 * @multi: A #CalClientMulti object.
 * @type: Type for objects.
 * @start: Start time.
 * @end: End time.
 *
 * Retrieves a list of all calendar components that are
 * scheduled within the given time range. The information is
 * retrieved from all the calendars being managed by the
 * given #CalClientMulti object.
 *
 * Returns: A list of UID strings. This should be freed using the
 * #cal_obj_uid_list_free() function.
 **/
GList *
cal_client_multi_get_objects_in_range (CalClientMulti *multi,
				       CalObjType type,
				       time_t start,
				       time_t end)
{
	CalClient *client;
	GList *l;
	GList *result = NULL;

	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), NULL);

	for (l = multi->priv->uris; l; l = l->next) {
		client = cal_client_multi_get_client_for_uri (multi,
							      (const char *) l->data);
		if (IS_CAL_CLIENT (client)) {
			GList *tmp;

			tmp = cal_client_get_objects_in_range (client, type, start, end);
			if (tmp)
				result = g_list_concat (result, tmp);
		}
	}

	return result;
}

/**
 * cal_client_multi_get_free_busy
 * @multi: A #CalClientMulti object.
 * @users: List of users to retrieve F/B information for.
 * @start: Start time.
 * @end: End time.
 *
 * Retrieves Free/Busy information for the given users in all
 * the calendars being managed by the given #CalClient multi
 * object.
 *
 * Returns: A GList of VFREEBUSY CalComponents
 */
GList *
cal_client_multi_get_free_busy (CalClientMulti *multi,
				GList *users,
				time_t start,
				time_t end)
{
	CalClient *client;
	GList *l;
	GList *result = NULL;

	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), NULL);

	for (l = multi->priv->uris; l; l = l->next) {
		client = cal_client_multi_get_client_for_uri (multi,
							      (const char *) l->data);
		if (IS_CAL_CLIENT (client)) {
			GList *tmp;

			tmp = cal_client_get_free_busy (client, users, start, end);
			if (tmp)
				result = g_list_concat (result, tmp);
		}
	}

	return result;
}

/**
 * cal_client_multi_generate_instances
 */
void
cal_client_multi_generate_instances (CalClientMulti *multi,
				     CalObjType type,
				     time_t start,
				     time_t end,
				     CalRecurInstanceFn cb,
				     gpointer cb_data)
{
	CalClient *client;
	GList *l;

	g_return_if_fail (IS_CAL_CLIENT_MULTI (multi));

	for (l = multi->priv->uris; l; l = l->next) {
		client = cal_client_multi_get_client_for_uri (multi,
							      (const char *) l->data);
		if (IS_CAL_CLIENT (client)) {
			cal_client_generate_instances (
				client, type, start, end, cb, cb_data);
		}
	}
}

/**
 * cal_client_multi_get_alarms_in_range
 */
GSList *
cal_client_multi_get_alarms_in_range (CalClientMulti *multi, time_t start, time_t end)
{
	CalClient *client;
	GList *l;
	GSList *result = NULL;

	g_return_val_if_fail (IS_CAL_CLIENT_MULTI (multi), NULL);

	for (l = multi->priv->uris; l; l = l->next) {
		client = cal_client_multi_get_client_for_uri (multi,
							      (const char *) l->data);
		if (IS_CAL_CLIENT (client)) {
			GSList *tmp;

			tmp = cal_client_get_alarms_in_range (client, start, end);
			if (tmp)
				result = g_slist_concat (result, tmp);
		}
	}

	return result;
}
