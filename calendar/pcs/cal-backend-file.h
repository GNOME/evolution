/* Evolution calendar - iCalendar file backend
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

#ifndef CAL_BACKEND_FILE_H
#define CAL_BACKEND_FILE_H

#include <libgnome/gnome-defs.h>
#include "cal-backend.h"

BEGIN_GNOME_DECLS



#define CAL_BACKEND_FILE_TYPE            (cal_backend_file_get_type ())
#define CAL_BACKEND_FILE(obj)            (GTK_CHECK_CAST ((obj), CAL_BACKEND_FILE_TYPE,		\
					  CalBackendFile))
#define CAL_BACKEND_FILE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_BACKEND_FILE_TYPE,	\
					  CalBackendFileClass))
#define IS_CAL_BACKEND_FILE(obj)         (GTK_CHECK_TYPE ((obj), CAL_BACKEND_FILE_TYPE))
#define IS_CAL_BACKEND_FILE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_BACKEND_FILE_TYPE))

typedef struct _CalBackendFile CalBackendFile;
typedef struct _CalBackendFileClass CalBackendFileClass;

typedef struct _CalBackendFilePrivate CalBackendFilePrivate;

struct _CalBackendFile {
	CalBackend backend;

	/* Private data */
	CalBackendFilePrivate *priv;
};

struct _CalBackendFileClass {
	CalBackendClass parent_class;
};

GtkType cal_backend_file_get_type (void);



END_GNOME_DECLS

#endif
