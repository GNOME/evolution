/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		JP Rosevear  <jpr@ximian.com>
 *	    Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <libecal/e-cal-component.h>
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-data-server-util.h>
#include "calendar-config.h"
#include "itip-utils.h"
#include "e-meeting-utils.h"
#include "e-meeting-attendee.h"
#include "e-meeting-store.h"

#define ROW_VALID(store, row) (row >= 0 && row < store->priv->attendees->len)

struct _EMeetingStorePrivate {
	GPtrArray *attendees;
	gint stamp;

	ECal *client;
	icaltimezone *zone;

	gchar *fb_uri;

	GPtrArray *refresh_queue;
	GHashTable *refresh_data;
	GMutex *mutex;
	guint refresh_idle_id;

	guint num_threads;
	guint num_queries;
};

#define BUF_SIZE 1024

typedef struct _EMeetingStoreQueueData EMeetingStoreQueueData;
struct _EMeetingStoreQueueData {
	EMeetingStore *store;
	EMeetingAttendee *attendee;

	gboolean refreshing;

	EMeetingTime start;
	EMeetingTime end;

	gchar buffer[BUF_SIZE];
	GString *string;

	GPtrArray *call_backs;
	GPtrArray *data;
};

static GObjectClass *parent_class = NULL;

static icalparameter_cutype
text_to_type (const gchar *type)
{
	if (!e_util_utf8_strcasecmp (type, _("Individual")))
		return ICAL_CUTYPE_INDIVIDUAL;
	else if (!e_util_utf8_strcasecmp (type, _("Group")))
		return ICAL_CUTYPE_GROUP;
	else if (!e_util_utf8_strcasecmp (type, _("Resource")))
		return ICAL_CUTYPE_RESOURCE;
	else if (!e_util_utf8_strcasecmp (type, _("Room")))
		return ICAL_CUTYPE_ROOM;
	else
		return ICAL_CUTYPE_NONE;
}

static gchar *
type_to_text (icalparameter_cutype type)
{
	switch (type) {
	case ICAL_CUTYPE_INDIVIDUAL:
		return _("Individual");
	case ICAL_CUTYPE_GROUP:
		return _("Group");
	case ICAL_CUTYPE_RESOURCE:
		return _("Resource");
	case ICAL_CUTYPE_ROOM:
		return _("Room");
	default:
		return _("Unknown");
	}

	return NULL;

}

static icalparameter_role
text_to_role (const gchar *role)
{
	if (!e_util_utf8_strcasecmp (role, _("Chair")))
		return ICAL_ROLE_CHAIR;
	else if (!e_util_utf8_strcasecmp (role, _("Required Participant")))
		return ICAL_ROLE_REQPARTICIPANT;
	else if (!e_util_utf8_strcasecmp (role, _("Optional Participant")))
		return ICAL_ROLE_OPTPARTICIPANT;
	else if (!e_util_utf8_strcasecmp (role, _("Non-Participant")))
		return ICAL_ROLE_NONPARTICIPANT;
	else
		return ICAL_ROLE_NONE;
}

static gchar *
role_to_text (icalparameter_role role)
{
	switch (role) {
	case ICAL_ROLE_CHAIR:
		return _("Chair");
	case ICAL_ROLE_REQPARTICIPANT:
		return _("Required Participant");
	case ICAL_ROLE_OPTPARTICIPANT:
		return _("Optional Participant");
	case ICAL_ROLE_NONPARTICIPANT:
		return _("Non-Participant");
	default:
		return _("Unknown");
	}
}

static gboolean
text_to_boolean (const gchar *role)
{
	if (!e_util_utf8_strcasecmp (role, _("Yes")))
		return TRUE;
	else
		return FALSE;
}

static gchar *
boolean_to_text (gboolean b)
{
	if (b)
		return _("Yes");
	else
		return _("No");
}

static icalparameter_partstat
text_to_partstat (const gchar *partstat)
{
	if (!e_util_utf8_strcasecmp (partstat, _("Needs Action")))
		return ICAL_PARTSTAT_NEEDSACTION;
	else if (!e_util_utf8_strcasecmp (partstat, _("Accepted")))
		return ICAL_PARTSTAT_ACCEPTED;
	else if (!e_util_utf8_strcasecmp (partstat, _("Declined")))
		return ICAL_PARTSTAT_DECLINED;
	else if (!e_util_utf8_strcasecmp (partstat, _("Tentative")))
		return ICAL_PARTSTAT_TENTATIVE;
	else if (!e_util_utf8_strcasecmp (partstat, _("Delegated")))
		return ICAL_PARTSTAT_DELEGATED;
	else if (!e_util_utf8_strcasecmp (partstat, _("Completed")))
		return ICAL_PARTSTAT_COMPLETED;
	else if (!e_util_utf8_strcasecmp (partstat, _("In Process")))
		return ICAL_PARTSTAT_INPROCESS;
	else
		return ICAL_PARTSTAT_NONE;
}

static gchar *
partstat_to_text (icalparameter_partstat partstat)
{
	switch (partstat) {
	case ICAL_PARTSTAT_NEEDSACTION:
		return _("Needs Action");
	case ICAL_PARTSTAT_ACCEPTED:
		return _("Accepted");
	case ICAL_PARTSTAT_DECLINED:
		return _("Declined");
	case ICAL_PARTSTAT_TENTATIVE:
		return _("Tentative");
	case ICAL_PARTSTAT_DELEGATED:
		return _("Delegated");
	case ICAL_PARTSTAT_COMPLETED:
		return _("Completed");
	case ICAL_PARTSTAT_INPROCESS:
		return _("In Process");
	case ICAL_PARTSTAT_NONE:
	default:
		return _("Unknown");
	}
}

