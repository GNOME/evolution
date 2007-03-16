/*
 * e-cell-renderer-combo.h
 *
 * Author: Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 2003 Ximian Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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
 */

#ifndef __E_CELL_RENDERER_COMBO_H__
#define __E_CELL_RENDERER_COMBO_H__

#include <gtk/gtkcellrenderertext.h>

G_BEGIN_DECLS

#define E_TYPE_CELL_RENDERER_COMBO	   (e_cell_renderer_combo_get_type ())
#define E_CELL_RENDERER_COMBO(o)	   (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_CELL_RENDERER_COMBO, ECellRendererCombo))
#define E_CELL_RENDERER_COMBO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_CELL_RENDERER_COMBO, ECellRendererComboClass))
#define E_IS_CELL_RENDERER_COMBO(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_CELL_RENDERER_COMBO))
#define E_IS_CELL_RENDERER_COMBO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((o), E_TYPE_CELL_RENDERER_COMBO))
#define E_CELL_RENDERER_COMBO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), E_TYPE_CELL_RENDERER_COMBO, ECellRendererComboClass))

typedef struct _ECellRendererCombo      ECellRendererCombo;
typedef struct _ECellRendererComboClass ECellRendererComboClass;
typedef struct _ECellRendererComboPriv  ECellRendererComboPriv;

struct _ECellRendererCombo
{
	GtkCellRendererText  parent;

	ECellRendererComboPriv *priv;
};

struct _ECellRendererComboClass
{
	GtkCellRendererTextClass parent_class;
};

GType            e_cell_renderer_combo_get_type (void);
GtkCellRenderer *e_cell_renderer_combo_new      (void);

G_END_DECLS

#endif /* __E_CELL_RENDERER_COMBO_H__ */
