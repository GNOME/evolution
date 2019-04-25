/*
 * Evolution calendar - Timezone selector dialog
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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-send-options-utils.h"

#include <stdlib.h>
#include <string.h>

void
e_send_options_utils_set_default_data (ESendOptionsDialog *sod,
                                       ESource *source,
                                       const gchar *type)
{
	ESendOptionsGeneral *gopts = NULL;
	ESendOptionsStatusTracking *sopts;
	ESourceExtension *extension;
	const gchar *extension_name;
	gchar *value;

	/* FIXME These is all GroupWise-specific settings.
	 *       They absolutely do not belong here. */

	extension_name = "GroupWise Backend";

	if (!e_source_has_extension (source, extension_name))
		return;

	extension = e_source_get_extension (source, extension_name);

	gopts = sod->data->gopts;
	sopts = sod->data->sopts;

		/* priority */
	g_object_get (extension, "priority", &value, NULL);
	if (value) {
		if (!strcmp (value, "high"))
			gopts->priority = E_PRIORITY_HIGH;
		else if (!strcmp (value, "standard"))
			gopts->priority = E_PRIORITY_STANDARD;
		else if (!strcmp (value, "low"))
			gopts->priority = E_PRIORITY_LOW;
		else
			gopts->priority = E_PRIORITY_UNDEFINED;
	}
	g_free (value);

		/* Reply requested */
	g_object_get (extension, "reply-requested", &value, NULL);
	if (value) {
		if (!strcmp (value, "none"))
			gopts->reply_enabled = FALSE;
		else if (!strcmp (value, "convinient")) {
			gopts->reply_enabled = TRUE;
			gopts->reply_convenient = TRUE;
		} else {
			gint i = atoi (value);
			gopts->reply_within = i;
		}
	}
	g_free (value);

		/* Delay delivery */
	g_object_get (extension, "delivery-delay", &value, NULL);
	if (value) {
		if (!strcmp (value, "none"))
			gopts->delay_enabled = FALSE;
		else {
			ICalTime *itt;

			itt = i_cal_time_from_string (value);

			gopts->delay_enabled = TRUE;
			gopts->delay_until = i_cal_time_as_timet (itt);

			g_clear_object (&itt);
		}
	}
	g_free (value);

		/* Expiration Date */
	g_object_get (extension, "expiration", &value, NULL);
	if (value) {
		if (!strcmp (value, "none"))
			gopts->expiration_enabled = FALSE;
		else {
			gint i = atoi (value);
			if (i == 0)
				gopts->expiration_enabled = FALSE;
			else
				gopts->expiration_enabled = TRUE;
			gopts->expire_after = i;
		}
	}
	g_free (value);

		/* status tracking */
	g_object_get (extension, "status-tracking", &value, NULL);
	if (value) {
		if (!strcmp (value, "none"))
			sopts->tracking_enabled = FALSE;
		else {
			sopts->tracking_enabled = TRUE;
			if (!strcmp (value, "delivered"))
				sopts->track_when = E_DELIVERED;
			else if (!strcmp (value, "delivered-opened"))
				sopts->track_when = E_DELIVERED_OPENED;
			else
				sopts->track_when = E_ALL;
		}
	}
	g_free (value);

		/* Return Notifications */

	g_object_get (extension, "return-open", &value, NULL);
	if (value) {
		if (!strcmp (value, "none"))
			sopts->opened = E_RETURN_NOTIFY_NONE;
		else
			sopts->opened = E_RETURN_NOTIFY_MAIL;
	}
	g_free (value);

	g_object_get (extension, "return-accept", &value, NULL);
	if (value) {
		if (!strcmp (value, "none"))
			sopts->accepted = E_RETURN_NOTIFY_NONE;
		else
			sopts->accepted = E_RETURN_NOTIFY_MAIL;
	}
	g_free (value);

	g_object_get (extension, "return-decline", &value, NULL);
	if (value) {
		if (!strcmp (value, "none"))
			sopts->declined = E_RETURN_NOTIFY_NONE;
		else
			sopts->declined = E_RETURN_NOTIFY_MAIL;
	}
	g_free (value);

	g_object_get (extension, "return-complete", &value, NULL);
	if (value) {
		if (!strcmp (value, "none"))
			sopts->completed = E_RETURN_NOTIFY_NONE;
		else
			sopts->completed = E_RETURN_NOTIFY_MAIL;
	}
	g_free (value);
}