static GtkTreeModelFlags
get_flags (GtkTreeModel *model)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (model), 0);

	return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
get_n_columns (GtkTreeModel *model)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (model), 0);

	return E_MEETING_STORE_COLUMN_COUNT;
}

static GType
get_column_type (GtkTreeModel *model, gint col)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (model), G_TYPE_INVALID);

	switch (col) {
	case E_MEETING_STORE_ADDRESS_COL:
	case E_MEETING_STORE_MEMBER_COL:
	case E_MEETING_STORE_TYPE_COL:
	case E_MEETING_STORE_ROLE_COL:
	case E_MEETING_STORE_RSVP_COL:
	case E_MEETING_STORE_DELTO_COL:
	case E_MEETING_STORE_DELFROM_COL:
	case E_MEETING_STORE_STATUS_COL:
	case E_MEETING_STORE_CN_COL:
	case E_MEETING_STORE_LANGUAGE_COL:
	case E_MEETING_STORE_ATTENDEE_COL:
		return G_TYPE_STRING;
	case E_MEETING_STORE_ATTENDEE_UNDERLINE_COL:
		return PANGO_TYPE_UNDERLINE;
	default:
		return G_TYPE_INVALID;
	}
}

static gboolean
get_iter (GtkTreeModel *model, GtkTreeIter *iter, GtkTreePath *path)
{
	gint row;

	g_return_val_if_fail (E_IS_MEETING_STORE (model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	row = gtk_tree_path_get_indices (path) [0];

	if (!ROW_VALID (E_MEETING_STORE (model), row))
	       return FALSE;

	iter->stamp = E_MEETING_STORE (model)->priv->stamp;
	iter->user_data = GINT_TO_POINTER (row);

	return TRUE;
}

static GtkTreePath *
get_path (GtkTreeModel *model, GtkTreeIter *iter)
{
	gint row;
	GtkTreePath *result;

	g_return_val_if_fail (E_IS_MEETING_STORE (model), NULL);
	g_return_val_if_fail (iter->stamp == E_MEETING_STORE (model)->priv->stamp, NULL);

	row = GPOINTER_TO_INT (iter->user_data);

	g_return_val_if_fail (ROW_VALID (E_MEETING_STORE (model), row), NULL);

	result = gtk_tree_path_new ();
	gtk_tree_path_append_index (result, row);
	return result;
}

static void
get_value (GtkTreeModel *model, GtkTreeIter *iter, gint col, GValue *value)
{
	EMeetingStore *store;
	EMeetingAttendee *attendee;
	const gchar *cn;
	gint row;

	g_return_if_fail (E_IS_MEETING_STORE (model));
	g_return_if_fail (col >= 0 && col < E_MEETING_STORE_COLUMN_COUNT);

	row = GPOINTER_TO_INT (iter->user_data);
	store = E_MEETING_STORE (model);

	g_return_if_fail (iter->stamp == store->priv->stamp);
	g_return_if_fail (ROW_VALID (E_MEETING_STORE (model), row));

	attendee = g_ptr_array_index (store->priv->attendees, row);

	switch (col) {
	case E_MEETING_STORE_ADDRESS_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, itip_strip_mailto (e_meeting_attendee_get_address (attendee)));
		break;
	case E_MEETING_STORE_MEMBER_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, e_meeting_attendee_get_member (attendee));
		break;
	case E_MEETING_STORE_TYPE_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, type_to_text (e_meeting_attendee_get_cutype (attendee)));
		break;
	case E_MEETING_STORE_ROLE_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, role_to_text (e_meeting_attendee_get_role (attendee)));
		break;
	case E_MEETING_STORE_RSVP_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, boolean_to_text (e_meeting_attendee_get_rsvp (attendee)));
		break;
	case E_MEETING_STORE_DELTO_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, itip_strip_mailto (e_meeting_attendee_get_delto (attendee)));
		break;
	case E_MEETING_STORE_DELFROM_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, itip_strip_mailto (e_meeting_attendee_get_delfrom (attendee)));
		break;
	case E_MEETING_STORE_STATUS_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, partstat_to_text (e_meeting_attendee_get_status (attendee)));
		break;
	case E_MEETING_STORE_CN_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, e_meeting_attendee_get_cn (attendee));
		break;
	case E_MEETING_STORE_LANGUAGE_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, e_meeting_attendee_get_language (attendee));
		break;
	case E_MEETING_STORE_ATTENDEE_COL:
		g_value_init (value, G_TYPE_STRING);
		cn = e_meeting_attendee_get_cn (attendee);
		if (strcmp (cn, ""))
			g_value_set_string (value, cn);
		else
			g_value_set_string (value, itip_strip_mailto (e_meeting_attendee_get_address (attendee)));
		break;
	case E_MEETING_STORE_ATTENDEE_UNDERLINE_COL:
		cn = e_meeting_attendee_get_cn (attendee);
		g_value_init (value, PANGO_TYPE_UNDERLINE);
		g_value_set_enum (value, strcmp ("", cn) == 0 ? PANGO_UNDERLINE_NONE : PANGO_UNDERLINE_SINGLE);
	}
}

