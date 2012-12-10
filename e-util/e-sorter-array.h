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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_SORTER_ARRAY_H_
#define _E_SORTER_ARRAY_H_

#include <e-util/e-sorter.h>

G_BEGIN_DECLS

#define E_SORTER_ARRAY_TYPE        (e_sorter_array_get_type ())
#define E_SORTER_ARRAY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_SORTER_ARRAY_TYPE, ESorterArray))
#define E_SORTER_ARRAY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_SORTER_ARRAY_TYPE, ESorterArrayClass))
#define E_IS_SORTER_ARRAY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_SORTER_ARRAY_TYPE))
#define E_IS_SORTER_ARRAY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_SORTER_ARRAY_TYPE))

#ifndef _E_COMPARE_ROWS_FUNC_H_
#define _E_COMPARE_ROWS_FUNC_H_
typedef gint (*ECompareRowsFunc) (gint row1,
				 gint row2,
				 GHashTable *cmp_cache,
				 gpointer closure);
#endif

typedef GHashTable * (*ECreateCmpCacheFunc) (gpointer closure);

typedef struct {
	ESorter      base;

	GHashTable *cmp_cache;
	ECreateCmpCacheFunc create_cmp_cache;
	ECompareRowsFunc compare;
	gpointer     closure;

	/* If needs_sorting is 0, then model_to_sorted and sorted_to_model are no-ops. */
	gint         *sorted;
	gint         *backsorted;

	gint rows;
} ESorterArray;

typedef struct {
	ESorterClass parent_class;
} ESorterArrayClass;

GType         e_sorter_array_get_type   (void);
ESorterArray *e_sorter_array_construct  (ESorterArray     *sorter,
					 ECreateCmpCacheFunc create_cmp_cache,
					 ECompareRowsFunc  compare,
					 gpointer          closure);
ESorterArray *e_sorter_array_new        (ECreateCmpCacheFunc create_cmp_cache,
					 ECompareRowsFunc  compare,
					 gpointer          closure);
void          e_sorter_array_clean      (ESorterArray     *esa);
void          e_sorter_array_set_count  (ESorterArray     *esa,
					 gint               count);
void          e_sorter_array_append     (ESorterArray     *esa,
					 gint               count);

G_END_DECLS

#endif /* _E_SORTER_ARRAY_H_ */
