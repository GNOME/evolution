/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@helixcode.com>
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

/* We need this for strptime. */
#define _XOPEN_SOURCE

#include <config.h>
#include <ctype.h>
#undef _XOPEN_SOURCE
#include <sys/time.h>
#define _XOPEN_SOURCE
#include <time.h>
#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include "calendar-model.h"
#include "calendar-commands.h"



/* Private part of the ECalendarModel structure */
typedef struct {
	/* Calendar client we are using */
	CalClient *client;

	/* Types of objects we are dealing with */
	CalObjType type;

	/* Array of pointers to calendar objects */
	GArray *objects;

	/* UID -> array index hash */
	GHashTable *uid_index_hash;
} CalendarModelPrivate;



static void calendar_model_class_init (CalendarModelClass *class);
static void calendar_model_init (CalendarModel *model);
static void calendar_model_destroy (GtkObject *object);

static int calendar_model_column_count (ETableModel *etm);
static int calendar_model_row_count (ETableModel *etm);
static void *calendar_model_value_at (ETableModel *etm, int col, int row);
static void calendar_model_set_value_at (ETableModel *etm, int col, int row, const void *value);
static gboolean calendar_model_is_cell_editable (ETableModel *etm, int col, int row);
static void calendar_model_append_row (ETableModel *etm, ETableModel *source, gint row);
static void *calendar_model_duplicate_value (ETableModel *etm, int col, const void *value);
static void calendar_model_free_value (ETableModel *etm, int col, void *value);
static void *calendar_model_initialize_value (ETableModel *etm, int col);
static gboolean calendar_model_value_is_empty (ETableModel *etm, int col, const void *value);
#if 0
static char * calendar_model_value_to_string (ETableModel *etm, int col, const void *value);
#endif
static void load_objects (CalendarModel *model);
static int remove_object (CalendarModel *model, const char *uid);

static ETableModelClass *parent_class;



/**
 * calendar_model_get_type:
 * @void:
 *
 * Registers the #CalendarModel class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalendarModel class.
 **/
GtkType
calendar_model_get_type (void)
{
	static GtkType calendar_model_type = 0;

	if (!calendar_model_type) {
		static GtkTypeInfo calendar_model_info = {
			"CalendarModel",
			sizeof (CalendarModel),
			sizeof (CalendarModelClass),
			(GtkClassInitFunc) calendar_model_class_init,
			(GtkObjectInitFunc) calendar_model_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		calendar_model_type = gtk_type_unique (E_TABLE_MODEL_TYPE, &calendar_model_info);
	}

	return calendar_model_type;
}

/* Class initialization function for the calendar table model */
static void
calendar_model_class_init (CalendarModelClass *class)
{
	GtkObjectClass *object_class;
	ETableModelClass *etm_class;

	object_class = (GtkObjectClass *) class;
	etm_class = (ETableModelClass *) class;

	parent_class = gtk_type_class (E_TABLE_MODEL_TYPE);

	object_class->destroy = calendar_model_destroy;

	etm_class->column_count = calendar_model_column_count;
	etm_class->row_count = calendar_model_row_count;
	etm_class->value_at = calendar_model_value_at;
	etm_class->set_value_at = calendar_model_set_value_at;
	etm_class->is_cell_editable = calendar_model_is_cell_editable;
	etm_class->append_row = calendar_model_append_row;
	etm_class->duplicate_value = calendar_model_duplicate_value;
	etm_class->free_value = calendar_model_free_value;
	etm_class->initialize_value = calendar_model_initialize_value;
	etm_class->value_is_empty = calendar_model_value_is_empty;
#if 0
	etm_class->value_to_string = calendar_model_value_to_string;
#endif
}

/* Object initialization function for the calendar table model */
static void
calendar_model_init (CalendarModel *model)
{
	CalendarModelPrivate *priv;

	priv = g_new0 (CalendarModelPrivate, 1);
	model->priv = priv;

	priv->objects = g_array_new (FALSE, TRUE, sizeof (CalComponent *));
	priv->uid_index_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

/* Called from g_hash_table_foreach_remove(), frees a stored UID->index
 * mapping.
 */
static gboolean
free_uid_index (gpointer key, gpointer value, gpointer data)
{
	int *idx;

	idx = value;
	g_free (idx);

	return TRUE;
}

/* Frees the objects stored in the calendar model */
static void
free_objects (CalendarModel *model)
{
	CalendarModelPrivate *priv;
	int i;

	priv = model->priv;

	g_hash_table_foreach_remove (priv->uid_index_hash, free_uid_index, NULL);

	for (i = 0; i < priv->objects->len; i++) {
		CalComponent *comp;

		comp = g_array_index (priv->objects, CalComponent *, i);
		g_assert (comp != NULL);
		gtk_object_unref (GTK_OBJECT (comp));
	}

	g_array_set_size (priv->objects, 0);
}

/* Destroy handler for the calendar table model */
static void
calendar_model_destroy (GtkObject *object)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CALENDAR_MODEL (object));

	model = CALENDAR_MODEL (object);
	priv = model->priv;

	/* Free the calendar client interface object */

	if (priv->client) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->client), model);
		gtk_object_unref (GTK_OBJECT (priv->client));
		priv->client = NULL;
	}

	/* Free the uid->index hash data and the array of UIDs */

	free_objects (model);

	g_hash_table_destroy (priv->uid_index_hash);
	priv->uid_index_hash = NULL;

	g_array_free (priv->objects, TRUE);
	priv->objects = NULL;

	/* Free the private structure */

	g_free (priv);
	model->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* ETableModel methods */

/* column_count handler for the calendar table model */
static int
calendar_model_column_count (ETableModel *etm)
{
	return CAL_COMPONENT_FIELD_NUM_FIELDS;
}

