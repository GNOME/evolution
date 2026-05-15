/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Miguel de Icaza <miguel@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_CELL_CHECKBOX_H_
#define _E_CELL_CHECKBOX_H_

#include <e-util/e-cell-toggle.h>

/* Standard GObject macros */
#define E_TYPE_CELL_CHECKBOX \
	(e_cell_checkbox_get_type ())
#define E_CELL_CHECKBOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_CHECKBOX, ECellCheckbox))
#define E_CELL_CHECKBOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_CHECKBOX, ECellCheckboxClass))
#define E_IS_CELL_CHECKBOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_CHECKBOX))
#define E_IS_CELL_CHECKBOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_CHECKBOX))
#define E_CELL_CHECKBOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_CHECKBOX, ECellCheckboxClass))

G_BEGIN_DECLS

typedef struct _ECellCheckbox ECellCheckbox;
typedef struct _ECellCheckboxClass ECellCheckboxClass;

struct _ECellCheckbox {
	ECellToggle parent;
};

struct _ECellCheckboxClass {
	ECellToggleClass parent_class;
};

GType		e_cell_checkbox_get_type	(void) G_GNUC_CONST;
ECell *		e_cell_checkbox_new		(void);

G_END_DECLS

#endif /* _E_CELL_CHECKBOX_H_ */