static ICalProperty *
esnd_opts_new_property_take_value (gchar *value)
{
	ICalProperty *prop;

	prop = i_cal_property_new_x (value);

	g_free (value);

	return prop;
}

void
e_send_options_utils_fill_component (ESendOptionsDialog *sod,
				     ECalComponent *comp,
				     ICalTimezone *zone)
{
	ICalProperty *prop;
	ICalComponent *icomp;
	ESendOptionsGeneral *gopts;
	ESendOptionsStatusTracking *sopts;

	gopts = sod->data->gopts;
	sopts = sod->data->sopts;

	icomp = e_cal_component_get_icalcomponent (comp);

	if (e_send_options_get_need_general_options (sod)) {
		prop = esnd_opts_new_property_take_value (g_strdup_printf ("%d", gopts->priority));
		i_cal_property_set_x_name (prop, "X-EVOLUTION-OPTIONS-PRIORITY");
		i_cal_component_take_property (icomp, prop);

		if (gopts->reply_enabled) {
			if (gopts->reply_convenient)
				prop = i_cal_property_new_x ("convenient");
			else
				prop = esnd_opts_new_property_take_value (g_strdup_printf ("%d", gopts->reply_within));
			i_cal_property_set_x_name (prop, "X-EVOLUTION-OPTIONS-REPLY");
			i_cal_component_take_property (icomp, prop);
		}

		if (gopts->expiration_enabled && gopts->expire_after) {
			prop = esnd_opts_new_property_take_value (g_strdup_printf ("%d", gopts->expire_after));
			i_cal_property_set_x_name (prop, "X-EVOLUTION-OPTIONS-EXPIRE");
			i_cal_component_take_property (icomp, prop);
		}

		if (gopts->delay_enabled) {
			ICalTime *temp;
			gchar *str;

			temp = i_cal_time_from_timet_with_zone (gopts->delay_until, FALSE, zone);

			str = i_cal_time_as_ical_string_r (temp);
			prop = i_cal_property_new_x (str);
			g_free (str);
			i_cal_property_set_x_name (prop, "X-EVOLUTION-OPTIONS-DELAY");
			i_cal_component_take_property (icomp, prop);

			g_clear_object (&temp);
		}
	}

	if (sopts->tracking_enabled)
		prop = esnd_opts_new_property_take_value (g_strdup_printf ("%d", sopts->track_when));
	else
		prop = i_cal_property_new_x ("0");

	i_cal_property_set_x_name (prop, "X-EVOLUTION-OPTIONS-TRACKINFO");
	i_cal_component_take_property (icomp, prop);

	prop = esnd_opts_new_property_take_value (g_strdup_printf ("%d", sopts->opened));
	i_cal_property_set_x_name (prop, "X-EVOLUTION-OPTIONS-OPENED");
	i_cal_component_take_property (icomp, prop);

	prop = esnd_opts_new_property_take_value (g_strdup_printf ("%d", sopts->accepted));
	i_cal_property_set_x_name (prop, "X-EVOLUTION-OPTIONS-ACCEPTED");
	i_cal_component_take_property (icomp, prop);

	prop = esnd_opts_new_property_take_value (g_strdup_printf ("%d", sopts->declined));
	i_cal_property_set_x_name (prop, "X-EVOLUTION-OPTIONS-DECLINED");
	i_cal_component_take_property (icomp, prop);

	prop = esnd_opts_new_property_take_value (g_strdup_printf ("%d", sopts->completed));
	i_cal_property_set_x_name (prop, "X-EVOLUTION-OPTIONS-COMPLETED");
	i_cal_component_take_property (icomp, prop);
}
