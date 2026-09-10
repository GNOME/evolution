/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_VIRTUAL_TREE_ACCESSIBLE_H
#define E_VIRTUAL_TREE_ACCESSIBLE_H

#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>

G_BEGIN_DECLS

#define E_TYPE_VIRTUAL_TREE_ACCESSIBLE (e_virtual_tree_accessible_get_type ())
G_DECLARE_FINAL_TYPE (EVirtualTreeAccessible, e_virtual_tree_accessible, E, VIRTUAL_TREE_ACCESSIBLE, GtkContainerAccessible)

G_END_DECLS

#endif /* E_VIRTUAL_TREE_ACCESSIBLE_H */
