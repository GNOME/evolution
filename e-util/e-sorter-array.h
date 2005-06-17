/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-sorter-array.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_SORTER_ARRAY_H_
#define _E_SORTER_ARRAY_H_

#include <glib.h>
#include <glib-object.h>
#include <e-util/e-sorter.h>

G_BEGIN_DECLS

#define E_SORTER_ARRAY_TYPE        (e_sorter_array_get_type ())
#define E_SORTER_ARRAY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_SORTER_ARRAY_TYPE, ESorterArray))
#define E_SORTER_ARRAY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_SORTER_ARRAY_TYPE, ESorterArrayClass))
#define E_IS_SORTER_ARRAY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_SORTER_ARRAY_TYPE))
#define E_IS_SORTER_ARRAY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_SORTER_ARRAY_TYPE))

#ifndef _E_COMPARE_ROWS_FUNC_H_
#define _E_COMPARE_ROWS_FUNC_H_
typedef int (*ECompareRowsFunc) (int row1,
				 int row2,
				 gpointer closure);
#endif

typedef struct {
	ESorter      base;

	ECompareRowsFunc compare;
	gpointer     closure;

	/* If needs_sorting is 0, then model_to_sorted and sorted_to_model are no-ops. */
	int         *sorted;
	int         *backsorted;

	int rows;
} ESorterArray;

typedef struct {
	ESorterClass parent_class;
} ESorterArrayClass;

GType         e_sorter_array_get_type   (void);
ESorterArray *e_sorter_array_construct  (ESorterArray     *sorter,
					 ECompareRowsFunc  compare,
					 gpointer          closure);
ESorterArray *e_sorter_array_new        (ECompareRowsFunc  compare,
					 gpointer          closure);
void          e_sorter_array_clean      (ESorterArray     *esa);
void          e_sorter_array_set_count  (ESorterArray     *esa,
					 int               count);
void          e_sorter_array_append     (ESorterArray     *esa,
					 int               count);

G_END_DECLS

#endif /* _E_SORTER_ARRAY_H_ */
