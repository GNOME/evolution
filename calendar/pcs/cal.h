/* Evolution calendar client interface object
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
#include <bonobo/bonobo-object.h>
#include "evolution-calendar.h"
#include "cal-common.h"

BEGIN_GNOME_DECLS



#define CAL_TYPE            (cal_get_type ())
#define CAL(obj)            (GTK_CHECK_CAST ((obj), CAL_TYPE, Cal))
#define CAL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_TYPE, CalClass))
#define IS_CAL(obj)         (GTK_CHECK_TYPE ((obj), CAL_TYPE))
#define IS_CAL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_TYPE))

struct _Cal {
	BonoboObject object;

	/* Private data */
	gpointer priv;
};

struct _CalClass {
	BonoboObjectClass parent_class;
};

GtkType cal_get_type (void);

Cal *cal_construct (Cal *cal,
		    Evolution_Calendar_Cal corba_cal,
		    CalBackend *backend,
		    Evolution_Calendar_Listener listener);
Evolution_Calendar_Cal cal_corba_object_create (BonoboObject *object);

Cal *cal_new (CalBackend *backend, Evolution_Calendar_Listener listener);

void cal_notify_update (Cal *cal, const char *uid);

POA_Evolution_Calendar_Cal__epv *cal_get_epv (void);



END_GNOME_DECLS

#endif
