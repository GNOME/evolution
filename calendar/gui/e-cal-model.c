/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
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

#include <string.h>
#include <glib/garray.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include <e-util/e-time-utils.h>
#include <libecal/e-cal-time-util.h>
#include "comp-util.h"
#include "e-cal-model.h"
#include "itip-utils.h"
#include "misc.h"
#include "e-calendar-marshal.h"

typedef struct {
	ECal *client;
	ECalView *query;
} ECalModelClient;

struct _ECalModelPrivate {
	/* The list of clients we are managing. Each element is of type ECalModelClient */
	GList *clients;

	/* The default client in the list */
	ECal *default_client;
	
	/* Array for storing the objects. Each element is of type ECalModelComponent */
	GPtrArray *objects;

	icalcomponent_kind kind;
	icaltimezone *zone;

	/* The time range to display */
	time_t start;
	time_t end;
	
	/* The search regular expression */
	gchar *search_sexp;

	/* The full regular expression, including time range */
	gchar *full_sexp;

	/* The default category */
	gchar *default_category;

	/* Addresses for determining icons */
	EAccountList *accounts;

	/* Whether we display dates in 24-hour format. */
        gboolean use_24_hour_format;
};

static void e_cal_model_class_init (ECalModelClass *klass);
static void e_cal_model_init (ECalModel *model, ECalModelClass *klass);
static void e_cal_model_dispose (GObject *object);
static void e_cal_model_finalize (GObject *object);

static int ecm_column_count (ETableModel *etm);
static int ecm_row_count (ETableModel *etm);
static void *ecm_value_at (ETableModel *etm, int col, int row);
static void ecm_set_value_at (ETableModel *etm, int col, int row, const void *value);
static gboolean ecm_is_cell_editable (ETableModel *etm, int col, int row);
static void ecm_append_row (ETableModel *etm, ETableModel *source, int row);
static void *ecm_duplicate_value (ETableModel *etm, int col, const void *value);
static void ecm_free_value (ETableModel *etm, int col, void *value);
static void *ecm_initialize_value (ETableModel *etm, int col);
static gboolean ecm_value_is_empty (ETableModel *etm, int col, const void *value);
static char *ecm_value_to_string (ETableModel *etm, int col, const void *value);

static const char *ecm_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data);

/* Signal IDs */
enum {
	TIME_RANGE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

E_MAKE_TYPE (e_cal_model, "ECalModel", ECalModel, e_cal_model_class_init,
	     e_cal_model_init, E_TABLE_MODEL_TYPE);

static void
e_cal_model_class_init (ECalModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ETableModelClass *etm_class = E_TABLE_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = e_cal_model_dispose;
	object_class->finalize = e_cal_model_finalize;

	etm_class->column_count = ecm_column_count;
	etm_class->row_count = ecm_row_count;
	etm_class->value_at = ecm_value_at;
	etm_class->set_value_at = ecm_set_value_at;
	etm_class->is_cell_editable = ecm_is_cell_editable;
	etm_class->append_row = ecm_append_row;
	etm_class->duplicate_value = ecm_duplicate_value;
	etm_class->free_value = ecm_free_value;
	etm_class->initialize_value = ecm_initialize_value;
	etm_class->value_is_empty = ecm_value_is_empty;
	etm_class->value_to_string = ecm_value_to_string;

	klass->get_color_for_component = ecm_get_color_for_component;
	klass->fill_component_from_model = NULL;

	signals[TIME_RANGE_CHANGED] =
		g_signal_new ("time_range_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, time_range_changed),
			      NULL, NULL,
			      e_calendar_marshal_VOID__LONG_LONG,
			      G_TYPE_NONE, 2, G_TYPE_LONG, G_TYPE_LONG);	
}

static void
e_cal_model_init (ECalModel *model, ECalModelClass *klass)
{
	ECalModelPrivate *priv;

	priv = g_new0 (ECalModelPrivate, 1);
	model->priv = priv;

	/* match none by default */
	priv->start = -1;
	priv->end = -1;
	priv->search_sexp = NULL;
	priv->full_sexp = g_strdup ("#f");

	priv->objects = g_ptr_array_new ();
	priv->kind = ICAL_NO_COMPONENT;

	priv->accounts = itip_addresses_get ();

	priv->use_24_hour_format = TRUE;
}

static void
free_comp_data (ECalModelComponent *comp_data)
{
	g_return_if_fail (comp_data != NULL);

	comp_data->client = NULL;

	if (comp_data->icalcomp) {
		icalcomponent_free (comp_data->icalcomp);
		comp_data->icalcomp = NULL;
	}

	if (comp_data->dtstart) {
		g_free (comp_data->dtstart);
		comp_data->dtstart = NULL;
	}

	if (comp_data->dtend) {
		g_free (comp_data->dtend);
		comp_data->dtend = NULL;
	}

	if (comp_data->due) {
		g_free (comp_data->due);
		comp_data->due = NULL;
	}

	if (comp_data->completed) {
		g_free (comp_data->completed);
		comp_data->completed = NULL;
	}

	if (comp_data->color) {
		g_free (comp_data->color);
		comp_data->color = NULL;
	}

	g_free (comp_data);
}

static void
clear_objects_array (ECalModelPrivate *priv)
{
	gint i;

	for (i = 0; i < priv->objects->len; i++) {
		ECalModelComponent *comp_data;

		comp_data = g_ptr_array_index (priv->objects, i);
		g_assert (comp_data != NULL);
		free_comp_data (comp_data);
	}

	
	g_ptr_array_set_size (priv->objects, 0);
}

static void
e_cal_model_dispose (GObject *object)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) object;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (priv->clients) {
		while (priv->clients != NULL) {
			ECalModelClient *client_data = (ECalModelClient *) priv->clients->data;
			
			g_signal_handlers_disconnect_matched (client_data->client, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, model);
			if (client_data->query)
				g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA,
								      0, 0, NULL, NULL, model);
			
			priv->clients = g_list_remove (priv->clients, client_data);


			g_object_unref (client_data->client);
			if (client_data->query)
				g_object_unref (client_data->query);
			g_free (client_data);
		}

		priv->clients = NULL;
	}

	if (parent_class->dispose)
		parent_class->dispose (object);
}

