/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001 Mikael Hallendal <micke@codefactory.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#ifndef __E_CELL_SPIN_BUTTON_H__
#define __E_CELL_SPIN_BUTTON_H__

#include <glib.h>
#include <gtk/gtktypeutils.h>
#include <gal/e-table/e-cell.h>

#define E_CELL_SPIN_BUTTON_TYPE        (e_cell_spin_button_get_type ())
#define E_CELL_SPIN_BUTTON(o)          (GTK_CHECK_CAST ((o), E_CELL_SPIN_BUTTON_TYPE, ECellSpinButton))
#define E_CELL_SPIN_BUTTON_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_SPIN_BUTTON_TYPE, ECellSpinButtonClass))
#define M_IS_CELL_SPIN_BUTTON(o)       (GTK_CHECK_TYPE ((o), E_CELL_SPIN_BUTTON_TYPE))
#define M_IS_CELL_SPIN_BUTTON_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_SPIN_BUTTON_TYPE))

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

GtkType    e_cell_spin_button_get_type     (void);
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