/* row_count handler for the calendar table model */
static int
calendar_model_row_count (ETableModel *etm)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	return priv->objects->len;
}

static char*
get_time_t (time_t *t, gboolean skip_midnight)
{
	static char buffer[32];
	struct tm *tmp_tm;

	if (*t <= 0) {
		buffer[0] = '\0';
	} else {
		tmp_tm = localtime (t);

		if (skip_midnight && tmp_tm->tm_hour == 0
		    && tmp_tm->tm_min == 0 && tmp_tm->tm_sec == 0)
			strftime (buffer, 32, "%a %x", tmp_tm);
		else
			strftime (buffer, 32, "%a %x %T", tmp_tm);
	}

	return buffer;
}

/* Builds a string based on the list of CATEGORIES properties of a calendar
 * component.
 */
static char *
get_categories (CalComponent *comp)
{
	GSList *categories;
	GString *str;
	char *s;
	GSList *l;

	cal_component_get_categories_list (comp, &categories);

	str = g_string_new (NULL);

	for (l = categories; l; l = l->next) {
		const char *category;

		category = l->data;
		g_string_append (str, category);

		if (l->next != NULL)
			g_string_append (str, ", ");
	}

	s = str->str;

	g_string_free (s, FALSE);
	cal_component_free_categories_list (categories);

	return s;
}

