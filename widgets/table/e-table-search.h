/*
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
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TABLE_SEARCH_H_
#define _E_TABLE_SEARCH_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define E_TABLE_SEARCH_TYPE        (e_table_search_get_type ())
#define E_TABLE_SEARCH(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SEARCH_TYPE, ETableSearch))
#define E_TABLE_SEARCH_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_SEARCH_TYPE, ETableSearchClass))
#define E_IS_TABLE_SEARCH(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SEARCH_TYPE))
#define E_IS_TABLE_SEARCH_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SEARCH_TYPE))
#define E_TABLE_SEARCH_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_SEARCH_TYPE, ETableSearchClass))

typedef struct _ETableSearchPrivate ETableSearchPrivate;

typedef enum {
	E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST = 1 << 0
} ETableSearchFlags;

typedef struct {
	GObject   base;

	ETableSearchPrivate *priv;
} ETableSearch;

typedef struct {
	GObjectClass parent_class;

	/*
	 * Signals
	 */
	gboolean (*search)    (ETableSearch *ets, gchar *string /* utf8 */, ETableSearchFlags flags);
	void     (*accept)    (ETableSearch *ets);
} ETableSearchClass;

GType       e_table_search_get_type         (void);
ETableSearch *e_table_search_new              (void);

/**/
void          e_table_search_input_character  (ETableSearch *e_table_search,
					       gunichar      character);
gboolean      e_table_search_backspace        (ETableSearch *e_table_search);
void          e_table_search_cancel           (ETableSearch *e_table_search);

G_END_DECLS

#endif /* _E_TABLE_SEARCH_H_ */
