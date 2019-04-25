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

#include "evolution-config.h"

#include "e-cal-data-model-subscriber.h"

G_DEFINE_INTERFACE (ECalDataModelSubscriber, e_cal_data_model_subscriber, G_TYPE_OBJECT)

static void
e_cal_data_model_subscriber_default_init (ECalDataModelSubscriberInterface *iface)
{
}

/**
 * e_cal_data_model_subscriber_component_added:
 * @subscriber: an #ECalDataModelSubscriber
 * @client: an #ECalClient, which notifies about the component addition
 * @comp: an #ECalComponent which was added
 *
 * Notifies the @subscriber about an added component which belongs
 * to the time range used by the @subscriber.
 *
 * Note: The @subscriber can be frozen during these calls, to be able
 *    to cumulate multiple changes and propagate them at once.
 **/
void
e_cal_data_model_subscriber_component_added (ECalDataModelSubscriber *subscriber,
					     ECalClient *client,
					     ECalComponent *comp)
{
	ECalDataModelSubscriberInterface *iface;

	g_return_if_fail (E_IS_CAL_DATA_MODEL_SUBSCRIBER (subscriber));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	iface = E_CAL_DATA_MODEL_SUBSCRIBER_GET_INTERFACE (subscriber);
	g_return_if_fail (iface->component_added != NULL);

	iface->component_added (subscriber, client, comp);
}

/**
 * e_cal_data_model_subscriber_component_modified:
 * @subscriber: an #ECalDataModelSubscriber
 * @client: an #ECalClient, which notifies about the component modification
 * @comp: an #ECalComponent which was modified
 *
 * Notifies the @subscriber about a modified component which belongs
 * to the time range used by the @subscriber.
 *
 * Note: The @subscriber can be frozen during these calls, to be able
 *    to cumulate multiple changes and propagate them at once.
 **/
void
e_cal_data_model_subscriber_component_modified (ECalDataModelSubscriber *subscriber,
						ECalClient *client,
						ECalComponent *comp)
{
	ECalDataModelSubscriberInterface *iface;

	g_return_if_fail (E_IS_CAL_DATA_MODEL_SUBSCRIBER (subscriber));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	iface = E_CAL_DATA_MODEL_SUBSCRIBER_GET_INTERFACE (subscriber);
	g_return_if_fail (iface->component_modified != NULL);

	iface->component_modified (subscriber, client, comp);
}

/**
 * e_cal_data_model_subscriber_component_removed:
 * @subscriber: an #ECalDataModelSubscriber
 * @client: an #ECalClient, which notifies about the component removal
 * @uid: UID of a removed component
 * @rid: RID of a removed component
 *
 * Notifies the @subscriber about a removed component identified
 * by @uid and @rid. This component may or may not be within
 * the time range specified by the @subscriber.
 *
 * Note: The @subscriber can be frozen during these calls, to be able
 *    to cumulate multiple changes and propagate them at once.
 **/
void
e_cal_data_model_subscriber_component_removed (ECalDataModelSubscriber *subscriber,
					       ECalClient *client,
					       const gchar *uid,
					       const gchar *rid)
{
	ECalDataModelSubscriberInterface *iface;

	g_return_if_fail (E_IS_CAL_DATA_MODEL_SUBSCRIBER (subscriber));

	iface = E_CAL_DATA_MODEL_SUBSCRIBER_GET_INTERFACE (subscriber);
	g_return_if_fail (iface->component_removed != NULL);

	iface->component_removed (subscriber, client, uid, rid);
}

/**
 * e_cal_data_model_subscriber_freeze:
 * @subscriber: an #ECalDataModelSubscriber
 *
 * Tells the @subscriber that it'll be notified about multiple
 * changes. Once all the notifications are done,
 * a e_cal_data_model_subscriber_thaw() is called.
 *
 * Note: This function can be called multiple times/recursively, with
 *   the same count of the e_cal_data_model_subscriber_thaw(), thus
 *   count with it.
 **/
void
e_cal_data_model_subscriber_freeze (ECalDataModelSubscriber *subscriber)
{
	ECalDataModelSubscriberInterface *iface;

	g_return_if_fail (E_IS_CAL_DATA_MODEL_SUBSCRIBER (subscriber));

	iface = E_CAL_DATA_MODEL_SUBSCRIBER_GET_INTERFACE (subscriber);
	g_return_if_fail (iface->freeze != NULL);

	iface->freeze (subscriber);
}

/**
 * e_cal_data_model_subscriber_thaw:
 * @subscriber: an #ECalDataModelSubscriber
 *
 * A pair function for e_cal_data_model_subscriber_freeze(), which notifies
 * about the end of a content update.
 **/
void
e_cal_data_model_subscriber_thaw (ECalDataModelSubscriber *subscriber)
{
	ECalDataModelSubscriberInterface *iface;

	g_return_if_fail (E_IS_CAL_DATA_MODEL_SUBSCRIBER (subscriber));

	iface = E_CAL_DATA_MODEL_SUBSCRIBER_GET_INTERFACE (subscriber);
	g_return_if_fail (iface->thaw != NULL);

	iface->thaw (subscriber);
}