/* Returns a string based on the CLASSIFICATION property of a calendar component */
static char *
get_classification (CalComponent *comp)
{
	CalComponentClassification classif;

	cal_component_get_classification (comp, &classif);

	switch (classif) {
	case CAL_COMPONENT_CLASS_NONE:
		return "";

	case CAL_COMPONENT_CLASS_PUBLIC:
		return _("Public");

	case CAL_COMPONENT_CLASS_PRIVATE:
		return _("Private");

	case CAL_COMPONENT_CLASS_CONFIDENTIAL:
		return _("Confidential");

	case CAL_COMPONENT_CLASS_UNKNOWN:
		return _("Unknown");

	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/* Builds a string for the COMPLETED property of a calendar component */
static char *
get_completed (CalComponent *comp)
{
	struct icaltimetype *completed;
	time_t t;

	cal_component_get_completed (comp, &completed);

	if (!completed)
		t = 0;
	else {
		t = time_from_icaltimetype (*completed);
		cal_component_free_icaltimetype (completed);
	}

	return get_time_t (&t, FALSE);
}

/* Builds a string for and frees a date/time value */
static char *
get_and_free_datetime (CalComponentDateTime dt)
{
	time_t t;

	if (!dt.value)
		t = 0;
	else
		t = time_from_icaltimetype (*t.value);

	cal_component_free_datetime (comp, &dt);

	return get_time_t (&t, FALSE);
}

/* Builds a string for the DTEND property of a calendar component */
static char *
get_dtend (CalComponent *comp)
{
	CalComponentDateTime dt;

	cal_component_get_dtend (comp, &dt);
	return get_and_free_datetime (dt);
}

/* Builds a string for the DTSTART property of a calendar component */
static char *
get_dtstart (CalComponent *comp)
{
	CalComponentDateTime dt;

	cal_component_get_dtstart (comp, &dt);
	return get_and_free_datetime (dt);
}

/* Builds a string for the DUE property of a calendar component */
static char *
get_due (CalComponent *comp)
{
	CalComponentDateTime dt;

	cal_component_get_due (comp, &dt);
	return get_and_free_datetime (dt);
}

/* Builds a string for the PERCENT property of a calendar component */
static char*
get_geo (CalComponent *comp)
{
	struct icalgeotype *geo;
	static gchar buf[32];

	cal_component_get_geo (comp, &geo);

	if (!geo)
		buf[0] = '\0';
	else {
		g_snprintf (buf, sizeof (buf), "%g %s, %g %s",
			    fabs (geo->lat),
			    geo->lat >= 0.0 ? _("N") : _("S"),
			    fabs (geo->lon),
			    geo->lon >= 0.0 ? _("E") : _("W"));
		cal_component_free_geo (geo);
	}

	return buf;
}

/* Builds a string for the PERCENT property of a calendar component */
static char *
get_percent (CalComponent *comp)
{
	int *percent;
	static char buf[32];

	cal_component_get_percent (comp, &percent);

	if (!percent)
		buf[0] = '\0';
	else {
		g_snprintf (buf, sizeof (buf), "%d%%", *percent);
		cal_component_free_percent (percent);
	}

	return buf;
}

/* Builds a string for the PRIORITY property of a calendar component */
static char *
get_priority (CalComponent *comp)
{
	int *priority;
	static char buf[32];

	cal_component_get_priority (comp, &priority);

	if (!priority)
		buf[0] = '\0';
	else {
		g_snprintf (buf, sizeof (buf), "%d", *priority);
		cal_component_free_priority (priority);
	}

	return buf;
}

/* Builds a string for the SUMMARY property of a calendar component */
static char *
get_summary (CalComponent *comp)
{
	CalComponentSummary summary;

	cal_component_get_summary (comp, &summary);

	if (summary.value)
		return (char *) summary.value;
	else
		return "";
}

/* Builds a string for the TRANSPARENCY property of a calendar component */
static char *
get_transparency (CalComponent *comp)
{
	CalComponentTransparency transp;

	cal_component_get_transparency (comp, &transp);

	switch (transp) {
	case CAL_COMPONENT_TRANSP_NONE:
		return "";

	case CAL_COMPONENT_TRANSP_TRANSPARENT:
		return _("Transparent");

	case CAL_COMPONENT_TRANSP_OPAQUE:
		return _("Opaque");

	case CAL_COMPONENT_TRANSP_UNKNOWN:
		return _("Unknown");

	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/* Builds a string for the URL property of a calendar component */
static char *
get_url (CalComponent *comp)
{
	const char *url;

	cal_component_get_url (comp, &url);

	if (url)
		return (char *) url;
	else
		return "";
}

/* Returns whether the component has any alarms defined for it */
static gboolean
get_has_alarms (CalComponent *comp)
{
	CalComponentAlarm *alarm;
	gboolean retval;

	alarm = cal_component_get_first_alarm (comp);
	retval = (alarm != NULL);

	cal_component_alarm_free (alarm);
	return retval;
}

/* Returns whether the component has any recurrences defined for it */
static gboolean
get_has_recurrences (CalComponent *comp)
{
	return cal_component_has_rdates (comp) || cal_component_has_rrules (comp);
}

/* Returns whether the completion date has been set on a component */
static gboolean
get_is_complete (CalComponent *comp)
{
	struct icaltimetype *t;
	gboolean retval;

	cal_component_get_completed (comp, &t);
	retval = (t != NULL);

	cal_component_free_icaltimetype (t);
	return retval;
}

/* Returns whether a calendar component is overdue.
 *
 * FIXME: This will only get called when the component is scrolled into the
 * ETable.  There should be some sort of dynamic update thingy for if a component
 * becomes overdue while it is being viewed.
 */
static gboolean
get_is_overdue (CalComponent *comp)
{
	CalComponentDateTime dt;
	gboolean retval;

	cal_component_get_due (comp, &dt);

	/* First, do we have a due date? */

	if (!dt.value)
		retval = FALSE;
	else {
		struct icaltimetype *completed;
		time_t t;

		/* Second, is it already completed? */

		cal_component_get_completed (comp, &completed);

		if (completed) {
			retval = FALSE;

			cal_component_free_icaltimetype (completed);
			goto out;
		}

		/* Third, are we overdue as of right now?
		 *
		 * FIXME: should we check the PERCENT as well?  If it is at 100%
		 * but the COMPLETED property is not set, is the component
		 * really overdue?
		 **/

		t = time_from_icaltimetype (*dt.value);

		/* FIXME */
	}

 out:

	cal_component_free_datetime (&dt);

	return retval;
}

/* value_at handler for the calendar table model */
static void *
calendar_model_value_at (ETableModel *etm, int col, int row)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;
	CalComponent *comp;
	static char buffer[16];

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < CAL_COMPONENT_FIELD_NUM_FIELDS, NULL);
	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	comp = g_array_index (priv->objects, CalComponent *, row);
	g_assert (comp != NULL);

	switch (col) {
	case CAL_COMPONENT_FIELD_CATEGORIES:
		return get_categories (comp);

	case CAL_COMPONENT_FIELD_CLASSIFICATION:
		return get_classification (comp);

	case CAL_COMPONENT_FIELD_COMPLETED:
		return get_completed (comp);

	case CAL_COMPONENT_FIELD_DTEND:
		return get_dtend (comp);

	case CAL_COMPONENT_FIELD_DTSTART:
		return get_dtstart (comp);

	case CAL_COMPONENT_FIELD_DUE:
		return get_due (comp);

	case CAL_COMPONENT_FIELD_GEO:
		return get_geo (comp);

	case CAL_COMPONENT_FIELD_PERCENT:
		return get_percent (comp);

	case CAL_COMPONENT_FIELD_PRIORITY:
		return get_priority (comp);

	case CAL_COMPONENT_FIELD_SUMMARY:
		return get_summary (comp);

	case CAL_COMPONENT_FIELD_TRANSPARENCY:
		return get_transparency (comp);

	case CAL_COMPONENT_FIELD_URL:
		return get_url (comp);

	case CAL_COMPONENT_FIELD_HAS_ALARMS:
		return GINT_TO_POINTER (get_has_alarms (comp));

	case CAL_COMPONENT_FIELD_ICON:
		/* FIXME: Also support 'Assigned to me' & 'Assigned to someone
		   else'. */
		if (get_has_recurrences (comp))
			return GINT_TO_POINTER (1);
		else
			return GINT_TO_POINTER (0);

	case CAL_COMPONENT_FIELD_COMPLETE:
		return GINT_TO_POINTER (get_is_complete (comp));

	case CAL_COMPONENT_FIELD_RECURRING:
		return GINT_TO_POINTER (get_has_recurrences (comp));

	case CAL_COMPONENT_FIELD_OVERDUE:
		return GINT_TO_POINTER (get_is_overdue (comp));
		if (ico->percent != 100
		    && ico->dtend > 0
		    && ico->dtend < time (NULL))
			return GINT_TO_POINTER (TRUE);
		return GINT_TO_POINTER (FALSE);

	case ICAL_OBJECT_FIELD_COLOR:
		if (ico->percent != 100
		    && ico->dtend > 0
		    && ico->dtend < time (NULL))
			return "red";
		return NULL;

	default:
		g_message ("calendar_model_value_at(): Requested invalid column %d", col);
		return NULL;
	}
}

/* Replaces a string */
static void
set_string (char **dest, const char *value)
{
	if (*dest)
		g_free (*dest);

	if (value)
		*dest = g_strdup (value);
	else
		*dest = NULL;
}


static gboolean
string_is_empty (const char *value)
{
	const char *p;
	gboolean empty = TRUE;

	if (value) {
		p = value;
		while (*p) {
			if (!isspace (*p)) {
				empty = FALSE;
				break;
			}
			p++;
		}
	}
	return empty;
}


/* FIXME: We need to set the "transient_for" property for the dialog, but
   the model doesn't know anything about the windows. */
static void
show_date_warning ()
{
	GtkWidget *dialog;
	char buffer[32], message[256];
	time_t t;
	struct tm *tmp_tm;

	t = time (NULL);
	tmp_tm = localtime (&t);
	strftime (buffer, 32, "%a %x %T", tmp_tm);

	g_snprintf (message, 256,
		    _("The date must be entered in the format: \n\n%s"),
		    buffer);

	dialog = gnome_message_box_new (message,
					GNOME_MESSAGE_BOX_ERROR,
					GNOME_STOCK_BUTTON_OK, NULL);
	gtk_widget_show (dialog);
}


/* Replaces a time_t value */
static void
set_time_t (time_t *dest, const char *value)
{
	struct tm tmp_tm;
	struct tm *today_tm;
	time_t t;
	const char *p;

	if (string_is_empty (value)) {
		*dest = 0;
	} else {
		/* Skip any weekday name. */
		p = strptime (value, "%a", &tmp_tm);
		if (!p)
			p = value;

		/* Try to match the full date & time, or without the seconds,
		   or just the date, or just the time with/without seconds.
		   The info pages say we should clear the tm before calling
		   strptime. It also means that if we don't match a time we
		   get 00:00:00 which is good. */
		memset (&tmp_tm, 0, sizeof (tmp_tm));
		if (!strptime (value, "%x %T", &tmp_tm)) {
			memset (&tmp_tm, 0, sizeof (tmp_tm));
			if (!strptime (value, "%x %H:%M", &tmp_tm)) {
				memset (&tmp_tm, 0, sizeof (tmp_tm));
				if (!strptime (value, "%x", &tmp_tm)) {
					memset (&tmp_tm, 0, sizeof (tmp_tm));
					if (!strptime (value, "%T", &tmp_tm)) {
						memset (&tmp_tm, 0, sizeof (tmp_tm));
						if (!strptime (value, "%H:%M", &tmp_tm)) {

							g_warning ("Couldn't parse date string");
							show_date_warning ();
							return;
						}
					}

					/* We only got a time, so we use the
					   current day. */
					t = time (NULL);
					today_tm = localtime (&t);
					tmp_tm.tm_mday = today_tm->tm_mday;
					tmp_tm.tm_mon  = today_tm->tm_mon;
					tmp_tm.tm_year = today_tm->tm_year;
				}
			}
		}

		tmp_tm.tm_isdst = -1;
		*dest = mktime (&tmp_tm);
	}
}


/* FIXME: We need to set the "transient_for" property for the dialog, but
   the model doesn't know anything about the windows. */
static void
show_geo_warning ()
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("The geographical position must be entered in the format: \n\n45.436845,125.862501"),
					GNOME_MESSAGE_BOX_ERROR,
					GNOME_STOCK_BUTTON_OK, NULL);
	gtk_widget_show (dialog);
}


