/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-view-etable.h
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

#ifndef _GAL_VIEW_ETABLE_H_
#define _GAL_VIEW_ETABLE_H_

#include <gtk/gtkobject.h>
#include <widgets/menus/gal-view.h>
#include <table/e-table-state.h>
#include <table/e-table-specification.h>
#include <table/e-table.h>
#include <table/e-tree.h>

G_BEGIN_DECLS

#define GAL_VIEW_ETABLE_TYPE        (gal_view_etable_get_type ())
#define GAL_VIEW_ETABLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_VIEW_ETABLE_TYPE, GalViewEtable))
#define GAL_VIEW_ETABLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_VIEW_ETABLE_TYPE, GalViewEtableClass))
#define GAL_IS_VIEW_ETABLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_VIEW_ETABLE_TYPE))
#define GAL_IS_VIEW_ETABLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_VIEW_ETABLE_TYPE))

typedef struct {
	GalView base;

	ETableSpecification *spec;
	ETableState         *state;
	char                *title;

	ETable              *table;
	guint                table_state_changed_id;

	ETree               *tree;
	guint                tree_state_changed_id;
} GalViewEtable;

typedef struct {
	GalViewClass parent_class;
} GalViewEtableClass;

/* Standard functions */
GType    gal_view_etable_get_type      (void);
GalView *gal_view_etable_new           (ETableSpecification *spec,
					const gchar         *title);
GalView *gal_view_etable_construct     (GalViewEtable       *view,
					ETableSpecification *spec,
					const gchar         *title);
void     gal_view_etable_set_state     (GalViewEtable       *view,
					ETableState         *state);
void     gal_view_etable_attach_table  (GalViewEtable       *view,
					ETable              *table);
void     gal_view_etable_attach_tree   (GalViewEtable       *view,
					ETree               *tree);
void     gal_view_etable_detach        (GalViewEtable       *view);


G_END_DECLS

#endif /* _GAL_VIEW_ETABLE_H_ */