static void
e_cal_model_finalize (GObject *object)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) object;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	g_free (priv->search_sexp);
	g_free (priv->full_sexp);

	g_free (priv->default_category);

	clear_objects_array (priv);
	g_ptr_array_free (priv->objects, FALSE);

	g_free (priv);

	if (parent_class->finalize)
		parent_class->finalize (object);
}

/* ETableModel methods */

static int
ecm_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_FIELD_LAST;
}

static int
ecm_row_count (ETableModel *etm)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	priv = model->priv;

	return priv->objects->len;
}

static char *
get_categories (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY);
	if (prop)
		return (char *) icalproperty_get_categories (prop);

	return "";
}

static char *
get_classification (ECalModelComponent *comp_data)
{
	icalproperty *prop;
	icalproperty_class class;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CLASS_PROPERTY);

	if (!prop)
		return _("Public");

	class = icalproperty_get_class (prop);

	switch (class)
	{
	case ICAL_CLASS_PUBLIC:
		return _("Public");
	case ICAL_CLASS_PRIVATE:
		return _("Private");
	case ICAL_CLASS_CONFIDENTIAL:
		return _("Confidential");
	default:
		return _("Unknown");
	}

	return _("Unknown");
}

static const char *
get_color (ECalModel *model, ECalModelComponent *comp_data)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return e_cal_model_get_color_for_component (model, comp_data);
}

static char *
get_description (ECalModelComponent *comp_data)
{
	icalproperty *prop;
	static GString *str = NULL;

	if (str) {
		g_string_free (str, TRUE);
		str = NULL;
	}
	
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);
	if (prop) {
		str = g_string_new ("");
		do {
			str = g_string_append (str, icalproperty_get_description (prop));
		} while ((prop = icalcomponent_get_next_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY)));

		return str->str;
	}

	return "";
}

static ECellDateEditValue*
get_dtstart (ECalModel *model, ECalModelComponent *comp_data)
{
	ECalModelPrivate *priv;
        struct icaltimetype tt_start;

	priv = model->priv;

	if (!comp_data->dtstart) {
		icaltimezone *zone;
		icalproperty *prop;

		prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);
		if (!prop)
			return NULL;

		tt_start = icalproperty_get_dtstart (prop);
		if (!icaltime_is_valid_time (tt_start))
			return NULL;

		comp_data->dtstart = g_new0 (ECellDateEditValue, 1);
		comp_data->dtstart->tt = tt_start;

		if (icaltime_get_tzid (tt_start)
		    && e_cal_get_timezone (comp_data->client, icaltime_get_tzid (tt_start), &zone, NULL)) 
			comp_data->dtstart->zone = zone;
		else
			comp_data->dtstart->zone = NULL;
	}

	return comp_data->dtstart;
}

static char *
get_summary (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_SUMMARY_PROPERTY);
	if (prop)
		return (char *) icalproperty_get_summary (prop);

	return "";
}

static char *
get_uid (ECalModelComponent *comp_data)
{
	return (char *) icalcomponent_get_uid (comp_data->icalcomp);
}

static void *
ecm_value_at (ETableModel *etm, int col, int row)
{
	ECalModelPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	comp_data = g_ptr_array_index (priv->objects, row);
	g_assert (comp_data != NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		return get_categories (comp_data);
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
		return get_classification (comp_data);
	case E_CAL_MODEL_FIELD_COLOR :
		return (void *) get_color (model, comp_data);
	case E_CAL_MODEL_FIELD_COMPONENT :
		return comp_data->icalcomp;
	case E_CAL_MODEL_FIELD_DESCRIPTION :
		return get_description (comp_data);
	case E_CAL_MODEL_FIELD_DTSTART :
		return (void *) get_dtstart (model, comp_data);
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
		return GINT_TO_POINTER ((icalcomponent_get_first_component (comp_data->icalcomp,
									    ICAL_VALARM_COMPONENT) != NULL));
	case E_CAL_MODEL_FIELD_ICON :
	{
		ECalComponent *comp;
		icalcomponent *icalcomp;
		gint retval = 0;

		comp = e_cal_component_new ();
		icalcomp = icalcomponent_new_clone (comp_data->icalcomp);
		if (e_cal_component_set_icalcomponent (comp, icalcomp)) {
			if (e_cal_component_has_recurrences (comp))
				retval = 1;
			else if (itip_organizer_is_user (comp, comp_data->client))
				retval = 3;
			else {
				GSList *attendees = NULL, *sl;

				e_cal_component_get_attendee_list (comp, &attendees);
				for (sl = attendees; sl != NULL; sl = sl->next) {
					ECalComponentAttendee *ca = sl->data;
					const char *text;

					text = itip_strip_mailto (ca->value);
					if (e_account_list_find (priv->accounts, E_ACCOUNT_FIND_ID_ADDRESS, text) != NULL) {
						if (ca->delto != NULL)
							retval = 3;
						else
							retval = 2;
						break;
					}
				}

				e_cal_component_free_attendee_list (attendees);
			}
		} else
			icalcomponent_free (icalcomp);

		g_object_unref (comp);

		return GINT_TO_POINTER (retval);
	}
	case E_CAL_MODEL_FIELD_SUMMARY :
		return get_summary (comp_data);
	case E_CAL_MODEL_FIELD_UID :
		return get_uid (comp_data);
	}

	return "";
}

