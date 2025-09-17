/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		JP Rosevear  <jpr@ximian.com>
 *	    Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <libsoup/soup.h>

#include <libecal/libecal.h>
#include <libebackend/libebackend.h>

#include <shell/e-shell.h>
#include <e-util/e-util-enumtypes.h>

#include "itip-utils.h"
#include "e-meeting-utils.h"
#include "e-meeting-attendee.h"
#include "e-meeting-store.h"

#define ROW_VALID(store, row) \
	(row >= 0 && row < store->priv->attendees->len)

struct _EMeetingStorePrivate {
	GPtrArray *attendees;
	gint stamp;

	ECalClient *client;
	ICalTimezone *zone;

	gint default_reminder_interval;
	EDurationType default_reminder_units;

	gchar *fb_uri;

	GPtrArray *refresh_queue;
	GHashTable *refresh_data;
	GMutex mutex;
	guint refresh_idle_id;

	guint num_threads;
	guint num_queries;

	gboolean show_address;
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

enum {
	PROP_0,
	PROP_CLIENT,
	PROP_DEFAULT_REMINDER_INTERVAL,
	PROP_DEFAULT_REMINDER_UNITS,
	PROP_FREE_BUSY_TEMPLATE,
	PROP_SHOW_ADDRESS,
	PROP_TIMEZONE
};

/* Forward Declarations */
static void ems_tree_model_init (GtkTreeModelIface *iface);

G_DEFINE_TYPE_WITH_CODE (EMeetingStore, e_meeting_store, GTK_TYPE_LIST_STORE,
	G_ADD_PRIVATE (EMeetingStore)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, ems_tree_model_init))

static ICalParameterCutype
text_to_type (const gchar *type)
{
	if (!e_util_utf8_strcasecmp (type, _("Individual")))
		return I_CAL_CUTYPE_INDIVIDUAL;
	else if (!e_util_utf8_strcasecmp (type, _("Group")))
		return I_CAL_CUTYPE_GROUP;
	else if (!e_util_utf8_strcasecmp (type, _("Resource")))
		return I_CAL_CUTYPE_RESOURCE;
	else if (!e_util_utf8_strcasecmp (type, _("Room")))
		return I_CAL_CUTYPE_ROOM;
	else
		return I_CAL_CUTYPE_NONE;
}

static gchar *
type_to_text (ICalParameterCutype type)
{
	switch (type) {
	case I_CAL_CUTYPE_INDIVIDUAL:
		return _("Individual");
	case I_CAL_CUTYPE_GROUP:
		return _("Group");
	case I_CAL_CUTYPE_RESOURCE:
		return _("Resource");
	case I_CAL_CUTYPE_ROOM:
		return _("Room");
	default:
		return _("Unknown");
	}

	return NULL;

}

static ICalParameterRole
text_to_role (const gchar *role)
{
	if (!e_util_utf8_strcasecmp (role, _("Chair")))
		return I_CAL_ROLE_CHAIR;
	else if (!e_util_utf8_strcasecmp (role, _("Required Participant")))
		return I_CAL_ROLE_REQPARTICIPANT;
	else if (!e_util_utf8_strcasecmp (role, _("Optional Participant")))
		return I_CAL_ROLE_OPTPARTICIPANT;
	else if (!e_util_utf8_strcasecmp (role, _("Non-Participant")))
		return I_CAL_ROLE_NONPARTICIPANT;
	else
		return I_CAL_ROLE_NONE;
}

static gchar *
role_to_text (ICalParameterRole role)
{
	switch (role) {
	case I_CAL_ROLE_CHAIR:
		return _("Chair");
	case I_CAL_ROLE_REQPARTICIPANT:
		return _("Required Participant");
	case I_CAL_ROLE_OPTPARTICIPANT:
		return _("Optional Participant");
	case I_CAL_ROLE_NONPARTICIPANT:
		return _("Non-Participant");
	default:
		return _("Unknown");
	}
}

static ICalParameterPartstat
text_to_partstat (const gchar *partstat)
{
	if (!e_util_utf8_strcasecmp (partstat, _("Needs Action")))
		return I_CAL_PARTSTAT_NEEDSACTION;
	else if (!e_util_utf8_strcasecmp (partstat, _("Accepted")))
		return I_CAL_PARTSTAT_ACCEPTED;
	else if (!e_util_utf8_strcasecmp (partstat, _("Declined")))
		return I_CAL_PARTSTAT_DECLINED;
	else if (!e_util_utf8_strcasecmp (partstat, _("Tentative")))
		return I_CAL_PARTSTAT_TENTATIVE;
	else if (!e_util_utf8_strcasecmp (partstat, _("Delegated")))
		return I_CAL_PARTSTAT_DELEGATED;
	else if (!e_util_utf8_strcasecmp (partstat, _("Completed")))
		return I_CAL_PARTSTAT_COMPLETED;
	else if (!e_util_utf8_strcasecmp (partstat, _("In Process")))
		return I_CAL_PARTSTAT_INPROCESS;
	else
		return I_CAL_PARTSTAT_NONE;
}

static gchar *
partstat_to_text (ICalParameterPartstat partstat)
{
	switch (partstat) {
	case I_CAL_PARTSTAT_NEEDSACTION:
		return _("Needs Action");
	case I_CAL_PARTSTAT_ACCEPTED:
		return _("Accepted");
	case I_CAL_PARTSTAT_DECLINED:
		return _("Declined");
	case I_CAL_PARTSTAT_TENTATIVE:
		return _("Tentative");
	case I_CAL_PARTSTAT_DELEGATED:
		return _("Delegated");
	case I_CAL_PARTSTAT_COMPLETED:
		return _("Completed");
	case I_CAL_PARTSTAT_INPROCESS:
		return _("In Process");
	case I_CAL_PARTSTAT_NONE:
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
get_column_type (GtkTreeModel *model,
                 gint col)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (model), G_TYPE_INVALID);

	switch (col) {
	case E_MEETING_STORE_ADDRESS_COL:
	case E_MEETING_STORE_MEMBER_COL:
	case E_MEETING_STORE_TYPE_COL:
	case E_MEETING_STORE_ROLE_COL:
	case E_MEETING_STORE_DELTO_COL:
	case E_MEETING_STORE_DELFROM_COL:
	case E_MEETING_STORE_STATUS_COL:
	case E_MEETING_STORE_CN_COL:
	case E_MEETING_STORE_LANGUAGE_COL:
	case E_MEETING_STORE_ATTENDEE_COL:
		return G_TYPE_STRING;
	case E_MEETING_STORE_RSVP_COL:
		return G_TYPE_BOOLEAN;
	case E_MEETING_STORE_ATTENDEE_UNDERLINE_COL:
		return PANGO_TYPE_UNDERLINE;
	default:
		return G_TYPE_INVALID;
	}
}