static gboolean
iter_next (GtkTreeModel *model, GtkTreeIter *iter)
{
	gint row;

	g_return_val_if_fail (E_IS_MEETING_STORE (model), FALSE);
	g_return_val_if_fail (iter->stamp == E_MEETING_STORE (model)->priv->stamp, FALSE);

	row = GPOINTER_TO_INT (iter->user_data) + 1;
	iter->user_data = GINT_TO_POINTER (row);

	return ROW_VALID (E_MEETING_STORE (model), row);
}

static gboolean
iter_children (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent)
{
	EMeetingStore *store;

	g_return_val_if_fail (E_IS_MEETING_STORE (model), FALSE);

	store = E_MEETING_STORE (model);

	if (parent || store->priv->attendees->len <= 0)
		return FALSE;

	iter->stamp = store->priv->stamp;
	iter->user_data = GINT_TO_POINTER (0);

	return TRUE;
}

static gboolean
iter_has_child (GtkTreeModel *model, GtkTreeIter *iter)
{
	return FALSE;
}

static gint
iter_n_children (GtkTreeModel *model, GtkTreeIter *iter)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (model), -1);

	if (!iter)
		return E_MEETING_STORE (model)->priv->attendees->len;

	g_return_val_if_fail (iter->stamp == E_MEETING_STORE (model)->priv->stamp, -1);

	return 0;
}

static gboolean
iter_nth_child (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent, gint n)
{
	EMeetingStore *store;

	g_return_val_if_fail (E_IS_MEETING_STORE (model), FALSE);

	store = E_MEETING_STORE (model);

	if (parent || !ROW_VALID (store, n))
		return FALSE;

	iter->stamp = store->priv->stamp;
	iter->user_data = GINT_TO_POINTER (n);

	return TRUE;
}

static gboolean
iter_parent (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *child)
{
	return FALSE;
}

static void
ems_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = get_flags;
	iface->get_n_columns = get_n_columns;
	iface->get_column_type = get_column_type;
	iface->get_iter = get_iter;
	iface->get_path = get_path;
	iface->get_value = get_value;
	iface->iter_next = iter_next;
	iface->iter_children = iter_children;
	iface->iter_has_child = iter_has_child;
	iface->iter_n_children = iter_n_children;
	iface->iter_nth_child = iter_nth_child;
	iface->iter_parent = iter_parent;
}

void
e_meeting_store_set_value (EMeetingStore *store, gint row, gint col, const gchar *val)
{
	icalparameter_cutype type;
	EMeetingAttendee *attendee = g_ptr_array_index (store->priv->attendees, row);

	switch (col) {
	case E_MEETING_STORE_ADDRESS_COL:
		if (val != NULL && *((gchar *)val))
			e_meeting_attendee_set_address (attendee, g_strdup_printf ("MAILTO:%s", (gchar *) val));
		break;
	case E_MEETING_STORE_MEMBER_COL:
		e_meeting_attendee_set_member (attendee, g_strdup (val));
		break;
	case E_MEETING_STORE_TYPE_COL:
		type = text_to_type (val);
		e_meeting_attendee_set_cutype (attendee, text_to_type (val));
		if (type == ICAL_CUTYPE_RESOURCE) {
			e_meeting_attendee_set_role (attendee, ICAL_ROLE_NONPARTICIPANT);
		}
		break;
	case E_MEETING_STORE_ROLE_COL:
		e_meeting_attendee_set_role (attendee, text_to_role (val));
		break;
	case E_MEETING_STORE_RSVP_COL:
		e_meeting_attendee_set_rsvp (attendee, text_to_boolean (val));
		break;
	case E_MEETING_STORE_DELTO_COL:
		e_meeting_attendee_set_delto (attendee, g_strdup (val));
		break;
	case E_MEETING_STORE_DELFROM_COL:
		e_meeting_attendee_set_delfrom (attendee, g_strdup (val));
		break;
	case E_MEETING_STORE_STATUS_COL:
		e_meeting_attendee_set_status (attendee, text_to_partstat (val));
		break;
	case E_MEETING_STORE_CN_COL:
		e_meeting_attendee_set_cn (attendee, g_strdup (val));
		break;
	case E_MEETING_STORE_LANGUAGE_COL:
		e_meeting_attendee_set_language (attendee, g_strdup (val));
		break;
	}
}

static void
refresh_queue_remove (EMeetingStore *store, EMeetingAttendee *attendee)
{
	EMeetingStorePrivate *priv;
	EMeetingStoreQueueData *qdata;

	priv = store->priv;

	/* Free the queue data */
	qdata = g_hash_table_lookup (priv->refresh_data, itip_strip_mailto (e_meeting_attendee_get_address (attendee)));
	if (qdata) {
		g_mutex_lock (priv->mutex);
		g_hash_table_remove (priv->refresh_data, itip_strip_mailto (e_meeting_attendee_get_address (attendee)));
		g_mutex_unlock (priv->mutex);
		g_ptr_array_free (qdata->call_backs, TRUE);
		g_ptr_array_free (qdata->data, TRUE);
		g_free (qdata);
	}

	/* Unref the attendee */
	g_ptr_array_remove (priv->refresh_queue, attendee);
	g_object_unref (attendee);
}

static void
ems_finalize (GObject *obj)
{
	EMeetingStore *store = E_MEETING_STORE (obj);
	EMeetingStorePrivate *priv;
	gint i;

	priv = store->priv;

	for (i = 0; i < priv->attendees->len; i++)
		g_object_unref (g_ptr_array_index (priv->attendees, i));
	g_ptr_array_free (priv->attendees, TRUE);

	if (priv->client != NULL)
		g_object_unref (priv->client);

	while (priv->refresh_queue->len > 0)
		refresh_queue_remove (store, g_ptr_array_index (priv->refresh_queue, 0));
	g_ptr_array_free (priv->refresh_queue, TRUE);
	g_hash_table_destroy (priv->refresh_data);

	if (priv->refresh_idle_id)
		g_source_remove (priv->refresh_idle_id);

	g_free (priv->fb_uri);

	g_mutex_free (priv->mutex);

	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (obj);
}