/* Replaces a geo value */
static void
set_geo (iCalGeo *dest, const char *value)
{
	double latitude, longitude;
	gint matched;

	if (!string_is_empty (value)) {
		matched = sscanf (value, "%lg , %lg", &latitude, &longitude);

		if (matched != 2) {
			show_geo_warning ();
		} else {
			dest->valid = TRUE;
			dest->latitude = latitude;
			dest->longitude = longitude;
		}
	} else {
		dest->valid = FALSE;
		dest->latitude = 0.0;
		dest->longitude = 0.0;
	}
}

/* Replaces a person value */
static void
set_person (iCalPerson **dest, const iCalPerson *value)
{
	/* FIXME: This can't be set at present so it shouldn't be called. */
}

/* FIXME: We need to set the "transient_for" property for the dialog, but
   the model doesn't know anything about the windows. */
static void
show_percent_warning ()
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("The percent value must be between 0 and 100"),
					GNOME_MESSAGE_BOX_ERROR,
					GNOME_STOCK_BUTTON_OK, NULL);
	gtk_widget_show (dialog);
}


/* Sets an int value */
static void
set_percent (int *dest, const char *value)
{
	gint matched, percent;

	if (!string_is_empty (value)) {
		matched = sscanf (value, "%i", &percent);

		if (matched != 1 || percent < 0 || percent > 100) {
			show_percent_warning ();
		} else {
			*dest = percent;
		}
	} else {
		*dest = 0;
	}
}

/* FIXME: We need to set the "transient_for" property for the dialog, but
   the model doesn't know anything about the windows. */
static void
show_priority_warning ()
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("The priority must be between 0 and 10"),
					GNOME_MESSAGE_BOX_ERROR,
					GNOME_STOCK_BUTTON_OK, NULL);
	gtk_widget_show (dialog);
}


/* Sets an int value */
static void
set_priority (int *dest, const char *value)
{
	gint matched, priority;

	if (!string_is_empty (value)) {
		matched = sscanf (value, "%i", &priority);

		if (matched != 1 || priority < 0 || priority > 10) {
			show_priority_warning ();
		} else {
			*dest = priority;
		}
	} else {
		*dest = 0;
	}
}