static gboolean
get_iter (GtkTreeModel *model,
          GtkTreeIter *iter,
          GtkTreePath *path)
{
	gint row;

	g_return_val_if_fail (E_IS_MEETING_STORE (model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	row = gtk_tree_path_get_indices (path)[0];

	if (!ROW_VALID (E_MEETING_STORE (model), row))
	       return FALSE;

	iter->stamp = E_MEETING_STORE (model)->priv->stamp;
	iter->user_data = GINT_TO_POINTER (row);

	return TRUE;
}

static GtkTreePath *
get_path (GtkTreeModel *model,
          GtkTreeIter *iter)
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
get_value (GtkTreeModel *model,
           GtkTreeIter *iter,
           gint col,
           GValue *value)
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
		g_value_set_string (
			value, e_cal_util_strip_mailto (
			e_meeting_attendee_get_address (attendee)));
		break;
	case E_MEETING_STORE_MEMBER_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (
			value, e_meeting_attendee_get_member (attendee));
		break;
	case E_MEETING_STORE_TYPE_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (
			value, type_to_text (
			e_meeting_attendee_get_cutype (attendee)));
		break;
	case E_MEETING_STORE_ROLE_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (
			value, role_to_text (
			e_meeting_attendee_get_role (attendee)));
		break;
	case E_MEETING_STORE_RSVP_COL:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (
			value, e_meeting_attendee_get_rsvp (attendee));
		break;
	case E_MEETING_STORE_DELTO_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (
			value, e_cal_util_strip_mailto (
			e_meeting_attendee_get_delto (attendee)));
		break;
	case E_MEETING_STORE_DELFROM_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (
			value, e_cal_util_strip_mailto (
			e_meeting_attendee_get_delfrom (attendee)));
		break;
	case E_MEETING_STORE_STATUS_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (
			value, partstat_to_text (
			e_meeting_attendee_get_partstat (attendee)));
		break;
	case E_MEETING_STORE_CN_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (
			value, e_meeting_attendee_get_cn (attendee));
		break;
	case E_MEETING_STORE_LANGUAGE_COL:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (
			value, e_meeting_attendee_get_language (attendee));
		break;
	case E_MEETING_STORE_ATTENDEE_COL:
		g_value_init (value, G_TYPE_STRING);
		cn = e_meeting_attendee_get_cn (attendee);
		if (cn && *cn) {
			if (e_meeting_store_get_show_address (store) ||
			    e_meeting_attendee_get_show_address (attendee)) {
				const gchar *email = e_cal_util_strip_mailto (e_meeting_attendee_get_address (attendee));

				if (email && *email) {
					g_value_take_string (value, camel_internet_address_format_address (cn, email));
				} else {
					g_value_set_string (value, cn);
				}
			} else {
				g_value_set_string (value, cn);
			}
		} else {
			g_value_set_string (
				value, e_cal_util_strip_mailto (
				e_meeting_attendee_get_address (attendee)));
		}
		break;
	case E_MEETING_STORE_ATTENDEE_UNDERLINE_COL:
		cn = e_meeting_attendee_get_cn (attendee);
		g_value_init (value, PANGO_TYPE_UNDERLINE);
		g_value_set_enum (
			value, (!cn || !*cn) ?
			PANGO_UNDERLINE_NONE : PANGO_UNDERLINE_SINGLE);
	}
}

static gboolean
iter_next (GtkTreeModel *model,
           GtkTreeIter *iter)
{
	gint row;

	g_return_val_if_fail (E_IS_MEETING_STORE (model), FALSE);
	g_return_val_if_fail (iter->stamp == E_MEETING_STORE (model)->priv->stamp, FALSE);

	row = GPOINTER_TO_INT (iter->user_data) + 1;
	iter->user_data = GINT_TO_POINTER (row);

	return ROW_VALID (E_MEETING_STORE (model), row);
}

static gboolean
iter_children (GtkTreeModel *model,
               GtkTreeIter *iter,
               GtkTreeIter *parent)
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
iter_has_child (GtkTreeModel *model,
                GtkTreeIter *iter)
{
	return FALSE;
}

static gint
iter_n_children (GtkTreeModel *model,
                 GtkTreeIter *iter)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (model), -1);

	if (!iter)
		return E_MEETING_STORE (model)->priv->attendees->len;

	g_return_val_if_fail (iter->stamp == E_MEETING_STORE (model)->priv->stamp, -1);

	return 0;
}

static gboolean
iter_nth_child (GtkTreeModel *model,
                GtkTreeIter *iter,
                GtkTreeIter *parent,
                gint n)
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
iter_parent (GtkTreeModel *model,
             GtkTreeIter *iter,
             GtkTreeIter *child)
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
e_meeting_store_set_value (EMeetingStore *store,
                           gint row,
                           gint col,
                           const gchar *val)
{
	ICalParameterCutype cutype;
	EMeetingAttendee *attendee = g_ptr_array_index (store->priv->attendees, row);

	switch (col) {
	case E_MEETING_STORE_ADDRESS_COL:
		if (val != NULL && *((gchar *) val)) {
			gchar *mailto;

			mailto = g_strdup_printf ("mailto:%s", (const gchar *) val);
			e_meeting_attendee_set_address (attendee, mailto);
			g_free (mailto);
		}
		break;
	case E_MEETING_STORE_MEMBER_COL:
		e_meeting_attendee_set_member (attendee, val);
		break;
	case E_MEETING_STORE_TYPE_COL:
		cutype = text_to_type (val);
		e_meeting_attendee_set_cutype (attendee, cutype);
		if (cutype == I_CAL_CUTYPE_RESOURCE) {
			e_meeting_attendee_set_role (attendee, I_CAL_ROLE_NONPARTICIPANT);
		}
		break;
	case E_MEETING_STORE_ROLE_COL:
		e_meeting_attendee_set_role (attendee, text_to_role (val));
		break;
	case E_MEETING_STORE_RSVP_COL:
		e_meeting_attendee_set_rsvp (attendee, val != NULL);
		break;
	case E_MEETING_STORE_DELTO_COL:
		e_meeting_attendee_set_delto (attendee, val);
		break;
	case E_MEETING_STORE_DELFROM_COL:
		e_meeting_attendee_set_delfrom (attendee, val);
		break;
	case E_MEETING_STORE_STATUS_COL:
		e_meeting_attendee_set_partstat (attendee, text_to_partstat (val));
		break;
	case E_MEETING_STORE_CN_COL:
		e_meeting_attendee_set_cn (attendee, val);
		break;
	case E_MEETING_STORE_LANGUAGE_COL:
		e_meeting_attendee_set_language (attendee, val);
		break;
	}
}

