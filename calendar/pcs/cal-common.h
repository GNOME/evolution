/* Evolution calendar server - common declarations
 *
 * Copyright (C) 2000 Ximian, Inc.
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

#ifndef CAL_COMMON_H
#define CAL_COMMON_H

#include <glib/gmacros.h>

G_BEGIN_DECLS



typedef struct _CalBackend CalBackend;
typedef struct _CalBackendClass CalBackendClass;

typedef struct _Cal Cal;
typedef struct _CalClass CalClass;

typedef struct _Query Query;
typedef struct _QueryClass QueryClass;

typedef struct _CalBackendObjectSExp CalBackendObjectSExp;
typedef struct _CalBackendObjectSExpClass CalBackendObjectSExpClass;



G_END_DECLS

#endif
