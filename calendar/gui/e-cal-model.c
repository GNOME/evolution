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
#include <cal-util/timeutil.h>
#include "calendar-config.h"
#include "e-cal-model.h"
#include "itip-utils.h"
#include "misc.h"

typedef struct {
	CalClient *client;
	CalQuery *query;
} ECalModelClient;

struct _ECalModelPrivate {
	/* The list of clients we are managing. Each element is of type ECalModelClient */
	GList *clients;

	/* Array for storing the objects. Each element is of type ECalModelComponent */
	GPtrArray *objects;

	icalcomponent_kind kind;
	icaltimezone *zone;

	/* The search regular expression */
	gchar *sexp;

	/* The default category */
	gchar *default_category;

	/* Addresses for determining icons */
	EAccountList *accounts;
};

static void e_cal_model_class_init (ECalModelClass *klass);
static void e_cal_model_init (ECalModel *model, ECalModelClass *klass);
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

static GObjectClass *parent_class = NULL;

E_MAKE_TYPE (e_cal_model, "ECalModel", ECalModel, e_cal_model_class_init,
	     e_cal_model_init, E_TABLE_MODEL_TYPE);

static void
e_cal_model_class_init (ECalModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ETableModelClass *etm_class = E_TABLE_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

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
}

static void
e_cal_model_init (ECalModel *model, ECalModelClass *klass)
{
	ECalModelPrivate *priv;

	priv = g_new0 (ECalModelPrivate, 1);
	model->priv = priv;

	priv->sexp = g_strdup ("#t"); /* match all by default */

	priv->objects = g_ptr_array_new ();
	priv->kind = ICAL_NO_COMPONENT;

	priv->accounts = itip_addresses_get ();
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
e_cal_model_finalize (GObject *object)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) object;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	if (priv) {
		if (priv->clients) {
			e_cal_model_remove_all_clients (model);
			priv->clients = NULL;
		}

		if (priv->sexp) {
			g_free (priv->sexp);
			priv->sexp = NULL;
		}

		if (priv->default_category) {
			g_free (priv->default_category);
			priv->default_category = NULL;
		}

		if (priv->objects) {
			clear_objects_array (priv);
			g_ptr_array_free (priv->objects, FALSE);
			priv->objects = NULL;
		}
		
		if (priv->accounts) {
			g_object_unref (priv->accounts);
			priv->accounts = NULL;
		}

		g_free (priv);
		model->priv = NULL;
	}

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

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CLASS_PROPERTY);

	if (prop)
		return (char *) icalproperty_get_class (prop);

	return _("Public");
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

	if (str)
		g_string_free (str, TRUE);

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

		tt_start = icalcomponent_get_dtstart (comp_data->icalcomp);
		if (!icaltime_is_valid_time (tt_start))
			return NULL;

		comp_data->dtstart = g_new0 (ECellDateEditValue, 1);
		comp_data->dtstart->tt = tt_start;

		/* FIXME: handle errors */
		cal_client_get_timezone (comp_data->client,
					 icaltimezone_get_tzid (icaltimezone_get_builtin_timezone (tt_start.zone)),
					 &zone);
		comp_data->dtstart->zone = zone;
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
		return GINT_TO_POINTER (get_color (model, comp_data));
	case E_CAL_MODEL_FIELD_COMPONENT :
		return comp_data->icalcomp;
	case E_CAL_MODEL_FIELD_DESCRIPTION :
		return get_description (comp_data);
	case E_CAL_MODEL_FIELD_DTSTART :
		return get_dtstart (model, comp_data);
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
		return GINT_TO_POINTER ((icalcomponent_get_first_component (comp_data->icalcomp,
									    ICAL_VALARM_COMPONENT) != NULL));
	case E_CAL_MODEL_FIELD_ICON :
	{
		CalComponent *comp;
		icalcomponent *icalcomp;
		gint retval = 0;

		comp = cal_component_new ();
		icalcomp = icalcomponent_new_clone (comp_data->icalcomp);
		if (cal_component_set_icalcomponent (comp, icalcomp)) {
			if (cal_component_has_recurrences (comp))
				retval = 1;
			else if (itip_organizer_is_user (comp, comp_data->client))
				retval = 3;
			else {
				GSList *attendees = NULL, *sl;

				cal_component_get_attendee_list (comp, &attendees);
				for (sl = attendees; sl != NULL; sl = sl->next) {
					CalComponentAttendee *ca = sl->data;
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

				cal_component_free_attendee_list (attendees);
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
set_categories (icalcomponent *icalcomp, const char *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (icalcomp, ICAL_CATEGORIES_PROPERTY);
	if (!value || !(*value)) {
		if (prop) {
			icalcomponent_remove_property (icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (!prop) {
			prop = icalproperty_new_categories (value);
			icalcomponent_add_property (icalcomp, prop);
		} else
			icalproperty_set_categories (prop, value);
	}
}

static void
set_classification (icalcomponent *icalcomp, const char *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (icalcomp, ICAL_CLASS_PROPERTY);
	if (!value || !(*value)) {
		if (prop) {
			icalcomponent_remove_property (icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (!prop) {
			prop = icalproperty_new_class (value);
			icalcomponent_add_property (icalcomp, prop);
		} else
			icalproperty_set_class (prop, value);
	}
}

static void
set_description (icalcomponent *icalcomp, const char *value)
{
	icalproperty *prop;

	/* remove old description(s) */
	prop = icalcomponent_get_first_property (icalcomp, ICAL_DESCRIPTION_PROPERTY);
	while (prop) {
		icalproperty *next;

		next = icalcomponent_get_next_property (icalcomp, ICAL_DESCRIPTION_PROPERTY);

		icalcomponent_remove_property (icalcomp, prop);
		icalproperty_free (prop);

		prop = next;
	}

	/* now add the new description */
	if (!value || !(*value))
		return;

	prop = icalproperty_new_description (value);
	icalcomponent_add_property (icalcomp, prop);
}

static void
set_dtstart (ECalModel *model, ECalModelComponent *comp_data, const void *value)
{
	icalproperty *prop;
	ECellDateEditValue *dv = (ECellDateEditValue *) value;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);
	if (!dv) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else
		icalcomponent_set_dtstart (comp_data->icalcomp, dv->tt);
}

static void
set_summary (icalcomponent *icalcomp, const char *value)
{
	icalcomponent_set_summary (icalcomp, value);
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
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
		set_classification (comp_data, value);
	case E_CAL_MODEL_FIELD_DESCRIPTION :
		set_description (comp_data, value);
	case E_CAL_MODEL_FIELD_DTSTART :
		set_dtstart (model, comp_data, value);
	case E_CAL_MODEL_FIELD_SUMMARY :
		set_summary (comp_data, value);
	}

	if (cal_client_update_objects (comp_data->client, comp_data->icalcomp) != CAL_CLIENT_RESULT_SUCCESS)
		g_message ("ecm_set_value_at(): Could not update the object!");
}

static gboolean
ecm_is_cell_editable (ETableModel *etm, int col, int row)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col <= E_CAL_MODEL_FIELD_LAST, FALSE);

	/* FIXME: We can't check this as 'click-to-add' passes row 0. */
	/*g_return_val_if_fail (row >= 0 && row < priv->objects->len, FALSE);*/

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
	ECalModelComponent *comp_data;
	icalcomponent *icalcomp;
	ECalModel *source_model = (ECalModel *) source;
	ECalModel *model = (ECalModel *) etm;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_MODEL (source_model));

	comp_data = g_ptr_array_index (source_model->priv->objects, row);
	g_assert (comp_data != NULL);

	/* guard against saving before the calendar is open */
	if (!(comp_data->client && cal_client_get_load_state (comp_data->client) == CAL_CLIENT_LOAD_LOADED))
		return;

	icalcomp = e_cal_model_create_component_with_defaults (model);

	set_categories (icalcomp, e_table_model_value_at (source, E_CAL_MODEL_FIELD_CATEGORIES, row));
	set_classification (icalcomp, e_table_model_value_at (source, E_CAL_MODEL_FIELD_CLASSIFICATION, row));
	set_description (icalcomp, e_table_model_value_at (source, E_CAL_MODEL_FIELD_DESCRIPTION, row));
	set_summary (icalcomp, e_table_model_value_at (source, E_CAL_MODEL_FIELD_SUMMARY, row));

	if (cal_client_update_objects (comp_data->client, icalcomp) != CAL_CLIENT_RESULT_SUCCESS) {
		/* FIXME: show error dialog */
	}

	icalcomponent_free (icalcomp);
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
		return g_strdup (priv->default_category);
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
		 * calendar_model_initialize_value().
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

static const char *
ecm_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data)
{
	ECalModelPrivate *priv;
	gint i, pos;
	GList *l;
	gchar *colors[] = { "gray", "green", "darkblue" };

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (comp_data != NULL, NULL);

	priv = model->priv;

	for (l = priv->clients, i = 0; l != NULL; l = l->next, i++) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		if (client_data->client == comp_data->client) {
			pos = i % G_N_ELEMENTS (colors);
			return colors[pos];
		}
	}

	return NULL;
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

static ECalModelComponent *
search_by_uid_and_client (ECalModelPrivate *priv, CalClient *client, const char *uid)
{
	gint i;

	for (i = 0; i < priv->objects->len; i++) {
		ECalModelComponent *comp_data = g_ptr_array_index (priv->objects, i);

		if (comp_data) {
			const char *tmp_uid;

			tmp_uid = icalcomponent_get_uid (comp_data->icalcomp);
			if (tmp_uid && *tmp_uid) {
				if (comp_data->client == client && !strcmp (uid, tmp_uid))
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
query_obj_updated_cb (CalQuery *query, const char *uid,
		      gboolean query_in_progress,
		      int n_scanned, int total,
		      gpointer user_data)
{
	ECalModelPrivate *priv;
	icalcomponent *new_icalcomp;
	CalClientGetStatus status;
	ECalModelComponent *comp_data;
	gint pos;
	ECalModel *model = (ECalModel *) user_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	e_table_model_pre_change (E_TABLE_MODEL (model));

	comp_data = search_by_uid_and_client (priv, cal_query_get_client (query), uid);
	status = cal_client_get_object (cal_query_get_client (query), uid, &new_icalcomp);
	switch (status) {
	case CAL_CLIENT_GET_SUCCESS :
		if (comp_data) {
			if (comp_data->icalcomp)
				icalcomponent_free (comp_data->icalcomp);
			comp_data->icalcomp = new_icalcomp;

			e_table_model_row_changed (E_TABLE_MODEL (model), get_position_in_array (priv->objects, comp_data));
		} else {
			comp_data = g_new0 (ECalModelComponent, 1);
			comp_data->client = cal_query_get_client (query);
			comp_data->icalcomp = new_icalcomp;

			g_ptr_array_add (priv->objects, comp_data);
			e_table_model_row_inserted (E_TABLE_MODEL (model), priv->objects->len - 1);
		}
		break;
	case CAL_CLIENT_GET_NOT_FOUND :
	case CAL_CLIENT_GET_SYNTAX_ERROR :
		if (comp_data) {
			/* Nothing; the object may have been removed from the server. We just
			   notify that the old object was deleted.
			*/
			pos = get_position_in_array (priv->objects, comp_data);

			g_ptr_array_remove (priv->objects, comp_data);
			free_comp_data (comp_data);

			e_table_model_row_deleted (E_TABLE_MODEL (model), pos);
		} else
			e_table_model_no_change (E_TABLE_MODEL (model));
		break;
	default :
		g_assert_not_reached ();
	}
}

static void
query_obj_removed_cb (CalQuery *query, const char *uid, gpointer user_data)
{
	ECalModelComponent *comp_data;
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) user_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	e_table_model_pre_change (E_TABLE_MODEL (model));

	comp_data = search_by_uid_and_client (priv, cal_query_get_client (query), uid);
	if (comp_data) {
		gint pos = get_position_in_array (priv->objects, comp_data);

		g_ptr_array_remove (priv->objects, comp_data);
		free_comp_data (comp_data);

		e_table_model_row_deleted (E_TABLE_MODEL (model), pos);
	} else
		e_table_model_no_change (E_TABLE_MODEL (model));
}

static void
query_done_cb (CalQuery *query, CalQueryDoneStatus status, const char *error_str, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (status != CAL_QUERY_DONE_SUCCESS)
		g_warning ("query done: %s\n", error_str);
}

static void
query_eval_error_cb (CalQuery *query, const char *error_str, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	g_warning ("eval error: %s\n", error_str);
}

/* Builds a complete query sexp for the calendar model by adding the predicates
 * to filter only for the type of objects that the model supports, and
 * whether we want completed tasks.
 */
static char *
adjust_query_sexp (ECalModel *model, const char *sexp)
{
	ECalModelPrivate *priv;
	char *type_sexp, *new_sexp;

	priv = model->priv;

	if (priv->kind == ICAL_NO_COMPONENT)
		type_sexp = g_strdup ("#t");
	else {
		if (priv->kind == ICAL_VEVENT_COMPONENT)
			type_sexp = g_strdup ("(or (= (get-vtype) \"VEVENT\"))");
		else if (priv->kind == ICAL_VTODO_COMPONENT)
			type_sexp = g_strdup ("(or (= (get-vtype) \"VTODO\"))");
		else if (priv->kind == ICAL_VJOURNAL_COMPONENT)
			type_sexp = g_strdup ("(or (= (get-vtype) \"VJOURNAL\"))");
		else
			type_sexp = g_strdup ("#t");
	}

	new_sexp = g_strdup_printf ("(and %s %s)", type_sexp, sexp);
	g_free (type_sexp);

	return new_sexp;
}

static void
update_query_for_client (ECalModel *model, ECalModelClient *client_data)
{
	ECalModelPrivate *priv;
	gchar *real_sexp;

	priv = model->priv;

	/* free the previous query, if any */
	if (client_data->query) {
		g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, model);
		g_object_unref (client_data->query);
	}

	/* prepare the query */
	g_assert (priv->sexp != NULL);
	real_sexp = adjust_query_sexp (model, priv->sexp);

	client_data->query = cal_client_get_query (client_data->client, real_sexp);
	g_free (real_sexp);

	if (!client_data->query) {
		g_message ("update_query_for_client(): Could not create the query");
		return;
	}

	g_signal_connect (client_data->query, "obj_updated", G_CALLBACK (query_obj_updated_cb), model);
	g_signal_connect (client_data->query, "obj_removed", G_CALLBACK (query_obj_removed_cb), model);
	g_signal_connect (client_data->query, "query_done", G_CALLBACK (query_done_cb), model);
	g_signal_connect (client_data->query, "eval_error", G_CALLBACK (query_eval_error_cb), model);
}

static void
add_new_client (ECalModel *model, CalClient *client)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;

	priv = model->priv;

	client_data = g_new0 (ECalModelClient, 1);
	client_data->client = client;
	client_data->query = NULL;
	g_object_ref (client_data->client);

	priv->clients = g_list_append (priv->clients, client_data);

	update_query_for_client (model, client_data);
}

static void
cal_opened_cb (CalClient *client, CalClientOpenStatus status, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;

	if (status != CAL_CLIENT_OPEN_SUCCESS) {
		e_cal_model_remove_client (model, client);
		return;
	}

	add_new_client (model, client);
}

/**
 * e_cal_model_add_client
 */
void
e_cal_model_add_client (ECalModel *model, CalClient *client)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (IS_CAL_CLIENT (client));

	priv = model->priv;

	if (cal_client_get_load_state (client) == CAL_CLIENT_LOAD_LOADED)
		add_new_client (model, client);
	else
		g_signal_connect (client, "cal_opened", G_CALLBACK (cal_opened_cb), model);
}

static void
remove_client (ECalModel *model, ECalModelClient *client_data)
{
	gint i;

	g_signal_handlers_disconnect_matched (client_data->client, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
	g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);

	model->priv->clients = g_list_remove (model->priv->clients, client_data);

	/* remove all objects belonging to this client */
	e_table_model_pre_change (E_TABLE_MODEL (model));
	for (i = 0; i < model->priv->objects->len; i++) {
		ECalModelComponent *comp_data = (ECalModelComponent *) g_ptr_array_index (model->priv->objects, i);

		g_assert (comp_data != NULL);

		if (comp_data->client == client_data->client) {
			g_ptr_array_remove (model->priv->objects, comp_data);
			free_comp_data (comp_data);
		}
	}
	e_table_model_changed (E_TABLE_MODEL (model));

	/* free all remaining memory */
	g_object_unref (client_data->client);
	g_object_unref (client_data->query);
	g_free (client_data);

}

/**
 * e_cal_model_remove_client
 */
void
e_cal_model_remove_client (ECalModel *model, CalClient *client)
{
	GList *l;
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (IS_CAL_CLIENT (client));

	priv = model->priv;
	for (l = priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		if (client_data->client == client) {
			remove_client (model, client_data);
			break;
		}
	}
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

/**
 * e_cal_model_set_query
 */
void
e_cal_model_set_query (ECalModel *model, const char *sexp)
{
	ECalModelPrivate *priv;
	GList *l;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (sexp != NULL);

	priv = model->priv;

	if (priv->sexp)
		g_free (priv->sexp);

	priv->sexp = g_strdup (sexp);

	/* clean up the current contents */
	e_table_model_pre_change (E_TABLE_MODEL (model));
	clear_objects_array (priv);
	e_table_model_changed (E_TABLE_MODEL (model));

	/* update the query for all clients */
	for (l = priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data;

		client_data = (ECalModelClient *) l->data;
		update_query_for_client (model, client_data);
	}
}

/**
 * e_cal_model_create_component_with_defaults
 */
icalcomponent *
e_cal_model_create_component_with_defaults (ECalModel *model)
{
	ECalModelPrivate *priv;
	CalComponent *comp;
	icalcomponent *icalcomp;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (priv->clients != NULL, NULL);

	switch (priv->kind) {
	case ICAL_VEVENT_COMPONENT :
		comp = cal_comp_event_new_with_defaults ((CalClient *) priv->clients->data);
		break;
	case ICAL_VTODO_COMPONENT :
		comp = cal_comp_task_new_with_defaults ((CalClient *) priv->clients->data);
		break;
	default:
		return NULL;
	}

	icalcomp = icalcomponent_new_clone (cal_component_get_icalcomponent (comp));
	g_object_unref (comp);

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

	e_time_format_date_and_time (&tmp_tm, calendar_config_get_24_hour_format (),
				     TRUE, FALSE,
				     buffer, sizeof (buffer));
	return g_strdup (buffer);
}
