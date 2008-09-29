/*
 * e-cell-spin-button.h: Spin button item for e-table.
 * Celltype for drawing a spinbutton in a cell.
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
 *		Mikael Hallendal <micke@imendio.com>
 *
 * Used ECellPopup by Damon Chaplin <damon@ximian.com> as base for
 * buttondrawings.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CELL_SPIN_BUTTON_H__
#define __E_CELL_SPIN_BUTTON_H__

#include <gtk/gtk.h>
#include <table/e-cell.h>

#define E_CELL_SPIN_BUTTON_TYPE        (e_cell_spin_button_get_type ())
#define E_CELL_SPIN_BUTTON(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_SPIN_BUTTON_TYPE, ECellSpinButton))
#define E_CELL_SPIN_BUTTON_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_SPIN_BUTTON_TYPE, ECellSpinButtonClass))
#define M_IS_CELL_SPIN_BUTTON(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_SPIN_BUTTON_TYPE))
#define M_IS_CELL_SPIN_BUTTON_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_SPIN_BUTTON_TYPE))

typedef union {
	gint     i;
	gfloat   f;
} ECellSpinButtonData;

typedef enum {
	STEP_UP,
	STEP_DOWN
} ECellSpinButtonStep;

typedef struct {
	ECell                  parent;

	ECell                 *child;

	ECellSpinButtonData    min;
	ECellSpinButtonData    max;
	ECellSpinButtonData    step;

	gboolean               up_pressed;
	gboolean               down_pressed;

} ECellSpinButton;

typedef struct {
	ECellClass    parent_class;

	/* Functions */
	void    (*step)    (ECellSpinButton       *mcsb,
			    ECellView             *ecv,
			    ECellSpinButtonStep    direction,
			    gint                   col,
			    gint                   row);
} ECellSpinButtonClass;

GType      e_cell_spin_button_get_type     (void);
ECell *    e_cell_spin_button_new          (gint     min,
					    gint     max,
					    gint     step,
					    ECell   *child_cell);

ECell *    e_cell_spin_button_new_float    (gfloat    min,
 					    gfloat    max,
 					    gfloat    step,
					    ECell    *child_cell);


void       e_cell_spin_button_step         (ECellSpinButton       *mcsb,
					    ECellView             *ecv,
					    ECellSpinButtonStep    direction,
					    gint                   col,
					    gint                   row);

void       e_cell_spin_button_step_float   (ECellSpinButton       *mcsb,
					    ECellView             *ecv,
					    ECellSpinButtonStep    direction,
					    gint                   col,
					    gint                   row);

#endif /* __E_CELL_SPIN_BUTTON__ */

