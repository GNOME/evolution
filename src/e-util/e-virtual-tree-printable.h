/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_VIRTUAL_TREE_PRINTABLE_H
#define E_VIRTUAL_TREE_PRINTABLE_H

#include <e-util/e-printable.h>
#include <e-util/e-virtual-tree.h>

G_BEGIN_DECLS

#define E_TYPE_VIRTUAL_TREE_PRINTABLE (e_virtual_tree_printable_get_type ())
G_DECLARE_FINAL_TYPE (EVirtualTreePrintable, e_virtual_tree_printable, E, VIRTUAL_TREE_PRINTABLE, EPrintable)

EPrintable *	e_virtual_tree_printable_new		(EVirtualTree *vtree);

G_END_DECLS

#endif /* E_VIRTUAL_TREE_PRINTABLE_H */
