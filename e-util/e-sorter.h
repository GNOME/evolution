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

#ifndef _E_SORTER_H_
#define _E_SORTER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define E_SORTER_TYPE        (e_sorter_get_type ())
#define E_SORTER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_SORTER_TYPE, ESorter))
#define E_SORTER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_SORTER_TYPE, ESorterClass))
#define E_IS_SORTER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_SORTER_TYPE))
#define E_IS_SORTER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_SORTER_TYPE))
#define E_SORTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), E_SORTER_TYPE, ESorterClass))

typedef struct {
	GObject base;
} ESorter;

typedef struct {
	GObjectClass parent_class;
	gint      (*model_to_sorted)            (ESorter    *sorter,
						 gint         row);
	gint      (*sorted_to_model)            (ESorter    *sorter,
						 gint         row);

	void      (*get_model_to_sorted_array)  (ESorter    *sorter,
						 gint       **array,
						 gint        *count);
	void      (*get_sorted_to_model_array)  (ESorter    *sorter,
						 gint       **array,
						 gint        *count);

	gboolean  (*needs_sorting)              (ESorter    *sorter);
} ESorterClass;

GType     e_sorter_get_type                   (void);
ESorter  *e_sorter_new                        (void);

gint      e_sorter_model_to_sorted            (ESorter  *sorter,
					       gint       row);
gint      e_sorter_sorted_to_model            (ESorter  *sorter,
					       gint       row);

void      e_sorter_get_model_to_sorted_array  (ESorter  *sorter,
					       gint     **array,
					       gint      *count);
void      e_sorter_get_sorted_to_model_array  (ESorter  *sorter,
					       gint     **array,
					       gint      *count);

gboolean  e_sorter_needs_sorting              (ESorter  *sorter);

G_END_DECLS

#endif /* _E_SORTER_H_ */
