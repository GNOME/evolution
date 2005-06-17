/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell-combo.h: Combo cell renderer
 * Copyright 2001, Ximian, Inc.
 *
 * Author :
 *  Damon Chaplin <damon@ximian.com>
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
	GtkWidget *popup_list;
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
