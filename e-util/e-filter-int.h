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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_FILTER_INT_H
#define E_FILTER_INT_H

#include <e-util/e-filter-element.h>

/* Standard GObject macros */
#define E_TYPE_FILTER_INT \
	(e_filter_int_get_type ())
#define E_FILTER_INT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_INT, EFilterInt))
#define E_FILTER_INT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_INT, EFilterIntClass))
#define E_IS_FILTER_INT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_INT))
#define E_IS_FILTER_INT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_INT))
#define E_FILTER_INT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_INT, EFilterIntClass))

G_BEGIN_DECLS

typedef struct _EFilterInt EFilterInt;
typedef struct _EFilterIntClass EFilterIntClass;
typedef struct _EFilterIntPrivate EFilterIntPrivate;

struct _EFilterInt {
	EFilterElement parent;
	EFilterIntPrivate *priv;

	gchar *type;
	gint val;
	gint min;
	gint max;
};

struct _EFilterIntClass {
	EFilterElementClass parent_class;
};

GType		e_filter_int_get_type		(void) G_GNUC_CONST;
EFilterElement *e_filter_int_new		(void);
EFilterElement *e_filter_int_new_type		(const gchar *type,
						 gint min,
						 gint max);
void		e_filter_int_set_value		(EFilterInt *f_int,
						 gint value);

G_END_DECLS

#endif /* E_FILTER_INT_H */