struct FindAttendeeData
{
	EMeetingAttendee *find;
	EMeetingStoreQueueData *qdata;
};

static void
find_attendee_cb (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
	EMeetingStoreQueueData *qdata = value;
	struct FindAttendeeData *fad = user_data;

	g_return_if_fail (qdata != NULL);
	g_return_if_fail (fad != NULL);

	if (qdata->attendee == fad->find)
		fad->qdata = qdata;
}

static void
refresh_queue_remove (EMeetingStore *store,
                      EMeetingAttendee *attendee)
{
	EMeetingStorePrivate *priv;
	EMeetingStoreQueueData *qdata;

	priv = store->priv;

	/* Free the queue data */
	qdata = g_hash_table_lookup (
		priv->refresh_data, e_cal_util_strip_mailto (
		e_meeting_attendee_get_address (attendee)));
	if (!qdata) {
		struct FindAttendeeData fad = { 0 };

		fad.find = attendee;
		fad.qdata = NULL;

		g_hash_table_foreach (priv->refresh_data, find_attendee_cb, &fad);

		qdata = fad.qdata;
	}

	if (qdata) {
		g_mutex_lock (&priv->mutex);
		g_hash_table_remove (
			priv->refresh_data, e_cal_util_strip_mailto (
			e_meeting_attendee_get_address (attendee)));
		g_mutex_unlock (&priv->mutex);
		g_ptr_array_free (qdata->call_backs, TRUE);
		g_ptr_array_free (qdata->data, TRUE);
		g_string_free (qdata->string, TRUE);
		g_free (qdata);
	}

	/* Unref the attendee */
	g_ptr_array_remove (priv->refresh_queue, attendee);
	g_object_unref (attendee);
}

static void
meeting_store_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT:
			e_meeting_store_set_client (
				E_MEETING_STORE (object),
				g_value_get_object (value));
			return;

		case PROP_DEFAULT_REMINDER_INTERVAL:
			e_meeting_store_set_default_reminder_interval (
				E_MEETING_STORE (object),
				g_value_get_int (value));
			return;

		case PROP_DEFAULT_REMINDER_UNITS:
			e_meeting_store_set_default_reminder_units (
				E_MEETING_STORE (object),
				g_value_get_enum (value));
			return;

		case PROP_FREE_BUSY_TEMPLATE:
			e_meeting_store_set_free_busy_template (
				E_MEETING_STORE (object),
				g_value_get_string (value));
			return;

		case PROP_SHOW_ADDRESS:
			e_meeting_store_set_show_address (
				E_MEETING_STORE (object),
				g_value_get_boolean (value));
			return;

		case PROP_TIMEZONE:
			e_meeting_store_set_timezone (
				E_MEETING_STORE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
meeting_store_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT:
			g_value_set_object (
				value,
				e_meeting_store_get_client (
				E_MEETING_STORE (object)));
			return;

		case PROP_DEFAULT_REMINDER_INTERVAL:
			g_value_set_int (
				value,
				e_meeting_store_get_default_reminder_interval (
				E_MEETING_STORE (object)));
			return;

		case PROP_DEFAULT_REMINDER_UNITS:
			g_value_set_enum (
				value,
				e_meeting_store_get_default_reminder_units (
				E_MEETING_STORE (object)));
			return;

		case PROP_FREE_BUSY_TEMPLATE:
			g_value_set_string (
				value,
				e_meeting_store_get_free_busy_template (
				E_MEETING_STORE (object)));
			return;

		case PROP_SHOW_ADDRESS:
			g_value_set_boolean (
				value,
				e_meeting_store_get_show_address (
				E_MEETING_STORE (object)));
			return;

		case PROP_TIMEZONE:
			g_value_set_object (
				value,
				e_meeting_store_get_timezone (
				E_MEETING_STORE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
meeting_store_constructed (GObject *object)
{
	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_meeting_store_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
meeting_store_finalize (GObject *object)
{
	EMeetingStore *self = E_MEETING_STORE (object);
	gint i;

	for (i = 0; i < self->priv->attendees->len; i++)
		g_object_unref (g_ptr_array_index (self->priv->attendees, i));
	g_ptr_array_free (self->priv->attendees, TRUE);

	g_clear_object (&self->priv->client);

	while (self->priv->refresh_queue->len > 0)
		refresh_queue_remove (
			self,
			g_ptr_array_index (self->priv->refresh_queue, 0));
	g_ptr_array_free (self->priv->refresh_queue, TRUE);
	g_hash_table_destroy (self->priv->refresh_data);

	if (self->priv->refresh_idle_id)
		g_source_remove (self->priv->refresh_idle_id);

	g_free (self->priv->fb_uri);

	g_clear_object (&self->priv->zone);

	g_mutex_clear (&self->priv->mutex);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_meeting_store_parent_class)->finalize (object);
}

static void
e_meeting_store_class_init (EMeetingStoreClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = meeting_store_set_property;
	object_class->get_property = meeting_store_get_property;
	object_class->constructed = meeting_store_constructed;
	object_class->finalize = meeting_store_finalize;

	g_object_class_install_property (
		object_class,
		PROP_CLIENT,
		g_param_spec_object (
			"client",
			"ECalClient",
			NULL,
			E_TYPE_CAL_CLIENT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_REMINDER_INTERVAL,
		g_param_spec_int (
			"default-reminder-interval",
			"Default Reminder Interval",
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_REMINDER_UNITS,
		g_param_spec_enum (
			"default-reminder-units",
			"Default Reminder Units",
			NULL,
			E_TYPE_DURATION_TYPE,
			E_DURATION_MINUTES,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FREE_BUSY_TEMPLATE,
		g_param_spec_string (
			"free-busy-template",
			"Free/Busy Template",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_ADDRESS,
		g_param_spec_boolean (
			"show-address",
			"Show email addresses",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_object (
			"timezone",
			"Timezone",
			NULL,
			I_CAL_TYPE_TIMEZONE,
			G_PARAM_READWRITE));
}

static void
e_meeting_store_init (EMeetingStore *store)
{
	store->priv = e_meeting_store_get_instance_private (store);

	store->priv->attendees = g_ptr_array_new ();
	store->priv->refresh_queue = g_ptr_array_new ();
	store->priv->refresh_data = g_hash_table_new_full (
		g_str_hash, g_str_equal, g_free, NULL);

	g_mutex_init (&store->priv->mutex);

	store->priv->num_queries = 0;
}

GObject *
e_meeting_store_new (void)
{
	return g_object_new (E_TYPE_MEETING_STORE, NULL);
}

ECalClient *
e_meeting_store_get_client (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), NULL);

	return store->priv->client;
}

void
e_meeting_store_set_client (EMeetingStore *store,
                            ECalClient *client)
{
	g_return_if_fail (E_IS_MEETING_STORE (store));

	if (store->priv->client == client)
		return;

	if (client != NULL) {
		g_return_if_fail (E_IS_CAL_CLIENT (client));
		g_object_ref (client);
	}

	if (store->priv->client != NULL)
		g_object_unref (store->priv->client);

	store->priv->client = client;

	g_object_notify (G_OBJECT (store), "client");
}

gint
e_meeting_store_get_default_reminder_interval (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), 0);

	return store->priv->default_reminder_interval;
}

void
e_meeting_store_set_default_reminder_interval (EMeetingStore *store,
                                               gint default_reminder_interval)
{
	g_return_if_fail (E_IS_MEETING_STORE (store));

	if (store->priv->default_reminder_interval == default_reminder_interval)
		return;

	store->priv->default_reminder_interval = default_reminder_interval;

	g_object_notify (G_OBJECT (store), "default-reminder-interval");
}

EDurationType
e_meeting_store_get_default_reminder_units (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), 0);

	return store->priv->default_reminder_units;
}

void
e_meeting_store_set_default_reminder_units (EMeetingStore *store,
                                            EDurationType default_reminder_units)
{
	g_return_if_fail (E_IS_MEETING_STORE (store));

	if (store->priv->default_reminder_units == default_reminder_units)
		return;

	store->priv->default_reminder_units = default_reminder_units;

	g_object_notify (G_OBJECT (store), "default-reminder-units");
}

const gchar *
e_meeting_store_get_free_busy_template (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), NULL);

	return store->priv->fb_uri;
}