static void
ems_class_init (GObjectClass *klass)
{
	parent_class = g_type_class_peek_parent (klass);

	klass->finalize = ems_finalize;
}

static void
ems_init (EMeetingStore *store)
{
	EMeetingStorePrivate *priv;

	priv = g_new0 (EMeetingStorePrivate, 1);

	store->priv = priv;

	priv->attendees = g_ptr_array_new ();

	priv->zone = calendar_config_get_icaltimezone ();

	priv->fb_uri = calendar_config_get_free_busy_template ();

	priv->refresh_queue = g_ptr_array_new ();
	priv->refresh_data = g_hash_table_new (g_str_hash, g_str_equal);

	priv->mutex = g_mutex_new ();

	priv->num_queries = 0;
}

GType
e_meeting_store_get_type (void)
{
	static GType ems_type = 0;

	if (!ems_type) {
		static const GTypeInfo ems_info = {
				sizeof (EMeetingStoreClass),
				NULL,           /* base_init */
				NULL,           /* base_finalize */
				(GClassInitFunc) ems_class_init,
				NULL,           /* class_finalize */
				NULL,           /* class_data */
				sizeof (EMeetingStore),
				0,
				(GInstanceInitFunc) ems_init };

		static const GInterfaceInfo tree_model_info = {
				(GInterfaceInitFunc) ems_tree_model_init,
				NULL,
				NULL };

		ems_type = g_type_register_static (GTK_TYPE_LIST_STORE,
						   "EMeetingStore",
						   &ems_info, 0);

		g_type_add_interface_static (ems_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return ems_type;
}

GObject *
e_meeting_store_new (void)
{
	return g_object_new (E_TYPE_MEETING_STORE, NULL);
}

ECal *
e_meeting_store_get_e_cal (EMeetingStore *store)
{
	return store->priv->client;
}

void
e_meeting_store_set_e_cal (EMeetingStore *store, ECal *client)
{
	if (store->priv->client != NULL)
		g_object_unref (store->priv->client);

	if (client != NULL)
		g_object_ref (client);
	store->priv->client = client;
}

icaltimezone *
e_meeting_store_get_zone (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), NULL);

	return store->priv->zone;
}

void
e_meeting_store_set_zone (EMeetingStore *store, icaltimezone *zone)
{
	g_return_if_fail (E_IS_MEETING_STORE (store));

	store->priv->zone = zone;
}

gchar *
e_meeting_store_get_fb_uri (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), NULL);

	return g_strdup (store->priv->fb_uri);
}

void
e_meeting_store_set_fb_uri (EMeetingStore *store, const gchar *fb_uri)
{
	g_return_if_fail (E_IS_MEETING_STORE (store));

	g_free (store->priv->fb_uri);
	store->priv->fb_uri = g_strdup (fb_uri);
}

static void
attendee_changed_cb (EMeetingAttendee *attendee, gpointer data)
{
	EMeetingStore *store = E_MEETING_STORE (data);
	GtkTreePath *path;
	GtkTreeIter iter;
	gint row = -1, i;

	for (i = 0; i < store->priv->attendees->len; i++) {
		if (attendee == g_ptr_array_index (store->priv->attendees, i)) {
			row = i;
			break;
		}
	}

	if (row == -1)
		return;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, row);
	get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (store), path, &iter);
	gtk_tree_path_free (path);
}

void
e_meeting_store_add_attendee (EMeetingStore *store, EMeetingAttendee *attendee)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_MEETING_STORE (store));

	g_object_ref (attendee);
	g_ptr_array_add (store->priv->attendees, attendee);

	g_signal_connect (attendee, "changed", G_CALLBACK (attendee_changed_cb), store);

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, store->priv->attendees->len - 1);
	get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
	gtk_tree_path_free (path);
}

EMeetingAttendee *
e_meeting_store_add_attendee_with_defaults (EMeetingStore *store)
{
	EMeetingAttendee *attendee;
	gchar *str;

	attendee = E_MEETING_ATTENDEE (e_meeting_attendee_new ());

	e_meeting_attendee_set_address (attendee, g_strdup (""));
	e_meeting_attendee_set_member (attendee, g_strdup (""));

	str = g_strdup (_("Individual"));
	e_meeting_attendee_set_cutype (attendee, text_to_type (str));
	g_free (str);
	str = g_strdup (_("Required Participant"));
	e_meeting_attendee_set_role (attendee, text_to_role (str));
	g_free (str);
	str = g_strdup (_("Yes"));
	e_meeting_attendee_set_rsvp (attendee, text_to_boolean (str));
	g_free (str);

	e_meeting_attendee_set_delto (attendee, g_strdup (""));
	e_meeting_attendee_set_delfrom (attendee, g_strdup (""));

	str = g_strdup (_("Needs Action"));
	e_meeting_attendee_set_status (attendee, text_to_partstat (str));
	g_free (str);

	e_meeting_attendee_set_cn (attendee, g_strdup (""));
	e_meeting_attendee_set_language (attendee, g_strdup ("en"));

	e_meeting_store_add_attendee (store, attendee);

	return attendee;
}