/* set_value_at handler for the calendar table model */
static void
calendar_model_set_value_at (ETableModel *etm, int col, int row, const void *value)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;
	iCalObject *ico;

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	g_return_if_fail (col >= 0 && col < ICAL_OBJECT_FIELD_NUM_FIELDS);
	g_return_if_fail (row >= 0 && row < priv->objects->len);

	ico = g_array_index (priv->objects, iCalObject *, row);
	g_assert (ico != NULL);

	switch (col) {
	case ICAL_OBJECT_FIELD_COMMENT:
		set_string (&ico->comment, value);
		break;

	case ICAL_OBJECT_FIELD_COMPLETED:
		/* FIXME: Set status, percent etc. fields as well. */
		set_time_t (&ico->completed, value);
		break;

	case ICAL_OBJECT_FIELD_DESCRIPTION:
		set_string (&ico->desc, value);
		break;

	case ICAL_OBJECT_FIELD_DTSTAMP:
		set_time_t (&ico->dtstamp, value);
		break;

	case ICAL_OBJECT_FIELD_DTSTART:
		set_time_t (&ico->dtstart, value);
		break;

	case ICAL_OBJECT_FIELD_DTEND:
		set_time_t (&ico->dtend, value);
		break;

	case ICAL_OBJECT_FIELD_GEO:
		set_geo (&ico->geo, value);
		break;

	case ICAL_OBJECT_FIELD_LAST_MOD:
		set_time_t (&ico->last_mod, value);
		break;

	case ICAL_OBJECT_FIELD_LOCATION:
		set_string (&ico->location, value);
		break;

	case ICAL_OBJECT_FIELD_ORGANIZER:
		set_person (&ico->organizer, value);
		break;

	case ICAL_OBJECT_FIELD_PERCENT:
		/* FIXME: If set to 0 or 100 set other fields. */
		set_percent (&ico->percent, value);
		break;

	case ICAL_OBJECT_FIELD_PRIORITY:
		set_priority (&ico->priority, value);
		break;

	case ICAL_OBJECT_FIELD_SUMMARY:
		set_string (&ico->summary, value);
		break;

	case ICAL_OBJECT_FIELD_URL:
		set_string (&ico->url, value);
		break;

	case ICAL_OBJECT_FIELD_COMPLETE:
		/* FIXME: Need a ical_object_XXX function to mark an item
		   complete, which will also set the 'Completed' time and
		   maybe others such as the last modified fields. */
		ico->percent = 100;
		ico->completed = time (NULL);
		break;

	case ICAL_OBJECT_FIELD_HAS_ALARMS:
	case ICAL_OBJECT_FIELD_ICON:
	case ICAL_OBJECT_FIELD_RECURRING:
	case ICAL_OBJECT_FIELD_OVERDUE:
	case ICAL_OBJECT_FIELD_COLOR:
		/* These are all computed fields which can't be set, so we
		   do nothing. Note that the 'click-to-add' item will set all
		   fields when finished, so we don't want to output warnings
		   here. */
		break;

	default:
		g_message ("calendar_model_set_value_at(): Requested invalid column %d", col);
		break;
	}

	if (ico->new)
		g_print ("Skipping update - new iCalObject\n");
	else if (!cal_client_update_object (priv->client, ico))
		g_message ("calendar_model_set_value_at(): Could not update the object!");
}

/* is_cell_editable handler for the calendar table model */
static gboolean
calendar_model_is_cell_editable (ETableModel *etm, int col, int row)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < ICAL_OBJECT_FIELD_NUM_FIELDS, FALSE);
	/* We can't check this as 'click-to-add' passes row 0. */
	/*g_return_val_if_fail (row >= 0 && row < priv->objects->len, FALSE);*/

	switch (col) {
	case ICAL_OBJECT_FIELD_DTSTAMP:
	case ICAL_OBJECT_FIELD_LAST_MOD:
	case ICAL_OBJECT_FIELD_GEO:
	case ICAL_OBJECT_FIELD_HAS_ALARMS:
	case ICAL_OBJECT_FIELD_ICON:
	case ICAL_OBJECT_FIELD_RECURRING:
	case ICAL_OBJECT_FIELD_OVERDUE:
	case ICAL_OBJECT_FIELD_COLOR:
		return FALSE;

	default:
		return TRUE;
	}
}

static void
calendar_model_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;
	iCalObject *ico;
	gint *new_idx, col;

	g_print ("In calendar_model_append_row\n");

	model = CALENDAR_MODEL (etm);
	priv = model->priv;

	ico = ical_new ("", user_name, "");
	ico->type = ICAL_TODO;

	/* Flag the iCalObject as new so all the calls to set_value_at to set
	   each field don't call cal_client_update_object(). We only want to
	   do that once after all the fields are set. */
	ico->new = TRUE;

	g_array_append_val (priv->objects, ico);
	new_idx = g_new (int, 1);
	*new_idx = priv->objects->len - 1;
	g_hash_table_insert (priv->uid_index_hash, ico->uid, new_idx);

	/* Notify the views about the new row. I think we have to do that here,
	   or the views may become confused when they start getting
	   "row_changed" or "cell_changed" signals for this new row. */
	e_table_model_row_inserted (etm, *new_idx);

	for (col = 0; col < ICAL_OBJECT_FIELD_NUM_FIELDS; col++) {
		const void *val = e_table_model_value_at(source, col, row);
		e_table_model_set_value_at(etm, col, *new_idx, val);
	}

	if (!cal_client_update_object (priv->client, ico)) {
		/* FIXME: Show error dialog. */
		g_message ("calendar_model_append_row(): Could not add new object!");
		remove_object (model, ico->uid);
		e_table_model_row_deleted (etm, *new_idx);
	}
}

/* Duplicates a string value */
static char *
dup_string (const char *value)
{
	return g_strdup (value);
}

/* Duplicates a time_t value */
static char *
dup_time_t (const char *value)
{
	return g_strdup (value);

#if 0
	time_t *t;

	t = g_new (time_t, 1);
	*t = *value;
	return t;
#endif
}

/* Duplicates a geo value */
static char *
dup_geo (const char *value)
{
	return g_strdup (value);

#if 0
	iCalGeo *geo;

	geo = g_new (iCalGeo, 1);
	*geo = *value;
	return geo;
#endif
}

/* Duplicates a person value */
static char *
dup_person (const char *value)
{
	/* FIXME */
	return g_strdup (value);
}

/* Duplicates an int value */
static char *
dup_int (const char *value)
{
	return g_strdup (value);

#if 0
	int *v;

	v = g_new (int, 1);
	*v = *value;
	return v;
#endif
}

