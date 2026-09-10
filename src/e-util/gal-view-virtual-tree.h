/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef GAL_VIEW_VIRTUAL_TREE_H
#define GAL_VIEW_VIRTUAL_TREE_H

#include <e-util/gal-view.h>
#include <e-util/e-virtual-tree.h>

#define GAL_TYPE_VIEW_VIRTUAL_TREE (gal_view_virtual_tree_get_type ())

G_DECLARE_FINAL_TYPE (GalViewVirtualTree, gal_view_virtual_tree, GAL, VIEW_VIRTUAL_TREE, GalView)

G_BEGIN_DECLS

GalView *	gal_view_virtual_tree_new	(const gchar *title);
void		gal_view_virtual_tree_attach	(GalViewVirtualTree *view,
						 EVirtualTree *vtree);
void		gal_view_virtual_tree_detach	(GalViewVirtualTree *view);
EVirtualTree *	gal_view_virtual_tree_get_virtual_tree
						(GalViewVirtualTree *view);
gboolean	gal_view_virtual_tree_util_migrate_state_file
						(const gchar *path,
						 const gchar * const *legacy_column_ids,
						 guint n_legacy_column_ids);
void		gal_view_virtual_tree_util_migrate_views_dir
						(const gchar *views_dir,
						 const gchar * const *legacy_column_ids,
						 guint n_legacy_column_ids,
						 const gchar * const *extra_state_file_prefixes,
						 const gchar * const *extra_current_view_prefixes);

G_END_DECLS

#endif /* GAL_VIEW_VIRTUAL_TREE_H */
