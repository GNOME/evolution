/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Damon Chaplin <damon@ximian.com>
 */

/*
 * ECellCombo - a subclass of ECellPopup used to support popup lists like a
 * GtkCombo widget. It only supports a basic popup list of strings at present,
 * with no auto-completion. The child ECell of the ECellPopup must be an
 * ECellText or subclass.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_CELL_COMBO_H_
#define _E_CELL_COMBO_H_

#include <e-util/e-cell-popup.h>

/* Standard GObject macros */
#define E_TYPE_CELL_COMBO \
	(e_cell_combo_get_type ())
#define E_CELL_COMBO(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_COMBO, ECellCombo))
#define E_CELL_COMBO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_COMBO, ECellComboClass))
#define E_IS_CELL_COMBO(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_COMBO))
#define E_IS_CELL_COMBO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_COMBO))
#define E_CELL_COMBO_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_COMBO, ECellComboClass))

G_BEGIN_DECLS

typedef struct _ECellCombo ECellCombo;
typedef struct _ECellComboClass ECellComboClass;

struct _ECellCombo {
	ECellPopup parent;

	GtkWidget *popup_window;
	GtkWidget *popup_scrolled_window;
	GtkWidget *popup_tree_view;
	GtkCellRenderer *popup_renderer;

	GdkDevice *grabbed_keyboard;
	GdkDevice *grabbed_pointer;
};

struct _ECellComboClass {
	ECellPopupClass parent_class;
};

GType		e_cell_combo_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_combo_new		(void);

/* These must be UTF-8. */
void		e_cell_combo_set_popdown_strings
						(ECellCombo *ecc,
						 GList *strings);

void		e_cell_combo_use_tabular_numbers
						(ECellCombo *ecc);

G_END_DECLS

#endif /* _E_CELL_COMBO_H_ */
