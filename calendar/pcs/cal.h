/* GNOME calendar object
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

#ifndef CAL_H
#define CAL_H

#include <libgnome/gnome-defs.h>
#include <bonobo/gnome-object.h>
#include "gnome-calendar.h"

BEGIN_GNOME_DECLS



#define CAL_TYPE            (cal_get_type ())
#define CAL(obj)            (GTK_CHECK_CAST ((obj), CAL_TYPE, Cal))
#define CAL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_TYPE, CalClass))
#define IS_CAL(obj)         (GTK_CHECK_TYPE ((obj), CAL_TYPE))
#define IS_CAL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_TYPE))

typedef struct _Cal Cal;
typedef struct _CalClass CalClass;

struct _Cal {
	GnomeObject object;

	/* Private data */
	gpointer priv;
};

struct _CalClass {
	GnomeObjectClass parent_class;
};

GtkType cal_get_type (void);

Cal *cal_construct (Cal *cal, GNOME_Calendar_Cal corba_cal);
GNOME_Calendar_Cal cal_corba_object_create (GnomeObject *object);

Cal *cal_new (char *uri);
Cal *cal_new_from_file (char *uri);

void cal_add_listener (Cal *cal, GNOME_Calendar_Listener listener);
void cal_remove_listener (Cal *cal, GNOME_Calendar_Listener listener);

POA_GNOME_Calendar_Cal__epv *cal_get_epv (void);



END_GNOME_DECLS

#endif
