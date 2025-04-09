/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CAL_RANGE_MODEL_H
#define E_CAL_RANGE_MODEL_H

#include <libecal/libecal.h>
#include <libedataserver/libedataserver.h>
#include <e-util/e-util.h>

G_BEGIN_DECLS

#define E_TYPE_CAL_RANGE_MODEL e_cal_range_model_get_type ()

G_DECLARE_FINAL_TYPE (ECalRangeModel, e_cal_range_model, E, CAL_RANGE_MODEL, ESourceRegistryWatcher)

/**
 * ECalRangeModelSourceFilterFunc:
 * @source: an #ESource to decide whether include or not
 * @user_data: function user data, as passed to the e_cal_range_model_new()
 *
 * A filter function to determine whether the @source should be used
 * in the #ECalRangeModel or not.
 *
 * Returns: %TRUE to use the @source, %FALSE to not use it
 *
 * Since: 3.58
 **/
typedef gboolean (* ECalRangeModelSourceFilterFunc)
						(ESource *source,
						 gpointer user_data);

ECalRangeModel *e_cal_range_model_new		(EClientCache *client_cache,
						 EAlertSink *alert_sink,
						 ECalRangeModelSourceFilterFunc source_filter_func,
						 gpointer source_filter_user_data);
EClientCache *	e_cal_range_model_ref_client_cache
						(ECalRangeModel *self);
EAlertSink *	e_cal_range_model_ref_alert_sink(ECalRangeModel *self);
void		e_cal_range_model_set_timezone	(ECalRangeModel *self,
						 ICalTimezone *zone);
ICalTimezone *	e_cal_range_model_get_timezone	(ECalRangeModel *self);
void		e_cal_range_model_set_range	(ECalRangeModel *self,
						 time_t start,
						 time_t end);
void		e_cal_range_model_get_range	(ECalRangeModel *self,
						 time_t *out_start,
						 time_t *out_end);
GSList *	e_cal_range_model_get_components(ECalRangeModel *self, /* ECalComponent * */
						 time_t start,
						 time_t end);
void		e_cal_range_model_prepare_dispose
						(ECalRangeModel *self);
gboolean	e_cal_range_model_clamp_to_minutes
						(ECalRangeModel *self,
						 time_t start_tt,
						 guint duration_minutes,
						 guint *out_start_minute,
						 guint *out_duration_minutes);

G_END_DECLS

#endif /* E_CAL_RANGE_MODEL_H */
