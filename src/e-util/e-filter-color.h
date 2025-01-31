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

#ifndef E_FILTER_COLOR_H
#define E_FILTER_COLOR_H

#include <e-util/e-filter-element.h>

/* Standard GObject macros */
#define E_TYPE_FILTER_COLOR \
	(e_filter_color_get_type ())
#define E_FILTER_COLOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_COLOR, EFilterColor))
#define E_FILTER_COLOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_COLOR, EFilterColorClass))
#define E_IS_FILTER_COLOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_COLOR))
#define E_IS_FILTER_COLOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_COLOR))
#define E_FILTER_COLOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_COLOR, EFilterColorClass))

G_BEGIN_DECLS

typedef struct _EFilterColor EFilterColor;
typedef struct _EFilterColorClass EFilterColorClass;
typedef struct _EFilterColorPrivate EFilterColorPrivate;

struct _EFilterColor {
	EFilterElement parent;
	EFilterColorPrivate *priv;

	GdkRGBA color;
};

struct _EFilterColorClass {
	EFilterElementClass parent_class;
};

GType		e_filter_color_get_type		(void) G_GNUC_CONST;
EFilterColor *	e_filter_color_new		(void);

G_END_DECLS

#endif /* E_FILTER_COLOR_H */