void
e_meeting_store_set_free_busy_template (EMeetingStore *store,
                                        const gchar *free_busy_template)
{
	g_return_if_fail (E_IS_MEETING_STORE (store));

	if (g_strcmp0 (store->priv->fb_uri, free_busy_template) == 0)
		return;

	g_free (store->priv->fb_uri);
	store->priv->fb_uri = g_strdup (free_busy_template);

	g_object_notify (G_OBJECT (store), "free-busy-template");
}

ICalTimezone *
e_meeting_store_get_timezone (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), NULL);

	return store->priv->zone;
}

void
e_meeting_store_set_timezone (EMeetingStore *store,
			      const ICalTimezone *timezone)
{
	g_return_if_fail (E_IS_MEETING_STORE (store));

	if (store->priv->zone == timezone)
		return;

	g_clear_object (&store->priv->zone);
	store->priv->zone = timezone ? e_cal_util_copy_timezone (timezone) : NULL;

	g_object_notify (G_OBJECT (store), "timezone");
}

gboolean
e_meeting_store_get_show_address (EMeetingStore *store)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), FALSE);

	return store->priv->show_address;
}

void
e_meeting_store_set_show_address (EMeetingStore *store,
				  gboolean show_address)
{
	g_return_if_fail (E_IS_MEETING_STORE (store));

	if ((store->priv->show_address ? 1 : 0) == (show_address ? 1 : 0))
		return;

	store->priv->show_address = show_address;

	g_object_notify (G_OBJECT (store), "show-address");
}

static void
attendee_changed_cb (EMeetingAttendee *attendee,
                     gpointer data)
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
e_meeting_store_add_attendee (EMeetingStore *store,
                              EMeetingAttendee *attendee)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_MEETING_STORE (store));

	g_object_ref (attendee);
	g_ptr_array_add (store->priv->attendees, attendee);

	g_signal_connect (
		attendee, "changed",
		G_CALLBACK (attendee_changed_cb), store);

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

	attendee = E_MEETING_ATTENDEE (e_meeting_attendee_new ());

	e_meeting_attendee_set_address (attendee, "");
	e_meeting_attendee_set_member (attendee, "");

	e_meeting_attendee_set_cutype (attendee, text_to_type (_("Individual")));
	e_meeting_attendee_set_role (attendee, text_to_role (_("Required Participant")));
	e_meeting_attendee_set_rsvp (attendee, TRUE);

	e_meeting_attendee_set_delto (attendee, "");
	e_meeting_attendee_set_delfrom (attendee, "");

	e_meeting_attendee_set_partstat (attendee, text_to_partstat (_("Needs Action")));

	e_meeting_attendee_set_cn (attendee, "");
	e_meeting_attendee_set_language (attendee, "");

	e_meeting_store_add_attendee (store, attendee);

	return attendee;
}

void
e_meeting_store_remove_attendee (EMeetingStore *store,
                                 EMeetingAttendee *attendee)
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
		g_ptr_array_remove_index (store->priv->attendees, row);

		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, row);
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
		gtk_tree_path_free (path);

		g_object_unref (attendee);
	}
}

void
e_meeting_store_remove_all_attendees (EMeetingStore *store)
{
	gint i, j, k;

	for (i = 0, j = e_meeting_store_count_actual_attendees (store), k = 0;
	     i < j; i++) {
		/* Always try to remove the attendee at index 0 since
		 * it is the only one that will continue to exist until
		 * all attendees are removed. */
		EMeetingAttendee *attendee;
		GtkTreePath *path;

		attendee = g_ptr_array_index (store->priv->attendees, k);
		g_ptr_array_remove_index (store->priv->attendees, k);

		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, k);
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
		gtk_tree_path_free (path);

		g_object_unref (attendee);
	}
}

/**
 * e_meeting_store_find_self:
 * @store: an #EMeetingStore
 * @row: return location for the matching row number, or %NULL
 *
 * Looks for the user in @store by comparing attendee email addresses to
 * registered mail identities.  If a matching email address is found and
 * @row is not %NULL, @row will be set to the #EMeetingStore row number
 * with the matching email address.
 *
 * Returns: an #EMeetingAttendee, or %NULL
 **/