/* duplicate_value handler for the calendar table model */
static void *
calendar_model_duplicate_value (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < ICAL_OBJECT_FIELD_NUM_FIELDS, NULL);

	switch (col) {
	case ICAL_OBJECT_FIELD_COMMENT:
		return dup_string (value);

	case ICAL_OBJECT_FIELD_COMPLETED:
		return dup_time_t (value);

	case ICAL_OBJECT_FIELD_DESCRIPTION:
		return dup_string (value);

	case ICAL_OBJECT_FIELD_DTSTAMP:
		return dup_time_t (value);

	case ICAL_OBJECT_FIELD_DTSTART:
		return dup_time_t (value);

	case ICAL_OBJECT_FIELD_DTEND:
		return dup_time_t (value);

	case ICAL_OBJECT_FIELD_GEO:
		return dup_geo (value);

	case ICAL_OBJECT_FIELD_LAST_MOD:
		return dup_time_t (value);

	case ICAL_OBJECT_FIELD_LOCATION:
		return dup_string (value);

	case ICAL_OBJECT_FIELD_ORGANIZER:
		return dup_person (value);

	case ICAL_OBJECT_FIELD_PERCENT:
		return dup_int (value);

	case ICAL_OBJECT_FIELD_PRIORITY:
		return dup_int (value);

	case ICAL_OBJECT_FIELD_SUMMARY:
		return dup_string (value);

	case ICAL_OBJECT_FIELD_URL:
		return dup_string (value);

	case ICAL_OBJECT_FIELD_HAS_ALARMS:
		return (void *) value;

	case ICAL_OBJECT_FIELD_ICON:
	case ICAL_OBJECT_FIELD_COMPLETE:
	case ICAL_OBJECT_FIELD_RECURRING:
	case ICAL_OBJECT_FIELD_OVERDUE:
	case ICAL_OBJECT_FIELD_COLOR:
		return (void *) value;

	default:
		g_message ("calendar_model_duplicate_value(): Requested invalid column %d", col);
		return NULL;
	}
}

/* free_value handler for the calendar table model */
static void
calendar_model_free_value (ETableModel *etm, int col, void *value)
{
	g_return_if_fail (col >= 0 && col < ICAL_OBJECT_FIELD_NUM_FIELDS);

	switch (col) {
	case ICAL_OBJECT_FIELD_ORGANIZER:
		/* FIXME: this requires special handling for iCalPerson */

		break;
	case ICAL_OBJECT_FIELD_HAS_ALARMS:
	case ICAL_OBJECT_FIELD_ICON:
	case ICAL_OBJECT_FIELD_COMPLETE:
	case ICAL_OBJECT_FIELD_RECURRING:
	case ICAL_OBJECT_FIELD_OVERDUE:
	case ICAL_OBJECT_FIELD_COLOR:
		/* Do nothing. */
		break;
	default:
		g_free (value);
	}
}

/* Initializes a string value */
static char *
init_string (void)
{
	return g_strdup ("");
}

/* Initializes a time_t value */
static char *
init_time_t (void)
{
	return g_strdup ("");
#if 0
	time_t *t;

	t = g_new (time_t, 1);
	*t = -1;
	return t;
#endif
}

/* Initializes a geo value */
static char *
init_geo (void)
{
	return g_strdup ("");
#if 0
	iCalGeo *geo;

	geo = g_new (iCalGeo, 1);
	geo->valid = FALSE;
	geo->latitude = 0.0;
	geo->longitude = 0.0;
	return geo;
#endif
}

/* Initializes a person value */
static char *
init_person (void)
{
	/* FIXME */
	return g_strdup ("");
}

/* Initializes an int value */
static char *
init_int (void)
{
	return g_strdup ("");

#if 0
	int *v;

	v = g_new (int, 1);
	*v = 0;
	return v;
#endif
}

/* initialize_value handler for the calendar table model */
static void *
calendar_model_initialize_value (ETableModel *etm, int col)
{
	g_return_val_if_fail (col >= 0 && col < ICAL_OBJECT_FIELD_NUM_FIELDS, NULL);

	switch (col) {
	case ICAL_OBJECT_FIELD_COMMENT:
		return init_string ();

	case ICAL_OBJECT_FIELD_COMPLETED:
		return init_time_t ();

	case ICAL_OBJECT_FIELD_DESCRIPTION:
		return init_string ();

	case ICAL_OBJECT_FIELD_DTSTAMP:
		return init_time_t ();

	case ICAL_OBJECT_FIELD_DTSTART:
		return init_time_t ();

	case ICAL_OBJECT_FIELD_DTEND:
		return init_time_t ();

	case ICAL_OBJECT_FIELD_GEO:
		return init_geo ();

	case ICAL_OBJECT_FIELD_LAST_MOD:
		return init_time_t ();

	case ICAL_OBJECT_FIELD_LOCATION:
		return init_string ();

	case ICAL_OBJECT_FIELD_ORGANIZER:
		return init_person ();

	case ICAL_OBJECT_FIELD_PERCENT:
		return init_int ();

	case ICAL_OBJECT_FIELD_PRIORITY:
		return init_int ();

	case ICAL_OBJECT_FIELD_SUMMARY:
		return init_string ();

	case ICAL_OBJECT_FIELD_URL:
		return init_string ();

	case ICAL_OBJECT_FIELD_HAS_ALARMS:
		return NULL; /* "false" */

	case ICAL_OBJECT_FIELD_ICON:
	case ICAL_OBJECT_FIELD_COMPLETE:
	case ICAL_OBJECT_FIELD_RECURRING:
	case ICAL_OBJECT_FIELD_OVERDUE:
		return GINT_TO_POINTER (0);

	case ICAL_OBJECT_FIELD_COLOR:
		return NULL;

	default:
		g_message ("calendar_model_initialize_value(): Requested invalid column %d", col);
		return NULL;
	}
}


/* Returns whether a time_t is empty */
static gboolean
time_t_is_empty (const char *str)
{
	return string_is_empty (str);
#if 0
	return (*t <= 0);
#endif
}

