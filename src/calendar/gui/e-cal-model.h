/*
 *
 * Evolution calendar - Data model for ETable
 *
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CAL_MODEL_H
#define E_CAL_MODEL_H

#include <libecal/libecal.h>

#include <e-util/e-util.h>
#include <shell/e-shell.h>
#include <calendar/gui/e-cal-data-model.h>

#include "e-cell-date-edit-text.h"

/* Standard GObject macros */
#define E_TYPE_CAL_MODEL \
	(e_cal_model_get_type ())
#define E_CAL_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_MODEL, ECalModel))
#define E_CAL_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_MODEL, ECalModelClass))
#define E_IS_CAL_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_MODEL))
#define E_IS_CAL_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_MODEL))
#define E_CAL_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_MODEL, ECalModelClass))

/* Standard GObject macros */
#define E_TYPE_CAL_MODEL_COMPONENT \
	(e_cal_model_component_get_type ())
#define E_CAL_MODEL_COMPONENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_MODEL_COMPONENT, ECalModelComponent))
#define E_CAL_MODEL_COMPONENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_MODEL_COMPONENT, ECalModelComponentClass))
#define E_IS_CAL_MODEL_COMPONENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_MODEL_COMPONENT))
#define E_IS_CAL_MODEL_COMPONENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_MODEL_COMPONENT))
#define E_CAL_MODEL_COMPONENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_MODEL_COMPONENT, ECalModelComponentClass))

G_BEGIN_DECLS

typedef enum {
	/* If you add new items here or reorder them, you have to update the
	 * .etspec files for the tables using this model */
	E_CAL_MODEL_FIELD_CATEGORIES,
	E_CAL_MODEL_FIELD_CLASSIFICATION,
	E_CAL_MODEL_FIELD_COLOR,            /* not a real field */
	E_CAL_MODEL_FIELD_COMPONENT,        /* not a real field */
	E_CAL_MODEL_FIELD_DESCRIPTION,
	E_CAL_MODEL_FIELD_DTSTART,
	E_CAL_MODEL_FIELD_HAS_ALARMS,       /* not a real field */
	E_CAL_MODEL_FIELD_ICON,             /* not a real field */
	E_CAL_MODEL_FIELD_SUMMARY,
	E_CAL_MODEL_FIELD_UID,
	E_CAL_MODEL_FIELD_CREATED,
	E_CAL_MODEL_FIELD_LASTMODIFIED,
	E_CAL_MODEL_FIELD_SOURCE,           /* not a real field */
	E_CAL_MODEL_FIELD_CANCELLED,        /* not a real field */
	E_CAL_MODEL_FIELD_LAST
} ECalModelField;

typedef struct _ECalModel ECalModel;
typedef struct _ECalModelClass ECalModelClass;
typedef struct _ECalModelPrivate ECalModelPrivate;

typedef struct _ECalModelComponent ECalModelComponent;
typedef struct _ECalModelComponentClass ECalModelComponentClass;
typedef struct _ECalModelComponentPrivate ECalModelComponentPrivate;

struct _ECalModelComponent {
	GObject object;

	ECalClient *client;
	ICalComponent *icalcomp;
	time_t instance_start;
	time_t instance_end;
	gboolean is_new_component;

	/* Private data used by ECalModelCalendar and ECalModelTasks */
	/* keep these public to avoid many accessor functions */
	ECellDateEditValue *dtstart;
	ECellDateEditValue *dtend;
	ECellDateEditValue *due;
	ECellDateEditValue *completed;
	ECellDateEditValue *created;
	ECellDateEditValue *lastmodified;
	gchar *color;

	ECalModelComponentPrivate *priv;
};

struct _ECalModelComponentClass {
	GObjectClass parent_class;
};

typedef struct {
	ECalModelComponent *comp_data;
	gpointer cb_data;
} ECalModelGenerateInstancesData;

struct _ECalModel {
	GObject parent;
	ECalModelPrivate *priv;
};

struct _ECalModelClass {
	GObjectClass parent_class;

	/* virtual methods */
	const gchar *	(*get_color_for_component)
						(ECalModel *model,
						 ECalModelComponent *comp_data);
	void		(*store_values_from_model)
						(ECalModel *model,
						 ETableModel *source_model,
						 gint row,
						 GHashTable *values); /* column ID ~> value */
	void		(*fill_component_from_values)
						(ECalModel *model,
						 ECalModelComponent *comp_data,
						 GHashTable *values); /* column ID ~> value, populated by store_values_from_model() */

	/* Signals */
	void		(*time_range_changed)	(ECalModel *model,
						 gint64 start, /* time_t */
						 gint64 end); /* time_t */
	void		(*row_appended)		(ECalModel *model);
	void		(*comps_deleted)	(ECalModel *model,
						 gpointer list);
	void		(*timezone_changed)	(ECalModel *model,
						 ICalTimezone *old_zone,
						 ICalTimezone *new_zone);
	void		(*object_created)	(ECalModel *model,
						 ECalClient *where);
};

typedef time_t (*ECalModelDefaultTimeFunc) (ECalModel *model, gpointer user_data);