static void
set_categories (ECalModelComponent *comp_data, const char *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY);
	if (!value || !(*value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (!prop) {
			prop = icalproperty_new_categories (value);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		} else
			icalproperty_set_categories (prop, value);
	}
}

static void
set_classification (ECalModelComponent *comp_data, const char *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CLASS_PROPERTY);
	if (!value || !(*value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
	  icalproperty_class ical_class;

	  if (!strcasecmp (value, "PUBLIC"))
	    ical_class = ICAL_CLASS_PUBLIC;
	  else if (!strcasecmp (value, "PRIVATE"))
	    ical_class = ICAL_CLASS_PRIVATE;
	  else if (!strcasecmp (value, "CONFIDENTIAL"))
	    ical_class = ICAL_CLASS_CONFIDENTIAL;
	  else
	    ical_class = ICAL_CLASS_NONE;

		if (!prop) {
			prop = icalproperty_new_class (ical_class);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		} else
			icalproperty_set_class (prop, ical_class);
	}
}

static void
set_description (ECalModelComponent *comp_data, const char *value)
{
	icalproperty *prop;

	/* remove old description(s) */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);
	while (prop) {
		icalproperty *next;

		next = icalcomponent_get_next_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);

		icalcomponent_remove_property (comp_data->icalcomp, prop);
		icalproperty_free (prop);

		prop = next;
	}

	/* now add the new description */
	if (!value || !(*value))
		return;

	prop = icalproperty_new_description (value);
	icalcomponent_add_property (comp_data->icalcomp, prop);
}

static void
set_dtstart (ECalModel *model, ECalModelComponent *comp_data, const void *value)
{
	ECellDateEditValue *dv = (ECellDateEditValue *) value;
	icalproperty *prop;
	icalparameter *param;
	const char *tzid;
	
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);
	if (prop)
		param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);
	else
		param = NULL;

	/* If we are setting the property to NULL (i.e. removing it), then
	   we remove it if it exists. */
	if (!dv) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}

		return;
	}
	
	/* If the TZID is set to "UTC", we set the is_utc flag. */
	tzid = dv->zone ? icaltimezone_get_tzid (dv->zone) : "UTC";
	if (tzid && !strcmp (tzid, "UTC"))
		dv->tt.is_utc = 1;
	else
		dv->tt.is_utc = 0;

	if (prop) {
		icalproperty_set_dtstart (prop, dv->tt);
	} else {
		prop = icalproperty_new_dtstart (dv->tt);
		icalcomponent_add_property (comp_data->icalcomp, prop);
	}

	/* If the TZID is set to "UTC", we don't want to save the TZID. */
	if (tzid && strcmp (tzid, "UTC")) {
		if (param) {
			icalparameter_set_tzid (param, (char *) tzid);
		} else {
			param = icalparameter_new_tzid ((char *) tzid);
			icalproperty_add_parameter (prop, param);
		}
	} else if (param) {
		icalproperty_remove_parameter (prop, ICAL_TZID_PARAMETER);
	}
}

static void
set_summary (ECalModelComponent *comp_data, const char *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_SUMMARY_PROPERTY);

	if (string_is_empty (value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (prop)
			icalproperty_set_summary (prop, value);
		else {
			prop = icalproperty_new_summary (value);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}
	}
}

static void
ecm_set_value_at (ETableModel *etm, int col, int row, const void *value)
{
	ECalModelPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < priv->objects->len);

	comp_data = g_ptr_array_index (priv->objects, row);
	g_assert (comp_data != NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		set_categories (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
		set_classification (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DESCRIPTION :
		set_description (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DTSTART :
		set_dtstart (model, comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_SUMMARY :
		set_summary (comp_data, value);
		break;
	}

	/* FIXME ask about mod type */
	if (!e_cal_modify_object (comp_data->client, comp_data->icalcomp, CALOBJ_MOD_ALL, NULL)) {
		g_warning (G_STRLOC ": Could not modify the object!");
		
		/* FIXME Show error dialog */
	}
}

static gboolean
ecm_is_cell_editable (ETableModel *etm, int col, int row)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col <= E_CAL_MODEL_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < priv->objects->len), FALSE);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return TRUE;
	}

	return FALSE;
}

