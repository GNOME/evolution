/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ECellCombo - a subclass of ECellPopup used to support popup lists like a
 * GtkCombo widget. It only supports a basic popup list of strings at present,
 * with no auto-completion.
 */

#ifndef _E_CELL_COMBO_H_
#define _E_CELL_COMBO_H_

#include <gal/e-table/e-cell-popup.h>

#define E_CELL_COMBO_TYPE        (e_cell_combo_get_type ())
#define E_CELL_COMBO(o)          (GTK_CHECK_CAST ((o), E_CELL_COMBO_TYPE, ECellCombo))
#define E_CELL_COMBO_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_COMBO_TYPE, ECellComboClass))
#define E_IS_CELL_COMBO(o)       (GTK_CHECK_TYPE ((o), E_CELL_COMBO_TYPE))
#define E_IS_CELL_COMBO_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_COMBO_TYPE))


typedef struct {
	ECellPopup parent;

	GtkWidget *popup_window;
	GtkWidget *popup_scrolled_window;
	GtkWidget *popup_list;
} ECellCombo;

typedef struct {
	ECellPopupClass parent_class;
} ECellComboClass;


GtkType    e_cell_combo_get_type		(void);
ECell     *e_cell_combo_new			(void);

void       e_cell_combo_set_popdown_strings	(ECellCombo	*ecc, 
						 GList		*strings);

#endif /* _E_CELL_COMBO_H_ */
