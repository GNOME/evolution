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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
			gopts->delay_enabled = TRUE;
			gopts->delay_until = icaltime_as_timet (icaltime_from_string (value));
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

void
e_send_options_utils_fill_component (ESendOptionsDialog *sod,
                                     ECalComponent *comp,
                                     icaltimezone *zone)
{
	gint i = 1;
	icalproperty *prop;
	icalcomponent *icalcomp;
	ESendOptionsGeneral *gopts;
	ESendOptionsStatusTracking *sopts;

	gopts = sod->data->gopts;
	sopts = sod->data->sopts;

	e_cal_component_set_sequence (comp, &i);
	icalcomp = e_cal_component_get_icalcomponent (comp);

	if (e_send_options_get_need_general_options (sod)) {
		prop = icalproperty_new_x ((const gchar *) g_strdup_printf ("%d", gopts->priority));
		icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-PRIORITY");
		icalcomponent_add_property (icalcomp, prop);

		if (gopts->reply_enabled) {
			if (gopts->reply_convenient)
				prop = icalproperty_new_x ("convenient");
			else
				prop = icalproperty_new_x ((const gchar *) g_strdup_printf ("%d", gopts->reply_within));
			icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-REPLY");
			icalcomponent_add_property (icalcomp, prop);
		}

		if (gopts->expiration_enabled && gopts->expire_after) {
			prop = icalproperty_new_x ((const gchar *) g_strdup_printf ("%d", gopts->expire_after));
			icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-EXPIRE");
			icalcomponent_add_property (icalcomp, prop);
		}

		if (gopts->delay_enabled) {
			struct icaltimetype temp;
			gchar *str;

			temp = icaltime_from_timet_with_zone (gopts->delay_until, FALSE, zone);

			str = icaltime_as_ical_string_r (temp);
			prop = icalproperty_new_x (str);
			g_free (str);
			icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-DELAY");
			icalcomponent_add_property (icalcomp, prop);
		}
	}

	if (sopts->tracking_enabled)
		prop = icalproperty_new_x ((const gchar *) g_strdup_printf ("%d", sopts->track_when));
	else
		prop = icalproperty_new_x ("0");

	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-TRACKINFO");
	icalcomponent_add_property (icalcomp, prop);

	prop = icalproperty_new_x ((const gchar *) g_strdup_printf ("%d", sopts->opened));
	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-OPENED");
	icalcomponent_add_property (icalcomp, prop);

	prop = icalproperty_new_x ((const gchar *) g_strdup_printf ("%d", sopts->accepted));
	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-ACCEPTED");
	icalcomponent_add_property (icalcomp, prop);

	prop = icalproperty_new_x ((const gchar *) g_strdup_printf ("%d", sopts->declined));
	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-DECLINED");
	icalcomponent_add_property (icalcomp, prop);

	prop = icalproperty_new_x ((const gchar *) g_strdup_printf ("%d", sopts->completed));
	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-COMPLETED");
	icalcomponent_add_property (icalcomp, prop);
}
