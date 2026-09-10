/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_VIRTUAL_TREE_CUSTOMIZE_POPOVER_H
#define E_VIRTUAL_TREE_CUSTOMIZE_POPOVER_H

#include <gtk/gtk.h>
#include <e-util/e-virtual-tree.h>

#define E_TYPE_VIRTUAL_TREE_CUSTOMIZE_POPOVER (e_virtual_tree_customize_popover_get_type ())

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (EVirtualTreeCustomizePopover, e_virtual_tree_customize_popover,
	E, VIRTUAL_TREE_CUSTOMIZE_POPOVER, GtkPopover)

GtkWidget *	e_virtual_tree_customize_popover_new	(EVirtualTree *vtree);

G_END_DECLS

#endif /* E_VIRTUAL_TREE_CUSTOMIZE_POPOVER_H */
