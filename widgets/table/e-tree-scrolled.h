/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree-scrolled.h
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

#ifndef _E_TREE_SCROLLED_H_
#define _E_TREE_SCROLLED_H_

#include <gtk/gtkscrolledwindow.h>
#include <table/e-tree-model.h>
#include <table/e-tree.h>

G_BEGIN_DECLS

#define E_TREE_SCROLLED_TYPE        (e_tree_scrolled_get_type ())
#define E_TREE_SCROLLED(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TREE_SCROLLED_TYPE, ETreeScrolled))
#define E_TREE_SCROLLED_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TREE_SCROLLED_TYPE, ETreeScrolledClass))
#define E_IS_TREE_SCROLLED(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TREE_SCROLLED_TYPE))
#define E_IS_TREE_SCROLLED_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TREE_SCROLLED_TYPE))

typedef struct {
	GtkScrolledWindow parent;

	ETree *tree;
} ETreeScrolled;

typedef struct {
	GtkScrolledWindowClass parent_class;
} ETreeScrolledClass;

GType           e_tree_scrolled_get_type                  (void);

ETreeScrolled *e_tree_scrolled_construct                 (ETreeScrolled *ets,
							  ETreeModel    *etm,
							  ETableExtras   *ete,
							  const char     *spec,
							  const char     *state);
GtkWidget      *e_tree_scrolled_new                       (ETreeModel    *etm,
							   ETableExtras   *ete,
							   const char     *spec,
							   const char     *state);

ETreeScrolled *e_tree_scrolled_construct_from_spec_file  (ETreeScrolled *ets,
							  ETreeModel    *etm,
							  ETableExtras   *ete,
							  const char     *spec_fn,
							  const char     *state_fn);
GtkWidget      *e_tree_scrolled_new_from_spec_file        (ETreeModel    *etm,
							   ETableExtras   *ete,
							   const char     *spec_fn,
							   const char     *state_fn);

ETree         *e_tree_scrolled_get_tree                 (ETreeScrolled *ets);

G_END_DECLS

#endif /* _E_TREE_SCROLLED_H_ */
