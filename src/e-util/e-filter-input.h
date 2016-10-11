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

#ifndef E_FILTER_INPUT_H
#define E_FILTER_INPUT_H

#include <e-util/e-filter-element.h>

/* Standard GObject macros */
#define E_TYPE_FILTER_INPUT \
	(e_filter_input_get_type ())
#define E_FILTER_INPUT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_INPUT, EFilterInput))
#define E_FILTER_INPUT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_INPUT, EFilterInputClass))
#define E_IS_FILTER_INPUT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_INPUT))
#define E_IS_FILTER_INPUT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_INPUT))
#define E_FILTER_INPUT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_INPUT, EFilterInputClass))

G_BEGIN_DECLS

typedef struct _EFilterInput EFilterInput;
typedef struct _EFilterInputClass EFilterInputClass;
typedef struct _EFilterInputPrivate EFilterInputPrivate;

struct _EFilterInput {
	EFilterElement parent;
	EFilterInputPrivate *priv;

	gchar *type;		/* name of type */
	GList *values;		/* strings */
	gboolean allow_empty;	/* whether can have empty value */
	gchar *code_gen_func;   /* function name to build the 'code' */
};

struct _EFilterInputClass {
	EFilterElementClass parent_class;
};

GType		e_filter_input_get_type		(void) G_GNUC_CONST;
EFilterInput *	e_filter_input_new		(void);
EFilterInput *	e_filter_input_new_type_name	(const gchar *type);
void		e_filter_input_set_value	(EFilterInput *input,
						 const gchar *value);

G_END_DECLS

#endif /* E_FILTER_INPUT_H */