void
e_meeting_store_remove_attendee (EMeetingStore *store, EMeetingAttendee *attendee)
{
	gint i, row = -1;
	GtkTreePath *path;

	for (i = 0; i < store->priv->attendees->len; i++) {
		if (attendee == g_ptr_array_index (store->priv->attendees, i)) {
			row = i;
			break;
		}
	}

	if (row != -1) {
		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, row);
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
		gtk_tree_path_free (path);

		g_ptr_array_remove_index (store->priv->attendees, row);
		g_object_unref (attendee);
	}
}

void
e_meeting_store_remove_all_attendees (EMeetingStore *store)
{
	gint i, j, k;

	for (i = 0, j = e_meeting_store_count_actual_attendees (store), k = 0; i < j; i++) {
	/* Always try to remove the attendee at index 0 since it is the only one that will
	 * continue to exist until all attendees are removed. */
		EMeetingAttendee *attendee = g_ptr_array_index (store->priv->attendees, k);
		GtkTreePath *path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, k);
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
		gtk_tree_path_free (path);

		g_ptr_array_remove_index (store->priv->attendees, k);
		g_object_unref (attendee);
	}
}

EMeetingAttendee *
e_meeting_store_find_attendee (EMeetingStore *store, const gchar *address, gint *row)
{
	EMeetingAttendee *attendee;
	gint i;

	if (address == NULL)
		return NULL;

	for (i = 0; i < store->priv->attendees->len; i++) {
		const gchar *attendee_address;

		attendee = g_ptr_array_index (store->priv->attendees, i);

		attendee_address = e_meeting_attendee_get_address (attendee);
		if (attendee_address && !g_ascii_strcasecmp (itip_strip_mailto (attendee_address), itip_strip_mailto (address))) {
			if (row != NULL)
				*row = i;

			return attendee;
		}
	}

	return NULL;
}

EMeetingAttendee *
e_meeting_store_find_attendee_at_row (EMeetingStore *store, gint row)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), NULL);
	g_return_val_if_fail (ROW_VALID (store, row), NULL);

	return g_ptr_array_index (store->priv->attendees, row);
}

GtkTreePath *
e_meeting_store_find_attendee_path (EMeetingStore *store, EMeetingAttendee *attendee)
{
	GtkTreePath *path;
	gint row = -1, i;

	for (i = 0; i < store->priv->attendees->len; i++) {
		if (attendee == g_ptr_array_index (store->priv->attendees, i)) {
			row = i;
			break;
		}
	}

	if (row == -1)
		return NULL;

	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, row);

	return path;
}

gint
e_meeting_store_count_actual_attendees (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), 0);

	return store->priv->attendees->len;
}

const GPtrArray *
e_meeting_store_get_attendees (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), NULL);

	return store->priv->attendees;
}

static icaltimezone *
find_zone (icalproperty *ip, icalcomponent *tz_top_level)
{
	icalparameter *param;
	icalcomponent *sub_comp;
	const gchar *tzid;
	icalcompiter iter;

	if (tz_top_level == NULL)
		return NULL;

	param = icalproperty_get_first_parameter (ip, ICAL_TZID_PARAMETER);
	if (param == NULL)
		return NULL;
	tzid = icalparameter_get_tzid (param);

	iter = icalcomponent_begin_component (tz_top_level, ICAL_VTIMEZONE_COMPONENT);
	while ((sub_comp = icalcompiter_deref (&iter)) != NULL) {
		icalcomponent *clone;
		icalproperty *prop;
		const gchar *tz_tzid;

		prop = icalcomponent_get_first_property (sub_comp, ICAL_TZID_PROPERTY);
		tz_tzid = icalproperty_get_tzid (prop);
		if (!strcmp (tzid, tz_tzid)) {
			icaltimezone *zone;

			zone = icaltimezone_new ();
			clone = icalcomponent_new_clone (sub_comp);
			icaltimezone_set_component (zone, clone);

			return zone;
		}

		icalcompiter_next (&iter);
	}

	return NULL;
}

static void
process_callbacks (EMeetingStoreQueueData *qdata)
{
	EMeetingStore *store;
	gint i;

	store = qdata->store;

	for (i = 0; i < qdata->call_backs->len; i++) {
		EMeetingStoreRefreshCallback call_back;
		gpointer *data = NULL;

		call_back = g_ptr_array_index (qdata->call_backs, i);
		data = g_ptr_array_index (qdata->data, i);

		g_idle_add ((GSourceFunc) call_back, data);
	}

	g_mutex_lock (store->priv->mutex);
	store->priv->num_threads--;
	g_mutex_unlock (store->priv->mutex);

	refresh_queue_remove (qdata->store, qdata->attendee);
	g_object_unref (store);
}

