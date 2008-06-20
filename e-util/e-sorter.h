/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-sorter.h
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _E_SORTER_H_
#define _E_SORTER_H_

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
						 int         row);
	gint      (*sorted_to_model)            (ESorter    *sorter,
						 int         row);

	void      (*get_model_to_sorted_array)  (ESorter    *sorter,
						 int       **array,
						 int        *count);
	void      (*get_sorted_to_model_array)  (ESorter    *sorter,
						 int       **array,
						 int        *count);

	gboolean  (*needs_sorting)              (ESorter    *sorter);
} ESorterClass;

GType     e_sorter_get_type                   (void);
ESorter  *e_sorter_new                        (void);

gint      e_sorter_model_to_sorted            (ESorter  *sorter,
					       int       row);
gint      e_sorter_sorted_to_model            (ESorter  *sorter,
					       int       row);

void      e_sorter_get_model_to_sorted_array  (ESorter  *sorter,
					       int     **array,
					       int      *count);
void      e_sorter_get_sorted_to_model_array  (ESorter  *sorter,
					       int     **array,
					       int      *count);

gboolean  e_sorter_needs_sorting              (ESorter  *sorter);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SORTER_H_ */