EMeetingAttendee *
e_meeting_store_find_self (EMeetingStore *store,
                           gint *row)
{
	EMeetingAttendee *attendee = NULL;
	ESourceRegistry *registry;
	EShell *shell;
	GList *list, *link;
	const gchar *extension_name;

	g_return_val_if_fail (E_IS_MEETING_STORE (store), NULL);

	/* FIXME Refactor this so we don't need e_shell_get_default(). */
	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;

	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceMailIdentity *extension;
		GHashTable *aliases;
		const gchar *address;

		extension = e_source_get_extension (source, extension_name);
		address = e_source_mail_identity_get_address (extension);

		if (address != NULL)
			attendee = e_meeting_store_find_attendee (
				store, address, row);

		if (attendee != NULL)
			break;

		aliases = e_source_mail_identity_get_aliases_as_hash_table (extension);
		if (aliases) {
			GHashTableIter iter;
			gpointer key = NULL;

			g_hash_table_iter_init (&iter, aliases);
			while (!attendee && g_hash_table_iter_next (&iter, &key, NULL)) {
				const gchar *email = key;

				if (email && *email)
					attendee = e_meeting_store_find_attendee (store, email, row);
			}

			g_hash_table_destroy (aliases);
		}

		if (attendee)
			break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return attendee;
}

EMeetingAttendee *
e_meeting_store_find_attendee (EMeetingStore *store,
                               const gchar *address,
                               gint *row)
{
	EMeetingAttendee *attendee;
	gint i;

	if (address == NULL)
		return NULL;

	for (i = 0; i < store->priv->attendees->len; i++) {
		const gchar *attendee_address;

		attendee = g_ptr_array_index (store->priv->attendees, i);

		attendee_address = e_meeting_attendee_get_address (attendee);
		if (attendee_address && !g_ascii_strcasecmp (
			e_cal_util_strip_mailto (attendee_address),
			e_cal_util_strip_mailto (address))) {
			if (row != NULL)
				*row = i;

			return attendee;
		}
	}

	return NULL;
}

EMeetingAttendee *
e_meeting_store_find_attendee_at_row (EMeetingStore *store,
                                      gint row)
{
	g_return_val_if_fail (E_IS_MEETING_STORE (store), NULL);
	g_return_val_if_fail (ROW_VALID (store, row), NULL);

	return g_ptr_array_index (store->priv->attendees, row);
}

GtkTreePath *
e_meeting_store_find_attendee_path (EMeetingStore *store,
                                    EMeetingAttendee *attendee)
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

static ICalTimezone *
find_zone (ICalProperty *in_prop,
	   ICalComponent *tz_top_level)
{
	ICalParameter *param;
	ICalComponent *subcomp;
	const gchar *tzid;
	ICalCompIter *iter;

	if (tz_top_level == NULL)
		return NULL;

	param = i_cal_property_get_first_parameter (in_prop, I_CAL_TZID_PARAMETER);
	if (param == NULL)
		return NULL;
	tzid = i_cal_parameter_get_tzid (param);

	iter = i_cal_component_begin_component (tz_top_level, I_CAL_VTIMEZONE_COMPONENT);
	subcomp = i_cal_comp_iter_deref (iter);
	while (subcomp) {
		ICalComponent *next_subcomp;
		ICalProperty *prop;

		next_subcomp = i_cal_comp_iter_next (iter);

		prop = i_cal_component_get_first_property (subcomp, I_CAL_TZID_PROPERTY);
		if (prop && !g_strcmp0 (tzid, i_cal_property_get_tzid (prop))) {
			ICalComponent *clone;
			ICalTimezone *zone;

			zone = i_cal_timezone_new ();
			clone = i_cal_component_clone (subcomp);
			i_cal_timezone_set_component (zone, clone);

			g_clear_object (&next_subcomp);
			g_clear_object (&subcomp);
			g_clear_object (&param);
			g_clear_object (&prop);
			g_clear_object (&iter);

			return zone;
		}

		g_clear_object (&prop);
		g_object_unref (subcomp);
		subcomp = next_subcomp;
	}

	g_clear_object (&param);
	g_clear_object (&iter);

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

	g_mutex_lock (&store->priv->mutex);
	store->priv->num_threads--;
	g_mutex_unlock (&store->priv->mutex);

	refresh_queue_remove (qdata->store, qdata->attendee);
	g_object_unref (store);
}

static void
process_free_busy_comp_get_xfb (ICalProperty *ip,
                                gchar **summary,
                                gchar **location)
{
	gchar *tmp;

	g_return_if_fail (ip != NULL);
	g_return_if_fail (summary != NULL && *summary == NULL);
	g_return_if_fail (location != NULL && *location == NULL);

	/* We extract extended free/busy information from the ICalProperty
	 * here (X-SUMMARY and X-LOCATION). If the property carries such,
	 * it will be displayed as a tooltip for the busy period. Otherwise,
	 * nothing will happen (*summary and/or *location will be NULL)
	 */

	tmp = i_cal_property_get_parameter_as_string (ip, E_MEETING_FREE_BUSY_XPROP_SUMMARY);
	*summary = e_meeting_xfb_utf8_string_new_from_ical (tmp, E_MEETING_FREE_BUSY_XPROP_MAXLEN);
	g_free (tmp);

	tmp = i_cal_property_get_parameter_as_string (ip, E_MEETING_FREE_BUSY_XPROP_LOCATION);
	*location = e_meeting_xfb_utf8_string_new_from_ical (tmp, E_MEETING_FREE_BUSY_XPROP_MAXLEN);
	g_free (tmp);
}

