/*
 * e-util-enums.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UTIL_ENUMS_H
#define E_UTIL_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	E_ACTIVITY_RUNNING,
	E_ACTIVITY_WAITING,
	E_ACTIVITY_CANCELLED,
	E_ACTIVITY_COMPLETED
} EActivityState;

typedef enum {
	E_DURATION_MINUTES,
	E_DURATION_HOURS,
	E_DURATION_DAYS
} EDurationType;

G_END_DECLS

#endif /* E_UTIL_ENUMS_H */
