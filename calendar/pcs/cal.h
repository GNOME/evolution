/* Evolution calendar client interface object
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include <bonobo/bonobo-object.h>
#include "pcs/evolution-calendar.h"
#include "pcs/cal-common.h"

G_BEGIN_DECLS



#define CAL_TYPE            (cal_get_type ())
#define CAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_TYPE, Cal))
#define CAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_TYPE, CalClass))
#define IS_CAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_TYPE))
#define IS_CAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_TYPE))

typedef struct _CalPrivate CalPrivate;

struct _Cal {
	BonoboObject object;

	/* Private data */
	CalPrivate *priv;
};

struct _CalClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Calendar_Cal__epv epv;
};

GType cal_get_type (void);

Cal *cal_construct (Cal *cal,
		    CalBackend *backend,
		    GNOME_Evolution_Calendar_Listener listener);

Cal *cal_new (CalBackend *backend, GNOME_Evolution_Calendar_Listener listener);

void cal_notify_mode (Cal *cal, 
		      GNOME_Evolution_Calendar_Listener_SetModeStatus status, 
		      GNOME_Evolution_Calendar_CalMode mode);
void cal_notify_update (Cal *cal, const char *uid);
void cal_notify_remove (Cal *cal, const char *uid);
void cal_notify_error (Cal *cal, const char *message);

void cal_notify_categories_changed (Cal *cal, GNOME_Evolution_Calendar_StringSeq *categories);



G_END_DECLS

#endif
