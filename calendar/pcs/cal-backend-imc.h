/* Evolution calendar - Internet Mail Consortium formats backend
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@helixcode.com>
 *          Seth Alves <alves@helixcode.com>
 *          Miguel de Icaza <miguel@helixcode.com>
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

#ifndef CAL_BACKEND_IMC_H
#define CAL_BACKEND_IMC_H

#include <libgnome/gnome-defs.h>
#include "cal-backend.h"

BEGIN_GNOME_DECLS



#define CAL_BACKEND_IMC_TYPE            (cal_backend_imc_get_type ())
#define CAL_BACKEND_IMC(obj)            (GTK_CHECK_CAST ((obj), CAL_BACKEND_IMC_TYPE, CalBackendIMC))
#define CAL_BACKEND_IMC_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_BACKEND_IMC_TYPE,	\
					 CalBackendIMCClass))
#define IS_CAL_BACKEND_IMC(obj)         (GTK_CHECK_TYPE ((obj), CAL_BACKEND_IMC_TYPE))
#define IS_CAL_BACKEND_IMC_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_BACKEND_IMC_TYPE))

typedef struct _CalBackendIMC CalBackendIMC;
typedef struct _CalBackendIMCClass CalBackendIMCClass;

struct _CalBackendIMC {
	CalBackend backend;

	/* Private data */
	gpointer priv;
};

struct _CalBackendIMCClass {
	CalBackendClass parent_class;
};

GtkType cal_backend_imc_get_type (void);



END_GNOME_DECLS

#endif