/* Returns whether a geo is empty */
static gboolean
geo_is_empty (const char *str)
{
	return string_is_empty (str);
#if 0
	return !geo->valid;
#endif
}

/* Returns whether a person is empty */
static gboolean
person_is_empty (const char *str)
{
	/* FIXME */
	return string_is_empty (str);
}

/* value_is_empty handler for the calendar model. This should return TRUE
   unless a significant value has been set. The 'click-to-add' feature
   checks all fields to see if any are not empty and if so it adds a new
   row, so we only want to return FALSE if we have a useful object. */
static gboolean
calendar_model_value_is_empty (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < ICAL_OBJECT_FIELD_NUM_FIELDS, TRUE);

	switch (col) {
	case ICAL_OBJECT_FIELD_COMMENT:
		return string_is_empty (value);

	case ICAL_OBJECT_FIELD_COMPLETED:
		return time_t_is_empty (value);

	case ICAL_OBJECT_FIELD_DESCRIPTION:
		return string_is_empty (value);

	case ICAL_OBJECT_FIELD_DTSTAMP:
		return time_t_is_empty (value);

	case ICAL_OBJECT_FIELD_DTSTART:
		return time_t_is_empty (value);

	case ICAL_OBJECT_FIELD_DTEND:
		return time_t_is_empty (value);

	case ICAL_OBJECT_FIELD_GEO:
		return geo_is_empty (value);

	case ICAL_OBJECT_FIELD_LAST_MOD:
		return time_t_is_empty (value);

	case ICAL_OBJECT_FIELD_LOCATION:
		return string_is_empty (value);

	case ICAL_OBJECT_FIELD_ORGANIZER:
		return person_is_empty (value);

	case ICAL_OBJECT_FIELD_PERCENT:
		return string_is_empty (value);

	case ICAL_OBJECT_FIELD_PRIORITY:
		return string_is_empty (value);

	case ICAL_OBJECT_FIELD_SUMMARY:
		return string_is_empty (value);

	case ICAL_OBJECT_FIELD_URL:
		return string_is_empty (value);

	case ICAL_OBJECT_FIELD_HAS_ALARMS:
	case ICAL_OBJECT_FIELD_ICON:
	case ICAL_OBJECT_FIELD_COMPLETE:
	case ICAL_OBJECT_FIELD_RECURRING:
	case ICAL_OBJECT_FIELD_OVERDUE:
	case ICAL_OBJECT_FIELD_COLOR:
		return TRUE;

	default:
		g_message ("calendar_model_value_is_empty(): Requested invalid column %d", col);
		return TRUE;
	}
}



/**
 * calendar_model_new:
 *
 * Creates a new calendar model.  It must be told about the calendar client
 * interface object it will monitor with calendar_model_set_cal_client().
 *
 * Return value: A newly-created calendar model.
 **/
CalendarModel *
calendar_model_new (void)
{
	return CALENDAR_MODEL (gtk_type_new (TYPE_CALENDAR_MODEL));
}


/* Callback used when a calendar is loaded into the server */
static void
cal_loaded_cb (CalClient *client,
	       CalClientLoadStatus status,
	       CalendarModel *model)
{
	g_return_if_fail (IS_CALENDAR_MODEL (model));

	e_table_model_pre_change (E_TABLE_MODEL (model));
	load_objects (model);
	e_table_model_changed (E_TABLE_MODEL (model));
}


/* Removes an object from the model and updates all the indices that follow.
 * Returns the index of the object that was removed, or -1 if no object with
 * such UID was found.
 */
static int
remove_object (CalendarModel *model, const char *uid)
{
	CalendarModelPrivate *priv;
	int *idx;
	iCalObject *orig_ico;
	int i;
	int n;

	priv = model->priv;

	/* Find the index of the object to be removed */

	idx = g_hash_table_lookup (priv->uid_index_hash, uid);
	if (!idx)
		return -1;

	orig_ico = g_array_index (priv->objects, iCalObject *, *idx);
	g_assert (orig_ico != NULL);

	/* Decrease the indices of all the objects that follow in the array */

	for (i = *idx + 1; i < priv->objects->len; i++) {
		iCalObject *ico;
		int *ico_idx;

		ico = g_array_index (priv->objects, iCalObject *, i);
		g_assert (ico != NULL);

		ico_idx = g_hash_table_lookup (priv->uid_index_hash, ico->uid);
		g_assert (ico_idx != NULL);

		(*ico_idx)--;
		g_assert (*ico_idx >= 0);
	}

	/* Remove this object from the array and hash */

	g_hash_table_remove (priv->uid_index_hash, uid);
	g_array_remove_index (priv->objects, *idx);

	ical_object_unref (orig_ico);

	n = *idx;
	g_free (idx);

	return n;
}