static void
process_free_busy_comp (EMeetingAttendee *attendee,
			icalcomponent *fb_comp,
			icaltimezone *zone,
			icalcomponent *tz_top_level)
{
	icalproperty *ip;

	ip = icalcomponent_get_first_property (fb_comp, ICAL_DTSTART_PROPERTY);
	if (ip != NULL) {
		struct icaltimetype dtstart;
		icaltimezone *ds_zone;

		dtstart = icalproperty_get_dtstart (ip);
		if (!dtstart.is_utc)
			ds_zone = find_zone (ip, tz_top_level);
		else
			ds_zone = icaltimezone_get_utc_timezone ();
		icaltimezone_convert_time (&dtstart, ds_zone, zone);
		e_meeting_attendee_set_start_busy_range (attendee,
							 dtstart.year,
							 dtstart.month,
							 dtstart.day,
							 dtstart.hour,
							 dtstart.minute);
	}

	ip = icalcomponent_get_first_property (fb_comp, ICAL_DTEND_PROPERTY);
	if (ip != NULL) {
		struct icaltimetype dtend;
		icaltimezone *de_zone;

		dtend = icalproperty_get_dtend (ip);
		if (!dtend.is_utc)
			de_zone = find_zone (ip, tz_top_level);
		else
			de_zone = icaltimezone_get_utc_timezone ();
		icaltimezone_convert_time (&dtend, de_zone, zone);
		e_meeting_attendee_set_end_busy_range (attendee,
						       dtend.year,
						       dtend.month,
						       dtend.day,
						       dtend.hour,
						       dtend.minute);
	}

	ip = icalcomponent_get_first_property (fb_comp, ICAL_FREEBUSY_PROPERTY);
	while (ip != NULL) {
		icalparameter *param;
		struct icalperiodtype fb;
		EMeetingFreeBusyType busy_type = E_MEETING_FREE_BUSY_LAST;
		icalparameter_fbtype fbtype = ICAL_FBTYPE_BUSY;

		fb = icalproperty_get_freebusy (ip);
		param = icalproperty_get_first_parameter (ip, ICAL_FBTYPE_PARAMETER);
		if (param != NULL)
			fbtype =  icalparameter_get_fbtype (param);

		switch (fbtype) {
		case ICAL_FBTYPE_BUSY:
			busy_type = E_MEETING_FREE_BUSY_BUSY;
			break;

		case ICAL_FBTYPE_BUSYUNAVAILABLE:
			busy_type = E_MEETING_FREE_BUSY_OUT_OF_OFFICE;
			break;

		case ICAL_FBTYPE_BUSYTENTATIVE:
			busy_type = E_MEETING_FREE_BUSY_TENTATIVE;
			break;

		case ICAL_FBTYPE_FREE:
			busy_type = E_MEETING_FREE_BUSY_FREE;
			break;

		default:
			break;
		}

		if (busy_type != E_MEETING_FREE_BUSY_LAST) {
			icaltimezone *utc_zone = icaltimezone_get_utc_timezone ();

			icaltimezone_convert_time (&fb.start, utc_zone, zone);
			icaltimezone_convert_time (&fb.end, utc_zone, zone);
			e_meeting_attendee_add_busy_period (attendee,
							    fb.start.year,
							    fb.start.month,
							    fb.start.day,
							    fb.start.hour,
							    fb.start.minute,
							    fb.end.year,
							    fb.end.month,
							    fb.end.day,
							    fb.end.hour,
							    fb.end.minute,
							    busy_type);
		}

		ip = icalcomponent_get_next_property (fb_comp, ICAL_FREEBUSY_PROPERTY);
	}
}

static void
process_free_busy (EMeetingStoreQueueData *qdata, gchar *text)
{
	EMeetingStore *store = qdata->store;
	EMeetingStorePrivate *priv;
	EMeetingAttendee *attendee = qdata->attendee;
	icalcomponent *main_comp;
	icalcomponent_kind kind = ICAL_NO_COMPONENT;

	priv = store->priv;

	main_comp = icalparser_parse_string (text);
	if (main_comp == NULL) {
		process_callbacks (qdata);
		return;
	}

	kind = icalcomponent_isa (main_comp);
	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcompiter iter;
		icalcomponent *tz_top_level, *sub_comp;

		tz_top_level = e_cal_util_new_top_level ();

		iter = icalcomponent_begin_component (main_comp, ICAL_VTIMEZONE_COMPONENT);
		while ((sub_comp = icalcompiter_deref (&iter)) != NULL) {
			icalcomponent *clone;

			clone = icalcomponent_new_clone (sub_comp);
			icalcomponent_add_component (tz_top_level, clone);

			icalcompiter_next (&iter);
		}

		iter = icalcomponent_begin_component (main_comp, ICAL_VFREEBUSY_COMPONENT);
		while ((sub_comp = icalcompiter_deref (&iter)) != NULL) {
			process_free_busy_comp (attendee, sub_comp, priv->zone, tz_top_level);

			icalcompiter_next (&iter);
		}
		icalcomponent_free (tz_top_level);
	} else if (kind == ICAL_VFREEBUSY_COMPONENT) {
		process_free_busy_comp (attendee, main_comp, priv->zone, NULL);
	}

	icalcomponent_free (main_comp);

	process_callbacks (qdata);
}

/*
 * Replace all instances of from_value in string with to_value
 * In the returned newly allocated string.
*/
static gchar *
replace_string (gchar *string, const gchar *from_value, gchar *to_value)
{
	gchar *replaced;
	gchar **split_uri;

	split_uri = g_strsplit (string, from_value, 0);
	replaced = g_strjoinv (to_value, split_uri);
	g_strfreev (split_uri);

	return replaced;
}

static void start_async_read (const gchar *uri, gpointer data);

typedef struct {
	ECal *client;
	time_t startt;
	time_t endt;
	GList *users;
	GList *fb_data;
	gchar *fb_uri;
	gchar *email;
	EMeetingAttendee *attendee;
	EMeetingStoreQueueData *qdata;
	EMeetingStore *store;
} FreeBusyAsyncData;

#define USER_SUB   "%u"
#define DOMAIN_SUB "%d"

