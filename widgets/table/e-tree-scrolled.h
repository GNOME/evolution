/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TREE_SCROLLED_H_
#define _E_TREE_SCROLLED_H_

#include <gal/widgets/e-scroll-frame.h>
#include <gal/e-table/e-tree-model.h>
#include <gal/e-table/e-tree.h>

BEGIN_GNOME_DECLS

#define E_TREE_SCROLLED_TYPE        (e_tree_scrolled_get_type ())
#define E_TREE_SCROLLED(o)          (GTK_CHECK_CAST ((o), E_TREE_SCROLLED_TYPE, ETreeScrolled))
#define E_TREE_SCROLLED_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_SCROLLED_TYPE, ETreeScrolledClass))
#define E_IS_TREE_SCROLLED(o)       (GTK_CHECK_TYPE ((o), E_TREE_SCROLLED_TYPE))
#define E_IS_TREE_SCROLLED_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_SCROLLED_TYPE))

typedef struct {
	EScrollFrame parent;

	ETree *tree;
} ETreeScrolled;

typedef struct {
	EScrollFrameClass parent_class;
} ETreeScrolledClass;

GtkType         e_tree_scrolled_get_type                  (void);

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

END_GNOME_DECLS

#endif /* _E_TREE_SCROLLED_H_ */

