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

#ifndef E_CAL_DATA_MODEL_SUBSCRIBER_H
#define E_CAL_DATA_MODEL_SUBSCRIBER_H

#include <libecal/libecal.h>

/* Standard GObject macros */
#define E_TYPE_CAL_DATA_MODEL_SUBSCRIBER \
	(e_cal_data_model_subscriber_get_type ())
#define E_CAL_DATA_MODEL_SUBSCRIBER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, ECalDataModelSubscriber))
#define E_CAL_DATA_MODEL_SUBSCRIBER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, ECalDataModelSubscriberInterface))
#define E_IS_CAL_DATA_MODEL_SUBSCRIBER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_DATA_MODEL_SUBSCRIBER))
#define E_IS_CAL_DATA_MODEL_SUBSCRIBER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_DATA_MODEL_SUBSCRIBER))
#define E_CAL_DATA_MODEL_SUBSCRIBER_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_CAL_DATA_MODEL_SUBSCRIBER, ECalDataModelSubscriberInterface))

G_BEGIN_DECLS

typedef struct _ECalDataModelSubscriber ECalDataModelSubscriber;
typedef struct _ECalDataModelSubscriberInterface ECalDataModelSubscriberInterface;

struct _ECalDataModelSubscriberInterface {
	GTypeInterface parent_interface;

	void	(*component_added)	(ECalDataModelSubscriber *subscriber,
					 ECalClient *client,
					 ECalComponent *comp);
	void	(*component_modified)	(ECalDataModelSubscriber *subscriber,
					 ECalClient *client,
					 ECalComponent *comp);
	void	(*component_removed)	(ECalDataModelSubscriber *subscriber,
					 ECalClient *client,
					 const gchar *uid,
					 const gchar *rid);
	void	(*freeze)		(ECalDataModelSubscriber *subscriber);
	void	(*thaw)			(ECalDataModelSubscriber *subscriber);
};

GType		e_cal_data_model_subscriber_get_type		(void) G_GNUC_CONST;
void		e_cal_data_model_subscriber_component_added	(ECalDataModelSubscriber *subscriber,
								 ECalClient *client,
								 ECalComponent *comp);
void		e_cal_data_model_subscriber_component_modified	(ECalDataModelSubscriber *subscriber,
								 ECalClient *client,
								 ECalComponent *comp);
void		e_cal_data_model_subscriber_component_removed	(ECalDataModelSubscriber *subscriber,
								 ECalClient *client,
								 const gchar *uid,
								 const gchar *rid);
void		e_cal_data_model_subscriber_freeze		(ECalDataModelSubscriber *subscriber);
void		e_cal_data_model_subscriber_thaw		(ECalDataModelSubscriber *subscriber);

G_END_DECLS

#endif /* E_CAL_DATA_MODEL_SUBSCRIBER_H */