static void
ecm_append_row (ETableModel *etm, ETableModel *source, int row)
{
	ECalModelClass *model_class;
	ECalModelComponent comp_data;
	ECalModel *model = (ECalModel *) etm;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_TABLE_MODEL (source));

	memset (&comp_data, 0, sizeof (comp_data));
	comp_data.client = e_cal_model_get_default_client (model);

	/* guard against saving before the calendar is open */
	if (!(comp_data.client && e_cal_get_load_state (comp_data.client) == E_CAL_LOAD_LOADED))
		return;

	comp_data.icalcomp = e_cal_model_create_component_with_defaults (model);

	/* set values for our fields */
	set_categories (&comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_CATEGORIES, row));
	set_classification (&comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_CLASSIFICATION, row));
	set_description (&comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_DESCRIPTION, row));
	set_dtstart (model, &comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_DTSTART, row));
	set_summary (&comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_SUMMARY, row));

	/* call the class' method for filling the component */
	model_class = (ECalModelClass *) G_OBJECT_GET_CLASS (model);
	if (model_class->fill_component_from_model != NULL) {
		model_class->fill_component_from_model (model, &comp_data, source, row);
	}


	if (!e_cal_create_object (comp_data.client, comp_data.icalcomp, NULL, NULL)) {
		g_warning (G_STRLOC ": Could not create the object!");

		/* FIXME: show error dialog */
	}

	icalcomponent_free (comp_data.icalcomp);
}

static void *
ecm_duplicate_value (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup (value);
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
		return (void *) value;
	case E_CAL_MODEL_FIELD_COMPONENT :
		return icalcomponent_new_clone ((icalcomponent *) value);
	case E_CAL_MODEL_FIELD_DTSTART :
		if (value) {
			ECellDateEditValue *dv, *orig_dv;

			orig_dv = (ECellDateEditValue *) value;
			dv = g_new0 (ECellDateEditValue, 1);
			*dv = *orig_dv;

			return dv;
		}
		break;
	}

	return NULL;
}

static void
ecm_free_value (ETableModel *etm, int col, void *value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		if (value)
			g_free (value);
		break;
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
		break;
	case E_CAL_MODEL_FIELD_DTSTART :
		if (value)
			g_free (value);
		break;
	case E_CAL_MODEL_FIELD_COMPONENT :
		if (value)
			icalcomponent_free ((icalcomponent *) value);
		break;
	}
}

static void *
ecm_initialize_value (ETableModel *etm, int col)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	priv = model->priv;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		return g_strdup (priv->default_category?priv->default_category:"");
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup ("");
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return NULL;
	}

	return NULL;
}

static gboolean
ecm_value_is_empty (ETableModel *etm, int col, const void *value)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), TRUE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, TRUE);

	priv = model->priv;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		/* This could be a hack or not.  If the categories field only
		 * contains the default category, then it possibly means that
		 * the user has not entered anything at all in the click-to-add;
		 * the category is in the value because we put it there in
		 * ecm_initialize_value().
		 */
		if (priv->default_category && value && strcmp (priv->default_category, value) == 0)
			return TRUE;
		else
			return string_is_empty (value);
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return string_is_empty (value);
	case E_CAL_MODEL_FIELD_DTSTART :
		return value ? FALSE : TRUE;
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return TRUE;
	}

	return TRUE;
}

static char *
ecm_value_to_string (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup (value);
	case E_CAL_MODEL_FIELD_DTSTART :
		return e_cal_model_date_value_to_string (E_CAL_MODEL (etm), value);
	case E_CAL_MODEL_FIELD_ICON :
		if (GPOINTER_TO_INT (value) == 0)
			return _("Normal");
		else if (GPOINTER_TO_INT (value) == 1)
			return _("Recurring");
		else
			return _("Assigned");
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
		return value ? _("Yes") : _("No");
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return NULL;
	}

	return NULL;
}

/* ECalModel class methods */

typedef struct {
	const gchar *color;
	GList *uris;
} AssignedColorData;

static const char *
ecm_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data)
{
	ESource *source;
	guint32 source_color;
	ECalModelPrivate *priv;
	gint i, first_empty = 0;
	static AssignedColorData assigned_colors[] = {
		{ "#BECEDD", NULL }, /* 190 206 221     Blue */
		{ "#E2F0EF", NULL }, /* 226 240 239     Light Blue */
		{ "#C6E2B7", NULL }, /* 198 226 183     Green */
		{ "#E2F0D3", NULL }, /* 226 240 211     Light Green */
		{ "#E2D4B7", NULL }, /* 226 212 183     Khaki */
		{ "#EAEAC1", NULL }, /* 234 234 193     Light Khaki */
		{ "#F0B8B7", NULL }, /* 240 184 183     Pink */
		{ "#FED4D3", NULL }, /* 254 212 211     Light Pink */
		{ "#E2C6E1", NULL }, /* 226 198 225     Purple */
		{ "#F0E2EF", NULL }  /* 240 226 239     Light Purple */
	};

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;
             
	source = e_cal_get_source (comp_data->client);
	if (e_source_get_color (source, &source_color)) {
		g_free (comp_data->color);
		comp_data->color = g_strdup_printf ("#%06x", source_color & 0xffffff);
		return comp_data->color;
	}
                                                                   
	for (i = 0; i < G_N_ELEMENTS (assigned_colors); i++) {
		GList *l;

		if (assigned_colors[i].uris == NULL) {
			first_empty = i;
			continue;
		}

		for (l = assigned_colors[i].uris; l != NULL; l = l->next) {
			if (!strcmp ((const char *) l->data,
				     e_cal_get_uri (comp_data->client)))
			{
				return assigned_colors[i].color;
			}
		}
	}

	/* return the first unused color */
	assigned_colors[first_empty].uris = g_list_append (assigned_colors[first_empty].uris,
							   g_strdup (e_cal_get_uri (comp_data->client)));

	return assigned_colors[first_empty].color;
}

