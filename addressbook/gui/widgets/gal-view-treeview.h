/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-treeview.h: An TreeView View
 *
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#ifndef _GAL_VIEW_TREEVIEW_H_
#define _GAL_VIEW_TREEVIEW_H_

#include <widgets/menus/gal-view.h>
#include <gtk/gtktreeview.h>

#define GAL_TYPE_VIEW_TREEVIEW        (gal_view_treeview_get_type ())
#define GAL_VIEW_TREEVIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_TYPE_VIEW_TREEVIEW, GalViewTreeView))
#define GAL_VIEW_TREEVIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_TYPE_VIEW_TREEVIEW, GalViewTreeViewClass))
#define GAL_IS_VIEW_TREEVIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_TYPE_VIEW_TREEVIEW))
#define GAL_IS_VIEW_TREEVIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_TYPE_VIEW_TREEVIEW))

typedef struct {
	GalView              base;

	char                *title;

	GtkTreeView         *tree;
} GalViewTreeView;

typedef struct {
	GalViewClass parent_class;
} GalViewTreeViewClass;

/* Standard functions */
GType    gal_view_treeview_get_type   (void);
GalView *gal_view_treeview_new        (const gchar         *title);
GalView *gal_view_treeview_construct  (GalViewTreeView     *view,
				       const gchar         *title);
void     gal_view_treeview_attach     (GalViewTreeView     *view,
				       GtkTreeView *tree);
void     gal_view_treeview_detach     (GalViewTreeView     *view);

#endif /* _GAL_VIEW_TREEVIEW_H_ */
