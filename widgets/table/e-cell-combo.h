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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellCombo - a subclass of ECellPopup used to support popup lists like a
 * GtkCombo widget. It only supports a basic popup list of strings at present,
 * with no auto-completion. The child ECell of the ECellPopup must be an
 * ECellText or subclass.
 */

#ifndef _E_CELL_COMBO_H_
#define _E_CELL_COMBO_H_

#include <table/e-cell-popup.h>

#define E_CELL_COMBO_TYPE        (e_cell_combo_get_type ())
#define E_CELL_COMBO(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_COMBO_TYPE, ECellCombo))
#define E_CELL_COMBO_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_COMBO_TYPE, ECellComboClass))
#define E_IS_CELL_COMBO(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_COMBO_TYPE))
#define E_IS_CELL_COMBO_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_COMBO_TYPE))

typedef struct {
	ECellPopup parent;

	GtkWidget *popup_window;
	GtkWidget *popup_scrolled_window;
	GtkWidget *popup_tree_view;
} ECellCombo;

typedef struct {
	ECellPopupClass parent_class;
} ECellComboClass;

GType      e_cell_combo_get_type		(void);
ECell     *e_cell_combo_new			(void);

/* These must be UTF-8. */
void       e_cell_combo_set_popdown_strings	(ECellCombo	*ecc,
						 GList		*strings);

#endif /* _E_CELL_COMBO_H_ */