/* Callback used when an object is updated in the server */
static void
obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	CalendarModel *model;
	CalendarModelPrivate *priv;
	int orig_idx;
	iCalObject *new_ico;
	int *new_idx;
	CalClientGetStatus status;
	gboolean added;

	model = CALENDAR_MODEL (data);
	priv = model->priv;

	orig_idx = remove_object (model, uid);

	status = cal_client_get_object (priv->client, uid, &new_ico);

	added = FALSE;

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		if (orig_idx == -1) {
			/* The object not in the model originally, so we just append it */

			g_array_append_val (priv->objects, new_ico);

			new_idx = g_new (int, 1);
			*new_idx = priv->objects->len - 1;
			g_hash_table_insert (priv->uid_index_hash, new_ico->uid, new_idx);
		} else {
			int i;

			/* Insert the new version of the object in its old position */

			g_array_insert_val (priv->objects, orig_idx, new_ico);

			new_idx = g_new (int, 1);
			*new_idx = orig_idx;
			g_hash_table_insert (priv->uid_index_hash, new_ico->uid, new_idx);

			/* Increase the indices of all subsequent objects */

			for (i = orig_idx + 1; i < priv->objects->len; i++) {
				iCalObject *ico;
				int *ico_idx;

				ico = g_array_index (priv->objects, iCalObject *, i);
				g_assert (ico != NULL);

				ico_idx = g_hash_table_lookup (priv->uid_index_hash, ico->uid);
				g_assert (ico_idx != NULL);

				(*ico_idx)++;
			}
		}

		e_table_model_row_changed (E_TABLE_MODEL (model), *new_idx);
		break;

	case CAL_CLIENT_GET_NOT_FOUND:
		/* Nothing; the object may have been removed from the server.  We just
		 * notify that the old object was deleted.
		 */
		if (orig_idx != -1)
			e_table_model_row_deleted (E_TABLE_MODEL (model), orig_idx);

		break;

	case CAL_CLIENT_GET_SYNTAX_ERROR:
		g_message ("obj_updated_cb(): Syntax error when getting object `%s'", uid);

		/* Same notification as above */
		if (orig_idx != -1)
			e_table_model_row_deleted (E_TABLE_MODEL (model), orig_idx);

		break;

	default:
		g_assert_not_reached ();
	}
}

/* Callback used when an object is removed in the server */
static void
obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	CalendarModel *model;
	int idx;

	model = CALENDAR_MODEL (data);

	idx = remove_object (model, uid);

	if (idx != -1)
		e_table_model_row_deleted (E_TABLE_MODEL (model), idx);
}

/* Loads the required objects from the calendar client */
static void
load_objects (CalendarModel *model)
{
	CalendarModelPrivate *priv;
	GList *uids;
	GList *l;

	priv = model->priv;

	uids = cal_client_get_uids (priv->client, priv->type);

	for (l = uids; l; l = l->next) {
		char *uid;
		iCalObject *ico;
		CalClientGetStatus status;
		int *idx;

		uid = l->data;
		status = cal_client_get_object (priv->client, uid, &ico);

		switch (status) {
		case CAL_CLIENT_GET_SUCCESS:
			break;

		case CAL_CLIENT_GET_NOT_FOUND:
			/* Nothing; the object may have been removed from the server */
			continue;

		case CAL_CLIENT_GET_SYNTAX_ERROR:
			g_message ("load_objects(): Syntax error when getting object `%s'", uid);
			continue;

		default:
			g_assert_not_reached ();
		}

		g_assert (ico->uid != NULL);

		/* FIXME: Why doesn't it just store the index in the hash
		   table as a GINT_TO_POINTER? - Damon. */
		idx = g_new (int, 1);

		g_array_append_val (priv->objects, ico);
		*idx = priv->objects->len - 1;

		g_hash_table_insert (priv->uid_index_hash, ico->uid, idx);
	}

	cal_obj_uid_list_free (uids);
}

CalClient*
calendar_model_get_cal_client	  (CalendarModel   *model)
{
	CalendarModelPrivate *priv;

	priv = model->priv;

	return priv->client;
}


/**
 * calendar_model_set_cal_client:
 * @model: A calendar model.
 * @client: A calendar client interface object.
 * @type: Type of objects to present.
 *
 * Sets the calendar client interface object that a calendar model will monitor.
 * It also sets the types of objects this model will present to an #ETable.
 **/
void
calendar_model_set_cal_client (CalendarModel *model, CalClient *client, CalObjType type)
{
	CalendarModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_CALENDAR_MODEL (model));

	if (client)
		g_return_if_fail (IS_CAL_CLIENT (client));

	priv = model->priv;

	if (priv->client == client && priv->type == type)
		return;

	e_table_model_pre_change (E_TABLE_MODEL(model));

	if (client)
		gtk_object_ref (GTK_OBJECT (client));

	if (priv->client) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->client), model);
		gtk_object_unref (GTK_OBJECT (priv->client));
	}

	free_objects (model);

	priv->client = client;
	priv->type = type;

	if (priv->client) {
		gtk_signal_connect (GTK_OBJECT (priv->client), "cal_loaded",
				    GTK_SIGNAL_FUNC (cal_loaded_cb), model);
		gtk_signal_connect (GTK_OBJECT (priv->client), "obj_updated",
				    GTK_SIGNAL_FUNC (obj_updated_cb), model);
		gtk_signal_connect (GTK_OBJECT (priv->client), "obj_removed",
				    GTK_SIGNAL_FUNC (obj_removed_cb), model);

		load_objects (model);
	}

	e_table_model_changed (E_TABLE_MODEL (model));
}


void
calendar_model_delete_task (CalendarModel *model,
			    gint row)
{
	CalendarModelPrivate *priv;
	iCalObject *ico;

	priv = model->priv;
	ico = g_array_index (priv->objects, iCalObject *, row);

	if (!cal_client_remove_object (priv->client, ico->uid))
		g_message ("calendar_model_mark_task_complete(): Could not update the object!");
}


void
calendar_model_mark_task_complete (CalendarModel *model,
				   gint row)
{
	CalendarModelPrivate *priv;
	iCalObject *ico;

	priv = model->priv;
	ico = g_array_index (priv->objects, iCalObject *, row);

	/* FIXME: Need a function to do all this. */
	ico->percent = 100;
	ico->completed = time (NULL);

	if (!cal_client_update_object (priv->client, ico))
		g_message ("calendar_model_mark_task_complete(): Could not update the object!");
}


/* Frees the objects stored in the calendar model */
CalComponent *
calendar_model_get_cal_object (CalendarModel *model,
			       gint	      row)
{
	CalendarModelPrivate *priv;

	priv = model->priv;

	return g_array_index (priv->objects, CalComponent *, row);
}
