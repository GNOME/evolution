/* Evolution calendar - iCalendar component object
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CAL_COMPONENT_H
#define CAL_COMPONENT_H

#include <libgnome/gnome-defs.h>
#include <time.h>
#include <gtk/gtkobject.h>
#include <ical.h>

BEGIN_GNOME_DECLS



#define CAL_COMPONENT_TYPE            (cal_component_get_type ())
#define CAL_COMPONENT(obj)            (GTK_CHECK_CAST ((obj), CAL_COMPONENT_TYPE, CalComponent))
#define CAL_COMPONENT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_COMPONENT_TYPE,	\
				       CalComponentClass))
#define IS_CAL_COMPONENT(obj)         (GTK_CHECK_TYPE ((obj), CAL_COMPONENT_TYPE))
#define IS_CAL_COMPONENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_COMPONENT_TYPE))

/* Types of calendar components to be stored by a CalComponent, as per RFC 2445.
 * We don't put the alarm component type here since we store alarms as separate
 * structures inside the other "real" components.
 */
typedef enum {
	CAL_COMPONENT_NO_TYPE,
	CAL_COMPONENT_EVENT,
	CAL_COMPONENT_TODO,
	CAL_COMPONENT_JOURNAL,
	CAL_COMPONENT_FREEBUSY,
	CAL_COMPONENT_TIMEZONE
} CalComponentVType;

typedef struct _CalComponent CalComponent;
typedef struct _CalComponentClass CalComponentClass;

struct _CalComponent {
	GtkObject object;

	/* Private data */
	gpointer priv;
};

struct _CalComponentClass {
	GtkObjectClass parent_class;
};

GtkType cal_component_get_type (void);

char *cal_component_gen_uid (void);

CalComponent *cal_component_new (void);

void cal_component_set_new_vtype (CalComponent *comp, CalComponentVType type);

void cal_component_set_icalcomponent (CalComponent *comp, icalcomponent *icalcomp);
icalcomponent *cal_component_get_icalcomponent (CalComponent *comp);

CalComponentVType cal_component_get_vtype (CalComponent *comp);

const char *cal_component_get_uid (CalComponent *comp);
void cal_component_set_uid (CalComponent *comp, const char *uid);

void cal_component_get_summary (CalComponent *comp, const char **summary, const char **altrep);
void cal_component_set_summary (CalComponent *comp, const char *summary, const char *altrep);



END_GNOME_DECLS

#endif
