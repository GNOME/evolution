/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-reflow-sorted.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __E_REFLOW_SORTED_H__
#define __E_REFLOW_SORTED_H__

#include <e-reflow.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EReflowSorted - A canvas item container.
 *
 * The following arguments are available:
 *
 * name		 type		read/write	description
 * --------------------------------------------------------------------------------
 * compare_func  GCompareFunc   RW              compare function
 * string_func   EReflowStringFunc RW           string function
 *
 * From EReflow:
 * minimum_width double         RW              minimum width of the reflow.  width >= minimum_width
 * width         double         R               width of the reflow
 * height        double         RW              height of the reflow
 */

#define E_REFLOW_SORTED_TYPE			(e_reflow_sorted_get_type ())
#define E_REFLOW_SORTED(obj)			(GTK_CHECK_CAST ((obj), E_REFLOW_SORTED_TYPE, EReflowSorted))
#define E_REFLOW_SORTED_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_REFLOW_SORTED_TYPE, EReflowSortedClass))
#define E_IS_REFLOW_SORTED(obj) 		(GTK_CHECK_TYPE ((obj), E_REFLOW_SORTED_TYPE))
#define E_IS_REFLOW_SORTED_CLASS(klass) 	(GTK_CHECK_CLASS_TYPE ((obj), E_REFLOW_SORTED_TYPE))

typedef char * (* EReflowStringFunc) (GnomeCanvasItem *);

typedef struct _EReflowSorted       EReflowSorted;
typedef struct _EReflowSortedClass  EReflowSortedClass;

/* FIXME: Try reimplementing this as a hash table with key as string
   and change EReflow to use a GTree. */
struct _EReflowSorted
{
	EReflow parent;
	
	/* item specific fields */
	GCompareFunc      compare_func;
	EReflowStringFunc string_func;
};

struct _EReflowSortedClass
{
	EReflowClass parent_class;
};

/* 
 * To be added to a reflow, an item must have the argument "width" as
 * a Read/Write argument and "height" as a Read Only argument.  It
 * should also do an ECanvas parent reflow request if its size
 * changes.
 */
void         	 e_reflow_sorted_remove_item  (EReflowSorted *sorted, const char *id);
void         	 e_reflow_sorted_replace_item (EReflowSorted *sorted, GnomeCanvasItem *item);
void         	 e_reflow_sorted_reorder_item (EReflowSorted *e_reflow_sorted, const gchar *id);
GnomeCanvasItem *e_reflow_sorted_get_item     (EReflowSorted *e_reflow_sorted, const gchar *id);
GtkType          e_reflow_sorted_get_type     (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_REFLOW_SORTED_H__ */