GType		e_cal_model_get_type		(void);
GType		e_cal_model_component_get_type	(void);
ECalDataModel *	e_cal_model_get_data_model	(ECalModel *model);
ESourceRegistry *
		e_cal_model_get_registry	(ECalModel *model);
EShell *	e_cal_model_get_shell		(ECalModel *model);
EClientCache *	e_cal_model_get_client_cache	(ECalModel *model);
ICalComponentKind
		e_cal_model_get_component_kind	(ECalModel *model);
void		e_cal_model_set_component_kind	(ECalModel *model,
						 ICalComponentKind kind);
gboolean	e_cal_model_get_confirm_delete	(ECalModel *model);
void		e_cal_model_set_confirm_delete	(ECalModel *model,
						 gboolean confirm_delete);
ICalTimezone *	e_cal_model_get_timezone	(ECalModel *model);
void		e_cal_model_set_timezone	(ECalModel *model,
						 const ICalTimezone *zone);
gboolean	e_cal_model_get_compress_weekend
						(ECalModel *model);
void		e_cal_model_set_compress_weekend
						(ECalModel *model,
						 gboolean compress_weekend);
void		e_cal_model_set_default_category
						(ECalModel *model,
						 const gchar *default_cat);
gint		e_cal_model_get_default_reminder_interval
						(ECalModel *model);
void		e_cal_model_set_default_reminder_interval
						(ECalModel *model,
						 gint def_reminder_interval);
EDurationType	e_cal_model_get_default_reminder_units
						(ECalModel *model);
void		e_cal_model_set_default_reminder_units
						(ECalModel *model,
						 EDurationType def_reminder_units);
gboolean	e_cal_model_get_use_24_hour_format
						(ECalModel *model);
void		e_cal_model_set_use_24_hour_format
						(ECalModel *model,
						 gboolean use24);
gboolean	e_cal_model_get_use_default_reminder
						(ECalModel *model);
void		e_cal_model_set_use_default_reminder
						(ECalModel *model,
						 gboolean use_def_reminder);
GDateWeekday	e_cal_model_get_week_start_day	(ECalModel *model);
void		e_cal_model_set_week_start_day	(ECalModel *model,
						 GDateWeekday week_start_day);
gboolean	e_cal_model_get_work_day	(ECalModel *model,
						 GDateWeekday weekday);
void		e_cal_model_set_work_day	(ECalModel *model,
						 GDateWeekday weekday,
						 gboolean work_day);
GDateWeekday	e_cal_model_get_work_day_first	(ECalModel *model);
GDateWeekday	e_cal_model_get_work_day_last	(ECalModel *model);
gint		e_cal_model_get_work_day_end_hour
						(ECalModel *model);
void		e_cal_model_set_work_day_end_hour
						(ECalModel *model,
						 gint work_day_end_hour);
gint		e_cal_model_get_work_day_end_minute
						(ECalModel *model);
void		e_cal_model_set_work_day_end_minute
						(ECalModel *model,
						 gint work_day_end_minute);
gint		e_cal_model_get_work_day_start_hour
						(ECalModel *model);
void		e_cal_model_set_work_day_start_hour
						(ECalModel *model,
						 gint work_day_start_hour);
gint		e_cal_model_get_work_day_start_minute
						(ECalModel *model);
void		e_cal_model_set_work_day_start_minute
						(ECalModel *model,
						 gint work_day_start_minute);
gint		e_cal_model_get_work_day_start_mon
						(ECalModel *model);
void		e_cal_model_set_work_day_start_mon
						(ECalModel *model,
						 gint work_day_start);
gint		e_cal_model_get_work_day_end_mon
						(ECalModel *model);
void		e_cal_model_set_work_day_end_mon
						(ECalModel *model,
						 gint work_day_end);
gint		e_cal_model_get_work_day_start_tue
						(ECalModel *model);
void		e_cal_model_set_work_day_start_tue
						(ECalModel *model,
						 gint work_day_start);
gint		e_cal_model_get_work_day_end_tue
						(ECalModel *model);
void		e_cal_model_set_work_day_end_tue
						(ECalModel *model,
						 gint work_day_end);
gint		e_cal_model_get_work_day_start_wed
						(ECalModel *model);
void		e_cal_model_set_work_day_start_wed
						(ECalModel *model,
						 gint work_day_start);
gint		e_cal_model_get_work_day_end_wed
						(ECalModel *model);
void		e_cal_model_set_work_day_end_wed
						(ECalModel *model,
						 gint work_day_end);
gint		e_cal_model_get_work_day_start_thu
						(ECalModel *model);
void		e_cal_model_set_work_day_start_thu
						(ECalModel *model,
						 gint work_day_start);
gint		e_cal_model_get_work_day_end_thu
						(ECalModel *model);
void		e_cal_model_set_work_day_end_thu
						(ECalModel *model,
						 gint work_day_end);
gint		e_cal_model_get_work_day_start_fri
						(ECalModel *model);
void		e_cal_model_set_work_day_start_fri
						(ECalModel *model,
						 gint work_day_start);