static void
process_free_busy_comp (EMeetingAttendee *attendee,
			ICalComponent *fb_comp,
			ICalTimezone *zone,
			ICalComponent *tz_top_level)
{
	ICalProperty *ip;

	ip = i_cal_component_get_first_property (fb_comp, I_CAL_DTSTART_PROPERTY);
	if (ip != NULL) {
		ICalTime *dtstart;
		ICalTimezone *ds_zone;

		dtstart = i_cal_property_get_dtstart (ip);
		if (dtstart) {
			if (!i_cal_time_is_utc (dtstart))
				ds_zone = find_zone (ip, tz_top_level);
			else
				ds_zone = g_object_ref (i_cal_timezone_get_utc_timezone ());
			i_cal_time_convert_timezone (dtstart, ds_zone, zone);

			e_meeting_attendee_set_start_busy_range (
				attendee,
				i_cal_time_get_year (dtstart),
				i_cal_time_get_month (dtstart),
				i_cal_time_get_day (dtstart),
				i_cal_time_get_hour (dtstart),
				i_cal_time_get_minute (dtstart));

			g_clear_object (&ds_zone);
			g_clear_object (&dtstart);
		}
	}
	g_clear_object (&ip);

	ip = i_cal_component_get_first_property (fb_comp, I_CAL_DTEND_PROPERTY);
	if (ip != NULL) {
		ICalTime *dtend;
		ICalTimezone *de_zone;

		dtend = i_cal_property_get_dtend (ip);
		if (dtend) {
			if (!i_cal_time_is_utc (dtend))
				de_zone = find_zone (ip, tz_top_level);
			else
				de_zone = g_object_ref (i_cal_timezone_get_utc_timezone ());
			i_cal_time_convert_timezone (dtend, de_zone, zone);

			e_meeting_attendee_set_end_busy_range (
				attendee,
				i_cal_time_get_year (dtend),
				i_cal_time_get_month (dtend),
				i_cal_time_get_day (dtend),
				i_cal_time_get_hour (dtend),
				i_cal_time_get_minute (dtend));

			g_clear_object (&de_zone);
			g_clear_object (&dtend);
		}
	}
	g_clear_object (&ip);

	for (ip = i_cal_component_get_first_property (fb_comp, I_CAL_FREEBUSY_PROPERTY);
	     ip;
	     g_object_unref (ip), ip = i_cal_component_get_next_property (fb_comp, I_CAL_FREEBUSY_PROPERTY)) {
		ICalParameter *param;
		ICalPeriod *fb;
		EMeetingFreeBusyType busy_type = E_MEETING_FREE_BUSY_LAST;
		ICalParameterFbtype fbtype = I_CAL_FBTYPE_BUSY;

		fb = i_cal_property_get_freebusy (ip);
		param = i_cal_property_get_first_parameter (ip, I_CAL_FBTYPE_PARAMETER);
		if (param) {
			fbtype = i_cal_parameter_get_fbtype (param);
			g_clear_object (&param);
		}

		switch (fbtype) {
		case I_CAL_FBTYPE_BUSY:
			busy_type = E_MEETING_FREE_BUSY_BUSY;
			break;

		case I_CAL_FBTYPE_BUSYUNAVAILABLE:
			busy_type = E_MEETING_FREE_BUSY_OUT_OF_OFFICE;
			break;

		case I_CAL_FBTYPE_BUSYTENTATIVE:
			busy_type = E_MEETING_FREE_BUSY_TENTATIVE;
			break;

		case I_CAL_FBTYPE_FREE:
			busy_type = E_MEETING_FREE_BUSY_FREE;
			break;

		default:
			break;
		}

		if (busy_type != E_MEETING_FREE_BUSY_LAST) {
			ICalTimezone *utc_zone = i_cal_timezone_get_utc_timezone ();
			ICalTime *fbstart, *fbend;
			gchar *summary = NULL;
			gchar *location = NULL;

			fbstart = i_cal_period_get_start (fb);
			fbend = i_cal_period_get_end (fb);

			i_cal_time_convert_timezone (fbstart, utc_zone, zone);
			i_cal_time_convert_timezone (fbend, utc_zone, zone);

			/* Extract extended free/busy (XFB) information from
			 * the ICalProperty, if it carries such.
			 * See the comment for the EMeetingXfbData structure
			 * for a reference.
			 */
			process_free_busy_comp_get_xfb (ip, &summary, &location);

			e_meeting_attendee_add_busy_period (
				attendee,
				i_cal_time_get_year (fbstart),
				i_cal_time_get_month (fbstart),
				i_cal_time_get_day (fbstart),
				i_cal_time_get_hour (fbstart),
				i_cal_time_get_minute (fbstart),
				i_cal_time_get_year (fbend),
				i_cal_time_get_month (fbend),
				i_cal_time_get_day (fbend),
				i_cal_time_get_hour (fbend),
				i_cal_time_get_minute (fbend),
				busy_type,
				summary,
				location);

			g_clear_object (&fbstart);
			g_clear_object (&fbend);
			g_free (summary);
			g_free (location);
		}

		g_clear_object (&fb);
	}
}

static void
process_free_busy (EMeetingStoreQueueData *qdata,
		   const gchar *text)
{
	EMeetingStore *store = qdata->store;
	EMeetingStorePrivate *priv;
	EMeetingAttendee *attendee = qdata->attendee;
	ICalComponent *main_comp;
	ICalComponentKind kind = I_CAL_NO_COMPONENT;

	priv = store->priv;

	main_comp = i_cal_parser_parse_string (text);
	if (main_comp == NULL) {
		process_callbacks (qdata);
		return;
	}

	kind = i_cal_component_isa (main_comp);
	if (kind == I_CAL_VCALENDAR_COMPONENT) {
		ICalCompIter *iter;
		ICalComponent *tz_top_level, *subcomp;

		tz_top_level = e_cal_util_new_top_level ();

		iter = i_cal_component_begin_component (main_comp, I_CAL_VTIMEZONE_COMPONENT);
		subcomp = i_cal_comp_iter_deref (iter);
		while (subcomp) {
			ICalComponent *next_subcomp;

			next_subcomp = i_cal_comp_iter_next (iter);

			i_cal_component_take_component (tz_top_level,
				i_cal_component_clone (subcomp));

			g_object_unref (subcomp);
			subcomp = next_subcomp;
		}

		g_clear_object (&iter);

		iter = i_cal_component_begin_component (main_comp, I_CAL_VFREEBUSY_COMPONENT);
		subcomp = i_cal_comp_iter_deref (iter);
		while (subcomp) {
			ICalComponent *next_subcomp;

			next_subcomp = i_cal_comp_iter_next (iter);

			process_free_busy_comp (attendee, subcomp, priv->zone, tz_top_level);

			g_object_unref (subcomp);
			subcomp = next_subcomp;
		}

		g_clear_object (&iter);
		g_clear_object (&tz_top_level);
	} else if (kind == I_CAL_VFREEBUSY_COMPONENT) {
		process_free_busy_comp (attendee, main_comp, priv->zone, NULL);
	}

	g_clear_object (&main_comp);

	process_callbacks (qdata);
}

