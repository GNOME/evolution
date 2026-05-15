/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Not Zed <notzed@lostzed.mmc.com.au>
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
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