/**
 * e_cal_model_get_component_kind
 */
icalcomponent_kind
e_cal_model_get_component_kind (ECalModel *model)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), ICAL_NO_COMPONENT);

	priv = model->priv;
	return priv->kind;
}

/**
 * e_cal_model_set_component_kind
 */
void
e_cal_model_set_component_kind (ECalModel *model, icalcomponent_kind kind)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	priv->kind = kind;
}

/**
 * e_cal_model_get_timezone
 */
icaltimezone *
e_cal_model_get_timezone (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	return model->priv->zone;
}

/**
 * e_cal_model_set_timezone
 */
void
e_cal_model_set_timezone (ECalModel *model, icaltimezone *zone)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	if (priv->zone != zone) {
		e_table_model_pre_change (E_TABLE_MODEL (model));
		priv->zone = zone;

		/* the timezone affects the times shown for date fields,
		   so we need to redisplay everything */
		e_table_model_changed (E_TABLE_MODEL (model));
	}
}

/**
 * e_cal_model_set_default_category
 */
void
e_cal_model_set_default_category (ECalModel *model, const gchar *default_cat)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->default_category)
		g_free (model->priv->default_category);

	model->priv->default_category = g_strdup (default_cat);
}

/**
 * e_cal_model_get_use_24_hour_format
 */
gboolean
e_cal_model_get_use_24_hour_format (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	return model->priv->use_24_hour_format;
}

/**
 * e_cal_model_set_use_24_hour_format
 */
void
e_cal_model_set_use_24_hour_format (ECalModel *model, gboolean use24)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->use_24_hour_format != use24) {
                e_table_model_pre_change (E_TABLE_MODEL (model));
                model->priv->use_24_hour_format = use24;
                /* Get the views to redraw themselves. */
                e_table_model_changed (E_TABLE_MODEL (model));
        }
}

/**
 * e_cal_model_get_default_client
 */
ECal *
e_cal_model_get_default_client (ECalModel *model)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	
	priv = model->priv;

	/* we always return a valid ECal, since we rely on it in many places */
	if (priv->default_client)
		return priv->default_client;

	if (!priv->clients)
		return NULL;

	client_data = (ECalModelClient *) priv->clients->data;

	return client_data ? client_data->client : NULL;
}

void
e_cal_model_set_default_client (ECalModel *model, ECal *client)
{
	ECalModelPrivate *priv;
	
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (client != NULL);
	g_return_if_fail (E_IS_CAL (client));

	priv = model->priv;

	/* Make sure its in the model */
	e_cal_model_add_client (model, client);

	/* Store the default client */	
	priv->default_client = e_cal_model_get_client_for_uri (model, e_cal_get_uri (client));
}

/**
 * e_cal_model_get_client_list
 */
GList *
e_cal_model_get_client_list (ECalModel *model)
{
	GList *list = NULL, *l;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	for (l = model->priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		list = g_list_append (list, client_data->client);
	}

	return list;
}

/**
 * e_cal_model_get_client_for_uri
 * @model: A calendar model.
 * @uri: Uri for the client to get.
 */
ECal *
e_cal_model_get_client_for_uri (ECalModel *model, const char *uri)
{
	GList *l;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	for (l = model->priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		if (!strcmp (uri, e_cal_get_uri (client_data->client)))
			return client_data->client;
	}

	return NULL;
}

static ECalModelClient *
find_client_data (ECalModel *model, ECal *client)
{
	ECalModelPrivate *priv;
	GList *l;
	
	priv = model->priv;

	for (l = priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		if (client_data->client == client)
			return client_data;
	}	

	return NULL;
}

/* Pass NULL for the client if we just want to find based on uid */
/* FIXME how do we prevent the same UID is different calendars? */
static ECalModelComponent *
search_by_uid_and_client (ECalModelPrivate *priv, ECal *client, const char *uid)
{
	gint i;

	for (i = 0; i < priv->objects->len; i++) {
		ECalModelComponent *comp_data = g_ptr_array_index (priv->objects, i);

		if (comp_data) {
			const char *tmp_uid;

			tmp_uid = icalcomponent_get_uid (comp_data->icalcomp);
			if (tmp_uid && *tmp_uid) {
				if ((!client || comp_data->client == client) && !strcmp (uid, tmp_uid))
					return comp_data;
			}
		}
	}

	return NULL;
}

static gint
get_position_in_array (GPtrArray *objects, gpointer item)
{
	gint i;

	for (i = 0; i < objects->len; i++) {
		if (g_ptr_array_index (objects, i) == item)
			return i;
	}

	return -1;
}

static void
e_cal_view_objects_added_cb (ECalView *query, GList *objects, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;
	ECalModelPrivate *priv;
	GList *l;

	priv = model->priv;

	for (l = objects; l; l = l->next) {
		ECalModelComponent *comp_data;

		e_table_model_pre_change (E_TABLE_MODEL (model));

		comp_data = g_new0 (ECalModelComponent, 1);
		comp_data->client = g_object_ref (e_cal_view_get_client (query));
		comp_data->icalcomp = icalcomponent_new_clone (l->data);
		
		g_ptr_array_add (priv->objects, comp_data);

		e_table_model_row_inserted (E_TABLE_MODEL (model), priv->objects->len - 1);
	}
}

