/* Evolution calendar factory
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#ifndef CAL_FACTORY_H
#define CAL_FACTORY_H

#include <bonobo/bonobo-object.h>
#include <libical/ical.h>

#include "pcs/evolution-calendar.h"

G_BEGIN_DECLS



#define CAL_FACTORY_TYPE            (cal_factory_get_type ())
#define CAL_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_FACTORY_TYPE, CalFactory))
#define CAL_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_FACTORY_TYPE,		\
				     CalFactoryClass))
#define IS_CAL_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_FACTORY_TYPE))
#define IS_CAL_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_FACTORY_TYPE))

typedef struct _CalFactory CalFactory;
typedef struct _CalFactoryClass CalFactoryClass;

typedef struct _CalFactoryPrivate CalFactoryPrivate;

struct _CalFactory {
	BonoboObject object;

	/* Private data */
	CalFactoryPrivate *priv;
};

struct _CalFactoryClass {
	BonoboObjectClass parent_class;
	
	POA_GNOME_Evolution_Calendar_CalFactory__epv epv;

	/* Notification signals */
	void (* last_calendar_gone) (CalFactory *factory);
};

GType       cal_factory_get_type        (void);
CalFactory *cal_factory_new             (void);

gboolean    cal_factory_register_storage (CalFactory *factory, const char *iid);
void        cal_factory_register_method  (CalFactory *factory,
					  const char *method,
					  icalcomponent_kind kind,
					  GType       backend_type);
int         cal_factory_get_n_backends   (CalFactory *factory);
void        cal_factory_dump_active_backends   (CalFactory *factory);

G_END_DECLS

#endif
