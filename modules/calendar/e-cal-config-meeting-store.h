/*
 * e-cal-config-meeting-store.h
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

#ifndef E_CAL_CONFIG_MEETING_STORE_H
#define E_CAL_CONFIG_MEETING_STORE_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_CAL_CONFIG_MEETING_STORE \
	(e_cal_config_meeting_store_get_type ())
#define E_CAL_CONFIG_MEETING_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_CONFIG_MEETING_STORE, ECalConfigMeetingStore))
#define E_CAL_CONFIG_MEETING_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_CONFIG_MEETING_STORE, ECalConfigMeetingStoreClass))
#define E_IS_CAL_CONFIG_MEETING_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_CONFIG_MEETING_STORE))
#define E_IS_CAL_CONFIG_MEETING_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_CONFIG_MEETING_STORE))
#define E_CAL_CONFIG_MEETING_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CaL_CONFIG_MEETING_STORE, ECalConfigMeetingStoreClass))

G_BEGIN_DECLS

typedef struct _ECalConfigMeetingStore ECalConfigMeetingStore;
typedef struct _ECalConfigMeetingStoreClass ECalConfigMeetingStoreClass;
typedef struct _ECalConfigMeetingStorePrivate ECalConfigMeetingStorePrivate;

struct _ECalConfigMeetingStore {
	EExtension parent;
	ECalConfigMeetingStorePrivate *priv;
};

struct _ECalConfigMeetingStoreClass {
	EExtensionClass parent_class;
};

GType		e_cal_config_meeting_store_get_type
						(void) G_GNUC_CONST;
void		e_cal_config_meeting_store_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_CAL_CONFIG_MEETING_STORE_H */
