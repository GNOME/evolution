/*
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
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _FILTER_DATESPEC_H
#define _FILTER_DATESPEC_H

#include <time.h>
#include "filter-element.h"

#define FILTER_TYPE_DATESPEC            (filter_datespec_get_type ())
#define FILTER_DATESPEC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_DATESPEC, FilterDatespec))
#define FILTER_DATESPEC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_DATESPEC, FilterDatespecClass))
#define IS_FILTER_DATESPEC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_DATESPEC))
#define IS_FILTER_DATESPEC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_DATESPEC))
#define FILTER_DATESPEC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_DATESPEC, FilterDatespecClass))

typedef struct _FilterDatespec FilterDatespec;
typedef struct _FilterDatespecClass FilterDatespecClass;

typedef enum _FilterDatespec_type {
	FDST_UNKNOWN = -1,
	FDST_NOW,
	FDST_SPECIFIED,
	FDST_X_AGO,
	FDST_X_FUTURE
} FilterDatespec_type;

struct _FilterDatespec {
	FilterElement parent;
	struct _FilterDatespecPrivate *priv;

	FilterDatespec_type type;

	/* either a timespan, an absolute time, or 0
	 * depending on type -- the above mapping to
	 * (X_FUTURE, X_AGO, SPECIFIED, NOW)
	 */

	time_t value;
};

struct _FilterDatespecClass {
	FilterElementClass parent_class;

	/* virtual methods */

	/* signals */
};

GType filter_datespec_get_type (void);
FilterDatespec *filter_datespec_new (void);

/* methods */

#endif /* ! _FILTER_DATESPEC_H */