static gboolean
freebusy_async (gpointer data)
{
	FreeBusyAsyncData *fbd = data;
	EMeetingAttendee *attendee = fbd->attendee;
	gchar *default_fb_uri = NULL;
	gchar *fburi = NULL;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
	EMeetingStorePrivate *priv = fbd->store->priv;

	if (fbd->client) {
		/* FIXME this a work around for getting all th free busy information for the users
		 we should be able to get free busy asynchronously */
		g_static_mutex_lock (&mutex);
		priv->num_queries++;
		e_cal_get_free_busy (fbd->client, fbd->users, fbd->startt, fbd->endt, &(fbd->fb_data), NULL);
		priv->num_queries--;
		g_static_mutex_unlock (&mutex);

		g_list_foreach (fbd->users, (GFunc)g_free, NULL);
		g_list_free (fbd->users);

		if (fbd->fb_data != NULL) {
			ECalComponent *comp = fbd->fb_data->data;
			gchar *comp_str;

			comp_str = e_cal_component_get_as_string (comp);
			process_free_busy (fbd->qdata, comp_str);
			g_free (comp_str);

			return TRUE;
		}
	}

	/* Look for fburl's of attendee with no free busy info on server */
	if (!e_meeting_attendee_is_set_address (attendee)) {
		process_callbacks (fbd->qdata);
		return TRUE;
	}

	/* Check for free busy info on the default server */
	default_fb_uri = g_strdup (fbd->fb_uri);
	fburi = g_strdup (e_meeting_attendee_get_fburi (attendee));

	if (fburi) {
		priv->num_queries++;
		start_async_read (fburi, fbd->qdata);
		g_free (fburi);
	} else if (default_fb_uri != NULL && !g_str_equal (default_fb_uri, "")) {
		gchar *tmp_fb_uri;
		gchar **split_email;

		split_email = g_strsplit (fbd->email, "@", 2);

		tmp_fb_uri = replace_string (default_fb_uri, USER_SUB, split_email[0]);
		g_free (default_fb_uri);
		default_fb_uri = replace_string (tmp_fb_uri, DOMAIN_SUB, split_email[1]);

		priv->num_queries++;
		start_async_read (default_fb_uri, fbd->qdata);
		g_free (tmp_fb_uri);
		g_strfreev (split_email);
		g_free (default_fb_uri);
	} else {
		process_callbacks (fbd->qdata);
	}

	return TRUE;
}

#undef USER_SUB
#undef DOMAIN_SUB

static gboolean
refresh_busy_periods (gpointer data)
{
	EMeetingStore *store = E_MEETING_STORE (data);
	EMeetingStorePrivate *priv;
	EMeetingAttendee *attendee = NULL;
	EMeetingStoreQueueData *qdata = NULL;
	gint i;
	GThread *thread;
	GError *error = NULL;
	FreeBusyAsyncData *fbd;

	priv = store->priv;

	/* Check to see if there are any remaining attendees in the queue */
	for (i = 0; i < priv->refresh_queue->len; i++) {
		attendee = g_ptr_array_index (priv->refresh_queue, i);
		g_return_val_if_fail (attendee != NULL, FALSE);

		qdata = g_hash_table_lookup (priv->refresh_data, itip_strip_mailto (e_meeting_attendee_get_address (attendee)));
		if (!qdata)
			continue;

		if (!qdata->refreshing)
			break;
	}

	/* The everything in the queue is being refreshed */
	if (i >= priv->refresh_queue->len) {
		priv->refresh_idle_id = 0;
		return FALSE;
	}

	/* Indicate we are trying to refresh it */
	qdata->refreshing = TRUE;

	/* We take a ref in case we get destroyed in the gui during a callback */
	g_object_ref (qdata->store);

	fbd = g_new0 (FreeBusyAsyncData, 1);
	fbd->client = priv->client;
	fbd->attendee = attendee;
	fbd->users = NULL;
	fbd->fb_data = NULL;
	fbd->qdata = qdata;
	fbd->fb_uri = priv->fb_uri;
	fbd->store = store;
	fbd->email = g_strdup (itip_strip_mailto (e_meeting_attendee_get_address (attendee)));

	/* Check the server for free busy data */
	if (priv->client) {
		struct icaltimetype itt;

		itt = icaltime_null_time ();
		itt.year = g_date_get_year (&qdata->start.date);
		itt.month = g_date_get_month (&qdata->start.date);
		itt.day = g_date_get_day (&qdata->start.date);
		itt.hour = qdata->start.hour;
		itt.minute = qdata->start.minute;
		fbd->startt = icaltime_as_timet_with_zone (itt, priv->zone);

		itt = icaltime_null_time ();
		itt.year = g_date_get_year (&qdata->end.date);
		itt.month = g_date_get_month (&qdata->end.date);
		itt.day = g_date_get_day (&qdata->end.date);
		itt.hour = qdata->end.hour;
		itt.minute = qdata->end.minute;
		fbd->endt = icaltime_as_timet_with_zone (itt, priv->zone);
		fbd->qdata = qdata;

		fbd->users = g_list_append (fbd->users, g_strdup (fbd->email));

	}

	g_mutex_lock (store->priv->mutex);
	store->priv->num_threads++;
	g_mutex_unlock (store->priv->mutex);

	thread = g_thread_create ((GThreadFunc) freebusy_async, fbd, FALSE, &error);
	if (!thread) {
		/* do clean up stuff here */
		g_list_foreach (fbd->users, (GFunc)g_free, NULL);
		g_list_free (fbd->users);
		g_free (fbd->email);
		priv->refresh_idle_id = 0;

		g_mutex_lock (store->priv->mutex);
		store->priv->num_threads--;
		g_mutex_unlock (store->priv->mutex);

		return FALSE;
	}

	return TRUE;
}