static void
e_cal_view_objects_modified_cb (ECalView *query, GList *objects, gpointer user_data)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) user_data;
	GList *l;
	
	priv = model->priv;

	for (l = objects; l; l = l->next) {
		ECalModelComponent *comp_data;

		e_table_model_pre_change (E_TABLE_MODEL (model));

		comp_data = search_by_uid_and_client (priv, e_cal_view_get_client (query), icalcomponent_get_uid (l->data));
		if (!comp_data)
			continue;
	
		if (comp_data->icalcomp)
			icalcomponent_free (comp_data->icalcomp);
		if (comp_data->dtstart) {
			g_free (comp_data->dtstart);
			comp_data->dtstart = NULL;
		}
		if (comp_data->dtend) {
			g_free (comp_data->dtend);
			comp_data->dtend = NULL;
		}
		if (comp_data->due) {
			g_free (comp_data->due);
			comp_data->due = NULL;
		}
		if (comp_data->completed) {
			g_free (comp_data->completed);
			comp_data->completed = NULL;
		}
		if (comp_data->color) {
			g_free (comp_data->color);
			comp_data->color = NULL;
		}
		     
		comp_data->icalcomp = icalcomponent_new_clone (l->data);

		e_table_model_row_changed (E_TABLE_MODEL (model), get_position_in_array (priv->objects, comp_data));
	}
}

static void
e_cal_view_objects_removed_cb (ECalView *query, GList *uids, gpointer user_data)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) user_data;
	GList *l;
	
	priv = model->priv;

	for (l = uids; l; l = l->next) {
		ECalModelComponent *comp_data;
		int pos;

		e_table_model_pre_change (E_TABLE_MODEL (model));
		
		comp_data = search_by_uid_and_client (priv, e_cal_view_get_client (query), l->data);
		if (!comp_data)
			continue;
		
		pos = get_position_in_array (priv->objects, comp_data);
		
		g_ptr_array_remove (priv->objects, comp_data);
		free_comp_data (comp_data);
		
		e_table_model_row_deleted (E_TABLE_MODEL (model), pos);
	}
}

static void
e_cal_view_progress_cb (ECalView *query, const char *message, int percent, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	/* FIXME Update status bar */
}

static void
e_cal_view_done_cb (ECalView *query, ECalendarStatus status, gpointer user_data)
{
 	ECalModel *model = (ECalModel *) user_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	/* FIXME Clear status bar */
}

static void
update_e_cal_view_for_client (ECalModel *model, ECalModelClient *client_data)
{
	ECalModelPrivate *priv;

	priv = model->priv;

	/* Skip if this client has not finished loading yet */
	if (e_cal_get_load_state (client_data->client) != E_CAL_LOAD_LOADED)
		return;
	
	/* free the previous query, if any */
	if (client_data->query) {
		g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, model);
		g_object_unref (client_data->query);
		client_data->query = NULL;
	}

	/* prepare the query */
	g_assert (priv->full_sexp != NULL);

	if (!e_cal_get_query (client_data->client, priv->full_sexp, &client_data->query, NULL)) {
		g_warning (G_STRLOC ": Unable to get query");

		return;
	}	

	g_signal_connect (client_data->query, "objects_added", G_CALLBACK (e_cal_view_objects_added_cb), model);
	g_signal_connect (client_data->query, "objects_modified", G_CALLBACK (e_cal_view_objects_modified_cb), model);
	g_signal_connect (client_data->query, "objects_removed", G_CALLBACK (e_cal_view_objects_removed_cb), model);
	g_signal_connect (client_data->query, "view_progress", G_CALLBACK (e_cal_view_progress_cb), model);
	g_signal_connect (client_data->query, "view_done", G_CALLBACK (e_cal_view_done_cb), model);

	e_cal_view_start (client_data->query);
}

static void
backend_died_cb (ECal *client, gpointer user_data)
{
	ECalModel *model;

	model = E_CAL_MODEL (user_data);

	e_cal_model_remove_client (model, client);
}

static void
cal_opened_cb (ECal *client, ECalendarStatus status, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;
	ECalModelClient *client_data;

	if (status != E_CALENDAR_STATUS_OK) {
		e_cal_model_remove_client (model, client);

		return;
	}
	
	/* Stop listening for this calendar to be opened */
	g_signal_handlers_disconnect_matched (client, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, NULL, cal_opened_cb, model);

	client_data = find_client_data (model, client);
	g_assert (client_data);
	
	update_e_cal_view_for_client (model, client_data);
}

static ECalModelClient *
add_new_client (ECalModel *model, ECal *client)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;

	priv = model->priv;

	client_data = g_new0 (ECalModelClient, 1);
	client_data->client = client;
	client_data->query = NULL;
	g_object_ref (client_data->client);

	priv->clients = g_list_append (priv->clients, client_data);

	g_signal_connect (G_OBJECT (client_data->client), "backend_died",
			  G_CALLBACK (backend_died_cb), model);

	return client_data;
}

/**
 * e_cal_model_add_client
 */
void
e_cal_model_add_client (ECalModel *model, ECal *client)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;
	
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL (client));

	priv = model->priv;

	if (e_cal_model_get_client_for_uri (model, e_cal_get_uri (client)))
		return;

	client_data = add_new_client (model, client);	
	if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED) {
		update_e_cal_view_for_client (model, client_data);
	} else {
		g_signal_connect (client, "cal_opened", G_CALLBACK (cal_opened_cb), model);
		e_cal_open_async (client, TRUE);
	}
}

