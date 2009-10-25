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

#ifndef E_FILTER_OPTION_H
#define E_FILTER_OPTION_H

#include "e-filter-element.h"

/* Standard GObject macros */
#define E_TYPE_FILTER_OPTION \
	(e_filter_option_get_type ())
#define E_FILTER_OPTION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_OPTION, EFilterOption))
#define E_FILTER_OPTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_OPTION, EFilterOptionClass))
#define E_IS_FILTER_OPTION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_OPTION))
#define E_IS_FILTER_OPTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_OPTION))
#define E_FILTER_OPTION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_OPTION, EFilterOptionClass))

G_BEGIN_DECLS

typedef struct _EFilterOption EFilterOption;
typedef struct _EFilterOptionClass EFilterOptionClass;
typedef struct _EFilterOptionPrivate EFilterOptionPrivate;

struct _filter_option {
	gchar *title;		/* button title */
	gchar *value;		/* value, if it has one */
	gchar *code;		/* used to string code segments together */

	gboolean is_dynamic;	/* whether is the option dynamic, FALSE if static */
};

struct _EFilterOption {
	EFilterElement parent;
	EFilterOptionPrivate *priv;

	const gchar *type;	/* static memory, type name written to xml */

	GList *options;
	struct _filter_option *current;
	gchar *dynamic_func;	/* name of the dynamic fill func, called in get_widget */
};

struct _EFilterOptionClass {
	EFilterElementClass parent_class;
};

GType		e_filter_option_get_type	(void);
EFilterOption *	e_filter_option_new		(void);
void		e_filter_option_set_current	(EFilterOption *option,
						 const gchar *name);
const gchar *	e_filter_option_get_current	(EFilterOption *option);
struct _filter_option *
		e_filter_option_add		(EFilterOption *option,
						 const gchar *name,
						 const gchar *title,
						 const gchar *code,
						 gboolean is_dynamic);
void		e_filter_option_remove_all	(EFilterOption *option);

G_END_DECLS

#endif /* E_FILTER_OPTION_H */