/*
 * Replace all instances of from_value in string with to_value
 * In the returned newly allocated string.
*/
static gchar *
replace_string (gchar *string,
                const gchar *from_value,
                gchar *to_value)
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
	ECalClient *client;
	time_t startt;
	time_t endt;
	GSList *users;
	GSList *fb_data;
	gchar *fb_uri;
	gchar *email;
	EMeetingAttendee *attendee;
	EMeetingStoreQueueData *qdata;
	EMeetingStore *store;
} FreeBusyAsyncData;

static void
free_busy_data_free (FreeBusyAsyncData *fbd)
{
	if (fbd) {
		g_slist_free_full (fbd->users, g_free);
		g_free (fbd->email);
		g_slice_free (FreeBusyAsyncData, fbd);
	}
}

#define USER_SUB   "%u"
#define DOMAIN_SUB "%d"

static gpointer
freebusy_async_thread (gpointer data)
{
	FreeBusyAsyncData *fbd = data;
	EMeetingAttendee *attendee = fbd->attendee;
	gchar *default_fb_uri = NULL;
	gchar *fburi = NULL;
	static GMutex mutex;
	EMeetingStorePrivate *priv = fbd->store->priv;

	if (fbd->client) {
		/* FIXME This a workaround for getting all the free busy
		 *       information for the users.  We should be able to
		 *       get free busy asynchronously. */
		g_mutex_lock (&mutex);
		priv->num_queries++;
		e_cal_client_get_free_busy_sync (
			fbd->client, fbd->startt,
			fbd->endt, fbd->users, &fbd->fb_data, NULL, NULL);
		priv->num_queries--;
		g_mutex_unlock (&mutex);

		if (fbd->fb_data != NULL) {
			ECalComponent *comp = fbd->fb_data->data;
			gchar *comp_str;

			comp_str = e_cal_component_get_as_string (comp);
			process_free_busy (fbd->qdata, comp_str);
			g_free (comp_str);

			free_busy_data_free (fbd);
			return NULL;
		}
	}

	/* Look for fburl's of attendee with no free busy info on server */
	if (!e_meeting_attendee_is_set_address (attendee)) {
		process_callbacks (fbd->qdata);
		free_busy_data_free (fbd);
		return NULL;
	}

	/* Check for free busy info on the default server */
	default_fb_uri = g_strdup (fbd->fb_uri);
	fburi = g_strdup (e_meeting_attendee_get_fburi (attendee));

	if (fburi && !*fburi) {
		g_free (fburi);
		fburi = NULL;
	}

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

	free_busy_data_free (fbd);

	return NULL;
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

		qdata = g_hash_table_lookup (
			priv->refresh_data, e_cal_util_strip_mailto (
			e_meeting_attendee_get_address (attendee)));
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

	fbd = g_slice_new0 (FreeBusyAsyncData);
	fbd->client = priv->client;
	fbd->attendee = attendee;
	fbd->users = NULL;
	fbd->fb_data = NULL;
	fbd->qdata = qdata;
	fbd->fb_uri = priv->fb_uri;
	fbd->store = store;
	fbd->email = g_strdup (e_cal_util_strip_mailto (
		e_meeting_attendee_get_address (attendee)));

	/* Check the server for free busy data */
	if (priv->client) {
		ICalTime *itt;

		itt = i_cal_time_new_null_time ();
		i_cal_time_set_date (itt,
			g_date_get_year (&qdata->start.date),
			g_date_get_month (&qdata->start.date),
			g_date_get_day (&qdata->start.date));
		i_cal_time_set_time (itt,
			qdata->start.hour,
			qdata->start.minute,
			0);
		fbd->startt = i_cal_time_as_timet_with_zone (itt, priv->zone);
		g_clear_object (&itt);

		itt = i_cal_time_new_null_time ();
		i_cal_time_set_date (itt,
			g_date_get_year (&qdata->end.date),
			g_date_get_month (&qdata->end.date),
			g_date_get_day (&qdata->end.date));
		i_cal_time_set_time (itt,
			qdata->end.hour,
			qdata->end.minute,
			0);
		fbd->endt = i_cal_time_as_timet_with_zone (itt, priv->zone);
		g_clear_object (&itt);

		fbd->qdata = qdata;
		fbd->users = g_slist_append (fbd->users, g_strdup (fbd->email));

	}

	g_mutex_lock (&store->priv->mutex);
	store->priv->num_threads++;
	g_mutex_unlock (&store->priv->mutex);

	thread = g_thread_try_new (NULL, freebusy_async_thread, fbd, &error);
	if (!thread) {
		free_busy_data_free (fbd);

		priv->refresh_idle_id = 0;

		g_mutex_lock (&store->priv->mutex);
		store->priv->num_threads--;
		g_mutex_unlock (&store->priv->mutex);

		g_object_unref (store);

		return FALSE;
	}

	g_thread_unref (thread);

	return TRUE;
}

static void
refresh_queue_add (EMeetingStore *store,
                   gint row,
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
	if ((attendee == NULL) || !strcmp (e_cal_util_strip_mailto (
		e_meeting_attendee_get_address (attendee)), ""))
		return;

	/* check the queue if the attendee is already in there*/
	for (i = 0; i < priv->refresh_queue->len; i++) {
		if (attendee == g_ptr_array_index (priv->refresh_queue, i))
			return;

		if (!strcmp (e_meeting_attendee_get_address (attendee),
			e_meeting_attendee_get_address (
			g_ptr_array_index (priv->refresh_queue, i))))
			return;
	}

	g_mutex_lock (&priv->mutex);
	qdata = g_hash_table_lookup (
		priv->refresh_data, e_cal_util_strip_mailto (
		e_meeting_attendee_get_address (attendee)));

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

		g_hash_table_insert (
			priv->refresh_data, g_strdup (e_cal_util_strip_mailto (
			e_meeting_attendee_get_address (attendee))), qdata);
	} else {
		if (e_meeting_time_compare_times (start, &qdata->start) == -1)
			qdata->start = *start;
		if (e_meeting_time_compare_times (end, &qdata->end) == -1)
			qdata->end = *end;
		g_ptr_array_add (qdata->call_backs, call_back);
		g_ptr_array_add (qdata->data, data);
	}
	g_mutex_unlock (&priv->mutex);

	g_object_ref (attendee);
	g_ptr_array_add (priv->refresh_queue, attendee);

	if (priv->refresh_idle_id == 0)
		priv->refresh_idle_id = g_idle_add (refresh_busy_periods, store);
}