static void
remove_client (ECalModel *model, ECalModelClient *client_data)
{
	gint i;

	g_signal_handlers_disconnect_matched (client_data->client, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
	if (client_data->query)
		g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);

	model->priv->clients = g_list_remove (model->priv->clients, client_data);

	/* remove all objects belonging to this client */
	for (i = model->priv->objects->len; i > 0; i--) {
		ECalModelComponent *comp_data = (ECalModelComponent *) g_ptr_array_index (model->priv->objects, i - 1);

		g_assert (comp_data != NULL);

		if (comp_data->client == client_data->client) {
			e_table_model_pre_change (E_TABLE_MODEL (model));
			
			g_ptr_array_remove (model->priv->objects, comp_data);
			free_comp_data (comp_data);

			e_table_model_row_deleted (E_TABLE_MODEL (model), i - 1);
		}
	}

	/* If this was the default client, unset it */
	if (model->priv->default_client == client_data->client)
		model->priv->default_client = NULL;
	
	/* free all remaining memory */
	g_object_unref (client_data->client);
	if (client_data->query)
		g_object_unref (client_data->query);
	g_free (client_data);
}

/**
 * e_cal_model_remove_client
 */
void
e_cal_model_remove_client (ECalModel *model, ECal *client)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;
	
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL (client));

	priv = model->priv;

	client_data = find_client_data (model, client);
	if (client_data)
		remove_client (model, client_data);
}

/**
 * e_cal_model_remove_all_clients
 */
void
e_cal_model_remove_all_clients (ECalModel *model)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	while (priv->clients != NULL) {
		ECalModelClient *client_data = (ECalModelClient *) priv->clients->data;
		remove_client (model, client_data);
	}
}

static void
redo_queries (ECalModel *model)
{
	ECalModelPrivate *priv;
	char *iso_start, *iso_end;
	GList *l;
	int len;
	
	priv = model->priv;

	if (priv->full_sexp)
		g_free (priv->full_sexp);

	if (priv->start != -1 && priv->end != -1) {
		iso_start = isodate_from_time_t (priv->start);
		iso_end = isodate_from_time_t (priv->end);
		
		priv->full_sexp = g_strdup_printf ("(and (occur-in-time-range? (make-time \"%s\")"
						   "                           (make-time \"%s\"))"
						   "     %s)",
						   iso_start, iso_end, 
						   priv->search_sexp ? priv->search_sexp : "");
	} else if (priv->search_sexp) {
		priv->full_sexp = g_strdup (priv->search_sexp);
	} else {
		priv->full_sexp = g_strdup ("#f");
	}	
	
	/* clean up the current contents */
	e_table_model_pre_change (E_TABLE_MODEL (model));
	len = priv->objects->len;
	clear_objects_array (priv);
	e_table_model_rows_deleted (E_TABLE_MODEL (model), 0, len);
	
	/* update the query for all clients */
	for (l = priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data;
		
		client_data = (ECalModelClient *) l->data;
		update_e_cal_view_for_client (model, client_data);
	}
}

void
e_cal_model_get_time_range (ECalModel *model, time_t *start, time_t *end)
{
	ECalModelPrivate *priv;
	
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	
	if (start)
		*start = priv->start;
	
	if (end)
		*end = priv->end;
}

void
e_cal_model_set_time_range (ECalModel *model, time_t start, time_t end)
{
	ECalModelPrivate *priv;
	
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (start >= 0 && end >= 0);
	g_return_if_fail (start <= end);

	priv = model->priv;

	if (priv->start == start && priv->end == end)
		return;
	
	priv->start = start;
	priv->end = end;

	g_signal_emit (G_OBJECT (model), signals[TIME_RANGE_CHANGED], 0, start, end);
	redo_queries (model);
}

const char *
e_cal_model_get_search_query (ECalModel *model)
{
	ECalModelPrivate *priv;
	
	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;
	
	return priv->search_sexp;
}

/**
 * e_cal_model_set_query
 */
void
e_cal_model_set_search_query (ECalModel *model, const char *sexp)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (priv->search_sexp)
		g_free (priv->search_sexp);

	priv->search_sexp = g_strdup (sexp);

	redo_queries (model);
}

/**
 * e_cal_model_create_component_with_defaults
 */
icalcomponent *
e_cal_model_create_component_with_defaults (ECalModel *model)
{
	ECalModelPrivate *priv;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	ECal *client;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (priv->clients != NULL, NULL);

	client = e_cal_model_get_default_client (model);
	if (!client)
		return icalcomponent_new (priv->kind);

	switch (priv->kind) {
	case ICAL_VEVENT_COMPONENT :
		comp = cal_comp_event_new_with_defaults (client);
		break;
	case ICAL_VTODO_COMPONENT :
		comp = cal_comp_task_new_with_defaults (client);
		break;
	default:
		return NULL;
	}

	if (!comp)
		return icalcomponent_new (priv->kind);

	icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp));
	g_object_unref (comp);

	/* make sure the component has an UID */
	if (!icalcomponent_get_uid (icalcomp)) {
		char *uid;

		uid = e_cal_component_gen_uid ();
		icalcomponent_set_uid (icalcomp, uid);

		g_free (uid);
	}

	return icalcomp;
}

/**
 * e_cal_model_get_color_for_component
 */