gint		e_cal_model_get_work_day_end_fri
						(ECalModel *model);
void		e_cal_model_set_work_day_end_fri
						(ECalModel *model,
						 gint work_day_end);
gint		e_cal_model_get_work_day_start_sat
						(ECalModel *model);
void		e_cal_model_set_work_day_start_sat
						(ECalModel *model,
						 gint work_day_start);
gint		e_cal_model_get_work_day_end_sat
						(ECalModel *model);
void		e_cal_model_set_work_day_end_sat
						(ECalModel *model,
						 gint work_day_end);
gint		e_cal_model_get_work_day_start_sun
						(ECalModel *model);
void		e_cal_model_set_work_day_start_sun
						(ECalModel *model,
						 gint work_day_start);
gint		e_cal_model_get_work_day_end_sun
						(ECalModel *model);
void		e_cal_model_set_work_day_end_sun
						(ECalModel *model,
						 gint work_day_end);
void		e_cal_model_get_work_day_range_for
						(ECalModel *model,
						 GDateWeekday weekday,
						 gint *start_hour,
						 gint *start_minute,
						 gint *end_hour,
						 gint *end_minute);
const gchar *	e_cal_model_get_default_source_uid
						(ECalModel *model);
void		e_cal_model_set_default_source_uid
						(ECalModel *model,
						 const gchar *source_uid);
void		e_cal_model_remove_all_objects	(ECalModel *model);
void		e_cal_model_get_time_range	(ECalModel *model,
						 time_t *start,
						 time_t *end);
void		e_cal_model_set_time_range	(ECalModel *model,
						 time_t start,
						 time_t end);
ICalComponent *	e_cal_model_create_component_with_defaults_sync
						(ECalModel *model,
						 ECalClient *client,
						 gboolean all_day,
						 GCancellable *cancellable,
						 GError **error);
gchar *		e_cal_model_get_attendees_status_info
						(ECalModel *model,
						 ECalComponent *comp,
						 ECalClient *cal_client);
const gchar *	e_cal_model_get_color_for_component
						(ECalModel *model,
						 ECalModelComponent *comp_data);
gboolean	e_cal_model_get_rgba_for_component
						(ECalModel *model,
						 ECalModelComponent *comp_data,
						 GdkRGBA *rgba);
gboolean	e_cal_model_get_rgb_color_for_component
						(ECalModel *model,
						 ECalModelComponent *comp_data,
						 gdouble *red,
						 gdouble *green,
						 gdouble *blue);
ECalModelComponent *
		e_cal_model_get_component_at	(ECalModel *model,
						 gint row);
ECalModelComponent *
		e_cal_model_get_component_for_client_and_uid
						(ECalModel *model,
						 ECalClient *client,
						 const ECalComponentId *id);
gchar *		e_cal_model_date_value_to_string (ECalModel *model,
						 gconstpointer value);
void		e_cal_model_generate_instances_sync
						(ECalModel *model,
						 time_t start,
						 time_t end,
						 GCancellable *cancellable,
						 ECalRecurInstanceCb cb,
						 gpointer cb_data);
GPtrArray *	e_cal_model_get_object_array	(ECalModel *model);
void		e_cal_model_set_instance_times	(ECalModelComponent *comp_data,
						 const ICalTimezone *zone);
gboolean	e_cal_model_test_row_editable	(ECalModel *model,
						 gint row);
void		e_cal_model_set_default_time_func
						(ECalModel *model,
						 ECalModelDefaultTimeFunc func,
						 gpointer user_data);

void		e_cal_model_update_comp_time	(ECalModel *model,
						 ECalModelComponent *comp_data,
						 gconstpointer time_value,
						 ICalPropertyKind kind,
						 void (*set_func) (ICalProperty *prop,
								   ICalTime *v),
						 ICalProperty * (*new_func) (ICalTime *v));

void		e_cal_model_emit_object_created	(ECalModel *model,
						 ECalClient *where);

void		e_cal_model_modify_component	(ECalModel *model,
						 ECalModelComponent *comp_data,
						 ECalObjModType mod);

void		e_cal_model_util_set_value	(GHashTable *values,
						 ETableModel *table_model,
						 gint column,
						 gint row);
gpointer	e_cal_model_util_get_value	(GHashTable *values,
						 gint column);
gpointer	e_cal_model_util_get_status	(ECalModelComponent *comp_data);
ICalPropertyStatus
		e_cal_model_util_set_status	(ECalModelComponent *comp_data,
						 gconstpointer value);
gint		e_cal_model_util_status_compare_cb
						(gconstpointer a,
						 gconstpointer b,
						 gpointer cmp_cache);
ECellDateEditValue *
		e_cal_model_util_get_datetime_value
						(ECalModel *model,
						 ECalModelComponent *comp_data,
						 ICalPropertyKind kind,
						 ICalTime * (*get_time_func) (ICalProperty *prop));
void		e_cal_model_until_sanitize_text_value
						(gchar *value,
						 gint value_length);

G_END_DECLS

#endif /* E_CAL_MODEL_H */
