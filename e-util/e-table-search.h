/*
 *
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_SEARCH_H_
#define _E_TABLE_SEARCH_H_

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_SEARCH \
	(e_table_search_get_type ())
#define E_TABLE_SEARCH(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_SEARCH, ETableSearch))
#define E_TABLE_SEARCH_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_SEARCH, ETableSearchClass))
#define E_IS_TABLE_SEARCH(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_SEARCH))
#define E_IS_TABLE_SEARCH_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_SEARCH))
#define E_TABLE_SEARCH_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_SEARCH, ETableSearchClass))

G_BEGIN_DECLS

typedef struct _ETableSearch ETableSearch;
typedef struct _ETableSearchClass ETableSearchClass;
typedef struct _ETableSearchPrivate ETableSearchPrivate;

typedef enum {
	E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST = 1 << 0
} ETableSearchFlags;

struct _ETableSearch {
	GObject parent;
	ETableSearchPrivate *priv;
};

struct _ETableSearchClass {
	GObjectClass parent_class;

	/* Signals */
	gboolean	(*search)		(ETableSearch *ets,
						 gchar *string /* utf8 */,
						 ETableSearchFlags flags);
	void		(*accept)		(ETableSearch *ets);
};

GType		e_table_search_get_type		(void) G_GNUC_CONST;
ETableSearch *	e_table_search_new		(void);
void		e_table_search_input_character	(ETableSearch *e_table_search,
						 gunichar character);
gboolean	e_table_search_backspace	(ETableSearch *e_table_search);
void		e_table_search_cancel		(ETableSearch *e_table_search);

G_END_DECLS

#endif /* _E_TABLE_SEARCH_H_ */