static void
async_read (GObject *source_object,
            GAsyncResult *result,
            gpointer data)
{
	EMeetingStoreQueueData *qdata = data;
	GError *error = NULL;
	GInputStream *istream;
	gssize read;

	g_return_if_fail (source_object != NULL);
	g_return_if_fail (G_IS_INPUT_STREAM (source_object));

	istream = G_INPUT_STREAM (source_object);

	read = g_input_stream_read_finish (istream, result, &error);

	if (error != NULL) {
		g_warning (
			"Read finish failed: %s", error->message);
		g_error_free (error);

		g_input_stream_close (istream, NULL, NULL);
		g_object_unref (istream);
		process_free_busy (qdata, qdata->string->str);
		return;
	}

	g_return_if_fail (read >= 0);

	if (read == 0) {
		g_input_stream_close (istream, NULL, NULL);
		g_object_unref (istream);
		process_free_busy (qdata, qdata->string->str);
	} else {
		qdata->buffer[read] = '\0';
		g_string_append (qdata->string, qdata->buffer);

		g_input_stream_read_async (
			istream, qdata->buffer, BUF_SIZE - 1,
			G_PRIORITY_DEFAULT, NULL, async_read, qdata);
	}
}

static gboolean
soup_authenticate (SoupMessage *msg,
                   SoupAuth *auth,
                   gboolean retrying,
                   gpointer data)
{
	GUri *guri;
	const gchar *orig_uri;
	gboolean tried = FALSE;

	g_return_val_if_fail (msg != NULL, FALSE);
	g_return_val_if_fail (auth != NULL, FALSE);

	orig_uri = g_object_get_data (G_OBJECT (msg), "orig-uri");
	g_return_val_if_fail (orig_uri != NULL, FALSE);

	guri = g_uri_parse (orig_uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	if (!guri)
		return FALSE;

	if (!g_uri_get_user (guri) || !g_uri_get_user (guri)[0]) {
		g_uri_unref (guri);
		return FALSE;
	}

	if (!retrying) {
		if (g_uri_get_password (guri)) {
			soup_auth_authenticate (auth, g_uri_get_user (guri), g_uri_get_password (guri));
			tried = TRUE;
		} else {
			gchar *password;

			password = e_passwords_get_password (orig_uri);
			if (password) {
				soup_auth_authenticate (auth, g_uri_get_user (guri), password);
				tried = TRUE;

				memset (password, 0, strlen (password));
				g_free (password);
			}
		}
	}

	if (!tried) {
		gboolean remember = FALSE;
		gchar *password, *bold_host, *bold_user;
		GString *description;

		bold_host = g_strconcat ("<b>", g_uri_get_host (guri), "</b>", NULL);
		bold_user = g_strconcat ("<b>", g_uri_get_user (guri), "</b>", NULL);

		description = g_string_new ("");

		g_string_append_printf (
			description, _("Enter password to access "
			"free/busy information on server %s as user %s"),
			bold_host, bold_user);

		g_free (bold_host);
		g_free (bold_user);

		if (retrying && soup_message_get_reason_phrase (msg) && *soup_message_get_reason_phrase (msg)) {
			g_string_append_c (description, '\n');
			g_string_append_printf (
				description, _("Failure reason: %s"),
				soup_message_get_reason_phrase (msg));
		}

		password = e_passwords_ask_password (
			_("Enter password"), orig_uri,
			description->str, E_PASSWORDS_REMEMBER_FOREVER |
			E_PASSWORDS_SECRET | E_PASSWORDS_ONLINE |
			(retrying ? E_PASSWORDS_REPROMPT : 0),
			&remember, NULL);

		g_string_free (description, TRUE);

		if (password) {
			soup_auth_authenticate (auth, g_uri_get_user (guri), password);

			memset (password, 0, strlen (password));
			g_free (password);
		}
	}

	g_uri_unref (guri);

	return FALSE;
}

static void
soup_msg_ready_cb (GObject *source_object,
                   GAsyncResult *result,
                   gpointer user_data)
{
	EMeetingStoreQueueData *qdata = user_data;
	GBytes *bytes;
	GError *local_error = NULL;

	g_return_if_fail (source_object != NULL);
	g_return_if_fail (qdata != NULL);

	bytes = soup_session_send_and_read_finish (SOUP_SESSION (source_object), result, &local_error);
	if (bytes && !local_error) {
		qdata->string = g_string_new_len (
			g_bytes_get_data (bytes, NULL),
			g_bytes_get_size (bytes));
		process_free_busy (qdata, qdata->string->str);
	} else {
		g_warning (
			"Unable to access free/busy url: %s",
			local_error ? local_error->message : "Unknown error");
		process_callbacks (qdata);
	}

	if (bytes)
		g_bytes_unref (bytes);
	g_clear_error (&local_error);
}

static void
download_with_libsoup (const gchar *uri,
                       EMeetingStoreQueueData *qdata)
{
	SoupSession *session;
	SoupMessage *msg;

	g_return_if_fail (uri != NULL);
	g_return_if_fail (qdata != NULL);

	msg = soup_message_new (SOUP_METHOD_GET, uri);
	if (!msg) {
		g_warning ("Unable to access free/busy url '%s'; malformed?", uri);
		process_callbacks (qdata);
		return;
	}

	g_object_set_data_full (G_OBJECT (msg), "orig-uri", g_strdup (uri), g_free);

	session = soup_session_new ();
	g_object_set (session, "timeout", 15, NULL);
	g_signal_connect (
		msg, "authenticate",
		G_CALLBACK (soup_authenticate), NULL);

	soup_message_headers_append (soup_message_get_request_headers (msg), "Connection", "close");
	soup_session_send_and_read_async (session, msg, G_PRIORITY_DEFAULT, NULL, soup_msg_ready_cb, qdata);
}

static void
start_async_read (const gchar *uri,
                  gpointer data)
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

	if (g_error_matches (error, E_SOUP_SESSION_ERROR, SOUP_STATUS_UNAUTHORIZED) ||
	    g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED)) {
		download_with_libsoup (uri, qdata);
		g_object_unref (file);
		g_error_free (error);
		return;
	}

	if (error != NULL) {
		g_warning (
			"Unable to access free/busy url: %s",
			error->message);
		g_error_free (error);
		process_callbacks (qdata);
		g_object_unref (file);
		return;
	}

	if (!istream) {
		process_callbacks (qdata);
		g_object_unref (file);
	} else {
		g_input_stream_read_async (
			istream, qdata->buffer, BUF_SIZE - 1,
			G_PRIORITY_DEFAULT, NULL, async_read, qdata);
	}
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
