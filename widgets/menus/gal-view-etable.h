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

#ifndef GAL_VIEW_ETABLE_H
#define GAL_VIEW_ETABLE_H

#include <gtk/gtk.h>
#include <menus/gal-view.h>
#include <table/e-table-state.h>
#include <table/e-table-specification.h>
#include <table/e-table.h>
#include <table/e-tree.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW_ETABLE \
	(gal_view_etable_get_type ())
#define GAL_VIEW_ETABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW_ETABLE, GalViewEtable))
#define GAL_VIEW_ETABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW_ETABLE, GalViewEtableClass))
#define GAL_IS_VIEW_ETABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW_ETABLE))
#define GAL_IS_VIEW_ETABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW_ETABLE))
#define GAL_VIEW_ETABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW_ETABLE, GalViewEtableClass))

G_BEGIN_DECLS

typedef struct _GalViewEtable GalViewEtable;
typedef struct _GalViewEtableClass GalViewEtableClass;

struct _GalViewEtable {
	GalView parent;

	ETableSpecification *spec;
	ETableState *state;
	gchar *title;

	ETable *table;
	guint table_state_changed_id;

	ETree *tree;
	guint tree_state_changed_id;
};

struct _GalViewEtableClass {
	GalViewClass parent_class;
};

GType		gal_view_etable_get_type	(void);
GalView *	gal_view_etable_new		(ETableSpecification *spec,
						 const gchar *title);
GalView *	gal_view_etable_construct	(GalViewEtable *view,
						 ETableSpecification *spec,
						 const gchar *title);
void		gal_view_etable_set_state	(GalViewEtable *view,
						 ETableState *state);
void		gal_view_etable_attach_table	(GalViewEtable *view,
						 ETable *table);
void		gal_view_etable_attach_tree	(GalViewEtable *view,
						 ETree *tree);
void		gal_view_etable_detach		(GalViewEtable *view);

G_END_DECLS

#endif /* GAL_VIEW_ETABLE_H */
