/*
 * e-settings-meeting-time-selector.h
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
 */

#ifndef E_SETTINGS_MEETING_TIME_SELECTOR_H
#define E_SETTINGS_MEETING_TIME_SELECTOR_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_MEETING_TIME_SELECTOR \
	(e_settings_meeting_time_selector_get_type ())
#define E_SETTINGS_MEETING_TIME_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_MEETING_TIME_SELECTOR, ESettingsMeetingTimeSelector))
#define E_SETTINGS_MEETING_TIME_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_MEETING_TIME_SELECTOR, ESettingsMeetingTimeSelectorClass))
#define E_IS_SETTINGS_MEETING_TIME_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_MEETING_TIME_SELECTOR))
#define E_IS_SETTINGS_MEETING_TIME_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_MEETING_TIME_SELECTOR))
#define E_SETTINGS_MEETING_TIME_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_MEETING_TIME_SELECTOR, ESettingsMeetingTimeSelectorClass))

G_BEGIN_DECLS

typedef struct _ESettingsMeetingTimeSelector ESettingsMeetingTimeSelector;
typedef struct _ESettingsMeetingTimeSelectorClass ESettingsMeetingTimeSelectorClass;
typedef struct _ESettingsMeetingTimeSelectorPrivate ESettingsMeetingTimeSelectorPrivate;

struct _ESettingsMeetingTimeSelector {
	EExtension parent;
	ESettingsMeetingTimeSelectorPrivate *priv;
};

struct _ESettingsMeetingTimeSelectorClass {
	EExtensionClass parent_class;
};

GType		e_settings_meeting_time_selector_get_type
						(void) G_GNUC_CONST;
void		e_settings_meeting_time_selector_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_MEETING_TIME_SELECTOR_H */