const gchar *
e_cal_model_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data)
{
	ECalModelClass *model_class;
	const gchar *color = NULL;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (comp_data != NULL, NULL);

	model_class = (ECalModelClass *) G_OBJECT_GET_CLASS (model);
	if (model_class->get_color_for_component != NULL)
		color = model_class->get_color_for_component (model, comp_data);

	if (!color)
		color = ecm_get_color_for_component (model, comp_data);

	return color;
}

/**
 * e_cal_model_get_rgb_color_for_component
 */
gboolean
e_cal_model_get_rgb_color_for_component (ECalModel *model, ECalModelComponent *comp_data, double *red, double *green, double *blue)
{
	GdkColor gdk_color;
	const gchar *color;

	color = e_cal_model_get_color_for_component (model, comp_data);
	if (color && gdk_color_parse (color, &gdk_color)) {

		if (red)
			*red = ((double) gdk_color.red)/0xffff;
		if (green)
			*green = ((double) gdk_color.green)/0xffff;
		if (blue)
			*blue = ((double) gdk_color.blue)/0xffff;

		return TRUE;
	}

	return FALSE;
}

/**
 * e_cal_model_get_component_at
 */
ECalModelComponent *
e_cal_model_get_component_at (ECalModel *model, gint row)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	return g_ptr_array_index (priv->objects, row);
}

ECalModelComponent *
e_cal_model_get_component_for_uid  (ECalModel *model, const char *uid)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	return search_by_uid_and_client (priv, NULL, uid);
}

/**
 * e_cal_model_date_value_to_string
 */
gchar*
e_cal_model_date_value_to_string (ECalModel *model, const void *value)
{
	ECalModelPrivate *priv;
	ECellDateEditValue *dv = (ECellDateEditValue *) value;
	struct icaltimetype tt;
	struct tm tmp_tm;
	char buffer[64];

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	if (!dv)
		return g_strdup ("");

	/* We currently convert all the dates to the current timezone. */
	tt = dv->tt;
	icaltimezone_convert_time (&tt, dv->zone, priv->zone);

	tmp_tm.tm_year = tt.year - 1900;
	tmp_tm.tm_mon = tt.month - 1;
	tmp_tm.tm_mday = tt.day;
	tmp_tm.tm_hour = tt.hour;
	tmp_tm.tm_min = tt.minute;
	tmp_tm.tm_sec = tt.second;
	tmp_tm.tm_isdst = -1;

	tmp_tm.tm_wday = time_day_of_week (tt.day, tt.month - 1, tt.year);

	memset (buffer, 0, sizeof (buffer));
	e_time_format_date_and_time (&tmp_tm, priv->use_24_hour_format,
				     TRUE, FALSE,
				     buffer, sizeof (buffer));
	return g_strdup (buffer);
}

static ECellDateEditValue *
copy_ecdv (ECellDateEditValue *ecdv)
{
	ECellDateEditValue *new_ecdv;
	
	
	new_ecdv = g_new0 (ECellDateEditValue, 1);
	new_ecdv->tt = ecdv->tt;
	new_ecdv->zone = ecdv->zone;

	return new_ecdv;
}

/**
 * e_cal_model_copy_component_data
 */
ECalModelComponent *
e_cal_model_copy_component_data (ECalModelComponent *comp_data)
{
	ECalModelComponent *new_data;

	g_return_val_if_fail (comp_data != NULL, NULL);

	new_data = g_new0 (ECalModelComponent, 1);

	if (comp_data->icalcomp)
		new_data->icalcomp = icalcomponent_new_clone (comp_data->icalcomp);
	if (comp_data->client)
		new_data->client = g_object_ref (comp_data->client);
	if (comp_data->dtstart)
		new_data->dtstart = copy_ecdv (comp_data->dtstart);
	if (comp_data->dtend)
		new_data->dtend = copy_ecdv (comp_data->dtend);
	if (comp_data->due)
		new_data->due = copy_ecdv (comp_data->due);
	if (comp_data->completed)
		new_data->completed = copy_ecdv (comp_data->completed);

	return new_data;
}

/**
 * e_cal_model_free_component_data
 */
void
e_cal_model_free_component_data (ECalModelComponent *comp_data)
{
	g_return_if_fail (comp_data != NULL);

	if (comp_data->client)
		g_object_unref (comp_data->client);
	if (comp_data->icalcomp)
		icalcomponent_free (comp_data->icalcomp);
	if (comp_data->dtstart)
		g_free (comp_data->dtstart);
	if (comp_data->dtend)
		g_free (comp_data->dtend);
	if (comp_data->due)
		g_free (comp_data->due);
	if (comp_data->completed)
		g_free (comp_data->completed);
	if (comp_data->color)
		g_free (comp_data->color);

	g_free (comp_data);
}

/**
 * e_cal_model_generate_instances
 *
 * cb function is not called with cb_data, but with ECalModelGenerateInstancesData which contains cb_data
 */
void
e_cal_model_generate_instances (ECalModel *model, time_t start, time_t end,
				ECalRecurInstanceFn cb, gpointer cb_data)
{
	ECalModelGenerateInstancesData mdata;
	gint i, n;

	n = e_table_model_row_count (E_TABLE_MODEL (model));
	for (i = 0; i < n; i ++) {
		ECalModelComponent *comp_data = e_cal_model_get_component_at (model, i);

		mdata.comp_data = comp_data;
		mdata.cb_data = cb_data;
		e_cal_generate_instances_for_object (comp_data->client, comp_data->icalcomp, start, end, cb, &mdata);
	}
}