static void
refresh_queue_add (EMeetingStore *store, gint row,
		   EMeetingTime *start,
		   EMeetingTime *end,
		   EMeetingStoreRefreshCallback call_back,
		   gpointer data)
{
	EMeetingStorePrivate *priv;
	EMeetingAttendee *attendee;
	EMeetingStoreQueueData *qdata;
	gint i;

	priv = store->priv;

	attendee = g_ptr_array_index (priv->attendees, row);
	if ((attendee == NULL) || !strcmp (itip_strip_mailto (e_meeting_attendee_get_address (attendee)), ""))
		return;

	/* check the queue if the attendee is already in there*/
	for (i = 0; i < priv->refresh_queue->len; i++) {
		if (attendee == g_ptr_array_index (priv->refresh_queue, i))
			return;

		if (!strcmp (e_meeting_attendee_get_address (attendee), e_meeting_attendee_get_address (g_ptr_array_index (priv->refresh_queue, i))))
			return;
	}

	g_mutex_lock (priv->mutex);
	qdata = g_hash_table_lookup (priv->refresh_data, itip_strip_mailto (e_meeting_attendee_get_address (attendee)));

	if (qdata == NULL) {
		qdata = g_new0 (EMeetingStoreQueueData, 1);

		qdata->store = store;
		qdata->attendee = attendee;
		e_meeting_attendee_clear_busy_periods (attendee);
		e_meeting_attendee_set_has_calendar_info (attendee, FALSE);

		qdata->start = *start;
		qdata->end = *end;
		qdata->string = g_string_new (NULL);
		qdata->call_backs = g_ptr_array_new ();
		qdata->data = g_ptr_array_new ();
		g_ptr_array_add (qdata->call_backs, call_back);
		g_ptr_array_add (qdata->data, data);

		g_hash_table_insert (priv->refresh_data, (gpointer)itip_strip_mailto (e_meeting_attendee_get_address (attendee)), qdata);
	} else {
		if (e_meeting_time_compare_times (start, &qdata->start) == -1)
			qdata->start = *start;
		if (e_meeting_time_compare_times (end, &qdata->end) == -1)
			qdata->end = *end;
		g_ptr_array_add (qdata->call_backs, call_back);
		g_ptr_array_add (qdata->data, data);
	}
	g_mutex_unlock (priv->mutex);

	g_object_ref (attendee);
	g_ptr_array_add (priv->refresh_queue, attendee);

	if (priv->refresh_idle_id == 0)
		priv->refresh_idle_id = g_idle_add (refresh_busy_periods, store);
}

static void
async_read (GObject *source_object, GAsyncResult *res, gpointer data)
{
	EMeetingStoreQueueData *qdata = data;
	GError *error = NULL;
	GInputStream *istream;
	gssize read;

	g_return_if_fail (source_object != NULL);
	g_return_if_fail (G_IS_INPUT_STREAM (source_object));

	istream = G_INPUT_STREAM (source_object);

	read = g_input_stream_read_finish (istream, res, &error);

	if (error || read < 0) {
		g_warning ("Read finish failed: %s", error ? error->message : "Unknown error");
		if (error)
			g_error_free (error);

		g_input_stream_close (istream, NULL, NULL);
		g_object_unref (istream);
		process_free_busy (qdata, qdata->string->str);
		return;
	}

	if (read == 0) {
		g_input_stream_close (istream, NULL, NULL);
		g_object_unref (istream);
		process_free_busy (qdata, qdata->string->str);
	} else {
		qdata->buffer[read] = '\0';
		qdata->string = g_string_append (qdata->string, qdata->buffer);

		g_input_stream_read_async (istream, qdata->buffer, BUF_SIZE - 1, G_PRIORITY_DEFAULT, NULL, async_read, qdata);
	}
}

static void
start_async_read (const gchar *uri, gpointer data)
{
	EMeetingStoreQueueData *qdata = data;
	GError *error = NULL;
	GFile *file;
	GInputStream *istream;

	g_return_if_fail (uri != NULL);
	g_return_if_fail (data != NULL);

	qdata->store->priv->num_queries--;
	file = g_file_new_for_uri (uri);

	g_return_if_fail (file != NULL);

	istream = G_INPUT_STREAM (g_file_read (file, NULL, &error));

	if (error) {
		g_warning ("Unable to access free/busy url: %s", error->message);
		g_error_free (error);
		process_callbacks (qdata);
		return;
	}

	if (!istream)
		process_callbacks (qdata);
	else
		g_input_stream_read_async (istream, qdata->buffer, BUF_SIZE - 1, G_PRIORITY_DEFAULT, NULL, async_read, qdata);
}

void
e_meeting_store_refresh_all_busy_periods (EMeetingStore *store,
					  EMeetingTime *start,
					  EMeetingTime *end,
					  EMeetingStoreRefreshCallback call_back,
					  gpointer data)
{
	gint i;

	g_return_if_fail (E_IS_MEETING_STORE (store));

	for (i = 0; i < store->priv->attendees->len; i++)
		refresh_queue_add (store, i, start, end, call_back, data);
}

void
e_meeting_store_refresh_busy_periods (EMeetingStore *store,
				      gint row,
				      EMeetingTime *start,
				      EMeetingTime *end,
				      EMeetingStoreRefreshCallback call_back,
				      gpointer data)
{
	g_return_if_fail (E_IS_MEETING_STORE (store));

	refresh_queue_add (store, row, start, end, call_back, data);
}

guint
e_meeting_store_get_num_queries (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), 0);

	return store->priv->num_queries;
}
