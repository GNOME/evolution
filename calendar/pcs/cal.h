/* Evolution calendar client interface object
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
#include <bonobo/bonobo-xobject.h>
#include "calendar/pcs/evolution-calendar.h"
#include "cal-common.h"

BEGIN_GNOME_DECLS



#define CAL_TYPE            (cal_get_type ())
#define CAL(obj)            (GTK_CHECK_CAST ((obj), CAL_TYPE, Cal))
#define CAL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_TYPE, CalClass))
#define IS_CAL(obj)         (GTK_CHECK_TYPE ((obj), CAL_TYPE))
#define IS_CAL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_TYPE))

typedef struct _CalPrivate CalPrivate;

struct _Cal {
	BonoboXObject object;

	/* Private data */
	CalPrivate *priv;
};

struct _CalClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_Calendar_Cal__epv epv;
};

GtkType cal_get_type (void);

Cal *cal_construct (Cal *cal,
		    CalBackend *backend,
		    GNOME_Evolution_Calendar_Listener listener);

Cal *cal_new (CalBackend *backend, GNOME_Evolution_Calendar_Listener listener);

void cal_notify_update (Cal *cal, const char *uid);
void cal_notify_remove (Cal *cal, const char *uid);



END_GNOME_DECLS

#endif
