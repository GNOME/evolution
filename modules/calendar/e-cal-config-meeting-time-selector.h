/*
 * e-cal-config-meeting-time-selector.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_CAL_CONFIG_MEETING_TIME_SELECTOR_H
#define E_CAL_CONFIG_MEETING_TIME_SELECTOR_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_CAL_CONFIG_MEETING_TIME_SELECTOR \
	(e_cal_config_meeting_time_selector_get_type ())
#define E_CAL_CONFIG_MEETING_TIME_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_CONFIG_MEETING_TIME_SELECTOR, ECalConfigMeetingTimeSelector))
#define E_CAL_CONFIG_MEETING_TIME_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_CONFIG_MEETING_TIME_SELECTOR, ECalConfigMeetingTimeSelectorClass))
#define E_IS_CAL_CONFIG_MEETING_TIME_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_CONFIG_MEETING_TIME_SELECTOR))
#define E_IS_CAL_CONFIG_MEETING_TIME_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_CONFIG_MEETING_TIME_SELECTOR))
#define E_CAL_CONFIG_MEETING_TIME_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_CONFIG_MEETING_TIME_SELECTOR, ECalConfigMeetingTimeSelectorClass))

G_BEGIN_DECLS

typedef struct _ECalConfigMeetingTimeSelector ECalConfigMeetingTimeSelector;
typedef struct _ECalConfigMeetingTimeSelectorClass ECalConfigMeetingTimeSelectorClass;
typedef struct _ECalConfigMeetingTimeSelectorPrivate ECalConfigMeetingTimeSelectorPrivate;

struct _ECalConfigMeetingTimeSelector {
	EExtension parent;
	ECalConfigMeetingTimeSelectorPrivate *priv;
};

struct _ECalConfigMeetingTimeSelectorClass {
	EExtensionClass parent_class;
};

GType		e_cal_config_meeting_time_selector_get_type
						(void) G_GNUC_CONST;
void		e_cal_config_meeting_time_selector_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_CAL_CONFIG_MEETING_TIME_SELECTOR_H */

