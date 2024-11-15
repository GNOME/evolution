/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#ifndef E_CAL_DATA_MODEL_H
#define E_CAL_DATA_MODEL_H

#include <libecal/libecal.h>
#include <e-util/e-util.h>

#include "e-cal-data-model-subscriber.h"

/* Standard GObject macros */
#define E_TYPE_CAL_DATA_MODEL \
	(e_cal_data_model_get_type ())
#define E_CAL_DATA_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_DATA_MODEL, ECalDataModel))
#define E_CAL_DATA_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_DATA_MODEL, ECalDataModelClass))
#define E_IS_CAL_DATA_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_DATA_MODEL))
#define E_IS_CAL_DATA_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_DATA_MODEL))
#define E_CAL_DATA_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_DATA_MODEL, ECalDataModelClass))

G_BEGIN_DECLS

typedef enum {
	E_CAL_DATA_MODEL_VIEW_STATE_START,
	E_CAL_DATA_MODEL_VIEW_STATE_PROGRESS,
	E_CAL_DATA_MODEL_VIEW_STATE_COMPLETE,
	E_CAL_DATA_MODEL_VIEW_STATE_STOP
} ECalDataModelViewState;

typedef struct _ECalDataModel ECalDataModel;
typedef struct _ECalDataModelClass ECalDataModelClass;
typedef struct _ECalDataModelPrivate ECalDataModelPrivate;

struct _ECalDataModel {
	GObject parent;
	ECalDataModelPrivate *priv;
};

struct _ECalDataModelClass {
	GObjectClass parent_class;

	/* Signals */
	void (* view_state_changed)	(ECalDataModel *data_model,
					 ECalClientView *view,
					 ECalDataModelViewState state,
					 guint percent,
					 const gchar *message,
					 const GError *error);
};

typedef GCancellable * (* ECalDataModelSubmitThreadJobFunc)
						(GObject *responder,
						 const gchar *description,
						 const gchar *alert_ident,
						 const gchar *alert_arg_0,
						 EAlertSinkThreadJobFunc func,
						 gpointer user_data,
						 GDestroyNotify free_user_data);

GType		e_cal_data_model_get_type	(void);
ECalDataModel *	e_cal_data_model_new		(ESourceRegistry *registry,
						 ECalDataModelSubmitThreadJobFunc func,
						 GObject *func_responder);
ECalDataModel * e_cal_data_model_new_clone	(ECalDataModel *src_data_model);
GCancellable *	e_cal_data_model_submit_thread_job
						(ECalDataModel *data_model,
						 const gchar *description,
						 const gchar *alert_ident,
						 const gchar *alert_arg_0,
						 EAlertSinkThreadJobFunc func,
						 gpointer user_data,
						 GDestroyNotify free_user_data);
ESourceRegistry *
		e_cal_data_model_get_registry	(ECalDataModel *data_model);
gboolean	e_cal_data_model_get_disposing	(ECalDataModel *data_model);
void		e_cal_data_model_set_disposing	(ECalDataModel *data_model,
						 gboolean disposing);
gboolean	e_cal_data_model_get_expand_recurrences
						(ECalDataModel *data_model);
void		e_cal_data_model_set_expand_recurrences
						(ECalDataModel *data_model,
						 gboolean expand_recurrences);
gboolean	e_cal_data_model_get_skip_cancelled
						(ECalDataModel *data_model);
void		e_cal_data_model_set_skip_cancelled
						(ECalDataModel *data_model,
						 gboolean expand_recurrences);
ICalTimezone *	e_cal_data_model_get_timezone	(ECalDataModel *data_model);
void		e_cal_data_model_set_timezone	(ECalDataModel *data_model,
						 ICalTimezone *zone);
void		e_cal_data_model_set_filter	(ECalDataModel *data_model,
						 const gchar *sexp);
gchar *		e_cal_data_model_dup_filter	(ECalDataModel *data_model);
void		e_cal_data_model_add_client	(ECalDataModel *data_model,
						 ECalClient *client);
void		e_cal_data_model_remove_client	(ECalDataModel *data_model,
						 const gchar *uid);
void		e_cal_data_model_remove_all_clients
						(ECalDataModel *data_model);
ECalClient *	e_cal_data_model_ref_client	(ECalDataModel *data_model,
						 const gchar *uid);
GList *		e_cal_data_model_get_clients	(ECalDataModel *data_model);
GSList *	e_cal_data_model_get_components	(ECalDataModel *data_model,
						 time_t in_range_start,
						 time_t in_range_end);

typedef gboolean (* ECalDataModelForeachFunc)	(ECalDataModel *data_model,
						 ECalClient *client,
						 const ECalComponentId *id,
						 ECalComponent *comp,
						 time_t instance_start,
						 time_t instance_end,
						 gpointer user_data);

gboolean	e_cal_data_model_foreach_component
						(ECalDataModel *data_model,
						 time_t in_range_start,
						 time_t in_range_end,
						 ECalDataModelForeachFunc func,
						 gpointer user_data);

void		e_cal_data_model_subscribe	(ECalDataModel *data_model,
						 ECalDataModelSubscriber *subscriber,
						 time_t range_start,
						 time_t range_end);
void		e_cal_data_model_unsubscribe	(ECalDataModel *data_model,
						 ECalDataModelSubscriber *subscriber);
gboolean	e_cal_data_model_get_subscriber_range
						(ECalDataModel *data_model,
						 ECalDataModelSubscriber *subscriber,
						 time_t *range_start,
						 time_t *range_end);
void		e_cal_data_model_freeze_views_update
						(ECalDataModel *data_model);
void		e_cal_data_model_thaw_views_update
						(ECalDataModel *data_model);
gboolean	e_cal_data_model_is_views_update_frozen
						(ECalDataModel *data_model);

G_END_DECLS

#endif /* E_CAL_DATA_MODEL_H */
