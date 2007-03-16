/* Evolution calendar - Timezone selector dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Damon Chaplin <damon@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "e-send-options-utils.h"
#include "../calendar-config.h"
#include <glib.h>
#include <string.h>

void 
e_sendoptions_utils_set_default_data (ESendOptionsDialog *sod, ESource *source, char * type) 
{
	ESendOptionsGeneral *gopts = NULL;
	ESendOptionsStatusTracking *sopts;
	GConfClient *gconf = gconf_client_get_default ();
	ESourceList *source_list;
	const char *uid;
	const char *value;
	
	gopts = sod->data->gopts;
	sopts = sod->data->sopts;
	
	if (!strcmp (type, "calendar"))
		source_list = e_source_list_new_for_gconf (gconf, "/apps/evolution/calendar/sources");
	else 
		source_list = e_source_list_new_for_gconf (gconf, "/apps/evolution/tasks/sources");
	
	uid = e_source_peek_uid (source);
	source = e_source_list_peek_source_by_uid (source_list, uid);

		/* priority */
	value = e_source_get_property (source, "priority");	
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
		/* Reply requested */
	value = e_source_get_property (source, "reply-requested");	
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
		/* Delay delivery */
	value = e_source_get_property (source, "delay-delivery");	
	if (value) {
		if (!strcmp (value, "none"))
			gopts->delay_enabled = FALSE;
		else {
			gopts->delay_enabled = TRUE;
			gopts->delay_until = icaltime_as_timet (icaltime_from_string (value));
		}
	}	
		/* Expiration Date */
	value = e_source_get_property (source, "expiration");	
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
		/* status tracking */
	value = e_source_get_property (source, "status-tracking");	
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

		/* Return Notifications */
	
	value = e_source_get_property (source, "return-open");	
	if (value) {
		if (!strcmp (value, "none"))
			sopts->opened = E_RETURN_NOTIFY_NONE;
		else 
			sopts->opened = E_RETURN_NOTIFY_MAIL;
	}
	
	value = e_source_get_property (source, "return-accept");	
	if (value) {
		if (!strcmp (value, "none"))
			sopts->accepted = E_RETURN_NOTIFY_NONE;
		else 
			sopts->accepted = E_RETURN_NOTIFY_MAIL;
	}

 	value = e_source_get_property (source, "return-decline");	
	if (value) {
		if (!strcmp (value, "none"))
			sopts->declined = E_RETURN_NOTIFY_NONE;
		else 
			sopts->declined = E_RETURN_NOTIFY_MAIL;
	}
	
	value = e_source_get_property (source, "return-complete");	
	if (value) {
		if (!strcmp (value, "none"))
			sopts->completed = E_RETURN_NOTIFY_NONE;
		else 
			sopts->completed = E_RETURN_NOTIFY_MAIL;
	}

}

void 
e_sendoptions_utils_fill_component (ESendOptionsDialog *sod, ECalComponent *comp) 
{
	int i = 1;
	icalproperty *prop;
	icalcomponent *icalcomp;
	ESendOptionsGeneral *gopts;
	ESendOptionsStatusTracking *sopts;

	gopts = sod->data->gopts;
	sopts = sod->data->sopts;

	e_cal_component_set_sequence (comp, &i);
	icalcomp = e_cal_component_get_icalcomponent (comp);

	if (e_sendoptions_get_need_general_options (sod)) {
		prop = icalproperty_new_x ((const char *) g_strdup_printf ("%d", gopts->priority));
		icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-PRIORITY");
		icalcomponent_add_property (icalcomp, prop);	

		if (gopts->reply_enabled) {
			if (gopts->reply_convenient) 
				prop = icalproperty_new_x ("convenient");	
			else 
				prop = icalproperty_new_x ((const char *) g_strdup_printf ( "%d", gopts->reply_within));
			icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-REPLY");
			icalcomponent_add_property (icalcomp, prop);
		}

		if (gopts->expiration_enabled && gopts->expire_after) {
			prop = icalproperty_new_x ((const char *) g_strdup_printf ( "%d", gopts->expire_after));	
			icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-EXPIRE");
			icalcomponent_add_property (icalcomp, prop);
		}

		if (gopts->delay_enabled) {
			struct icaltimetype temp;
			icaltimezone *zone = calendar_config_get_icaltimezone ();
			temp = icaltime_from_timet_with_zone (gopts->delay_until, FALSE, zone);	
			prop = icalproperty_new_x (icaltime_as_ical_string (temp));
			icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-DELAY");
			icalcomponent_add_property (icalcomp, prop);
		}
	}
	
	if (sopts->tracking_enabled) 
		prop = icalproperty_new_x ((const char *) g_strdup_printf ( "%d", sopts->track_when));	
	else
		prop = icalproperty_new_x ("0");

	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-TRACKINFO");
	icalcomponent_add_property (icalcomp, prop);
		
	
	prop = icalproperty_new_x ((const char *) g_strdup_printf ("%d", sopts->opened));	
	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-OPENED");
	icalcomponent_add_property (icalcomp, prop);
	
	prop = icalproperty_new_x ((const char *) g_strdup_printf ("%d", sopts->accepted));	
	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-ACCEPTED");
	icalcomponent_add_property (icalcomp, prop);

	prop = icalproperty_new_x ((const char *) g_strdup_printf ("%d", sopts->declined));	
	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-DECLINED");
	icalcomponent_add_property (icalcomp, prop);

	prop = icalproperty_new_x ((const char *) g_strdup_printf ("%d", sopts->completed));	
	icalproperty_set_x_name (prop, "X-EVOLUTION-OPTIONS-COMPLETED");
	icalcomponent_add_property (icalcomp, prop);
}

