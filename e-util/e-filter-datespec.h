/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_FILTER_DATESPEC_H
#define E_FILTER_DATESPEC_H

#include <time.h>
#include <e-util/e-filter-element.h>

/* Standard GObject types */
#define E_TYPE_FILTER_DATESPEC \
	(e_filter_datespec_get_type ())
#define E_FILTER_DATESPEC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_DATESPEC, EFilterDatespec))
#define E_FILTER_DATESPEC_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_DATESPEC, EFilterDatespecClass))
#define E_IS_FILTER_DATESPEC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_DATESPEC))
#define E_IS_FILTER_DATESPEC_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_DATESPEC))
#define E_FILTER_DATESPEC_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_DATESPEC, EFilterDatespecClass))

G_BEGIN_DECLS

typedef struct _EFilterDatespec EFilterDatespec;
typedef struct _EFilterDatespecClass EFilterDatespecClass;
typedef struct _EFilterDatespecPrivate EFilterDatespecPrivate;

typedef enum {
	FDST_UNKNOWN = -1,
	FDST_NOW,
	FDST_SPECIFIED,
	FDST_X_AGO,
	FDST_X_FUTURE
} EFilterDatespecType;

struct _EFilterDatespec {
	EFilterElement parent;
	EFilterDatespecPrivate *priv;

	EFilterDatespecType type;

	/* either a timespan, an absolute time, or 0
	 * depending on type -- the above mapping to
	 * (X_FUTURE, X_AGO, SPECIFIED, NOW)
	 */

	time_t value;
};

struct _EFilterDatespecClass {
	EFilterElementClass parent_class;
};

GType		e_filter_datespec_get_type	(void) G_GNUC_CONST;
EFilterDatespec *
		e_filter_datespec_new		(void);

G_END_DECLS

#endif /* E_FILTER_DATESPEC_H */
