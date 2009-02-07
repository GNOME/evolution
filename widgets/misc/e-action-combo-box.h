/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-action-combo-box.h
 *
 * Copyright (C) 2008 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef E_ACTION_COMBO_BOX_H
#define E_ACTION_COMBO_BOX_H

/* This is a GtkComboBox that is driven by a group of GtkRadioActions.
 * Just plug in a GtkRadioAction and the widget will handle the rest.
 * (Based on GtkhtmlComboBox.) */

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_ACTION_COMBO_BOX \
	(e_action_combo_box_get_type ())
#define E_ACTION_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACTION_COMBO_BOX, EActionComboBox))
#define E_ACTION_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACTION_COMBO_BOX, EActionComboBoxClass))
#define E_ACTION_IS_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACTION_COMBO_BOX))
#define E_ACTION_IS_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ACTION_COMBO_BOX))
#define E_ACTION_COMBO_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ACTION_COMBO_BOX, EActionComboBoxClass))

G_BEGIN_DECLS

typedef struct _EActionComboBox EActionComboBox;
typedef struct _EActionComboBoxClass EActionComboBoxClass;
typedef struct _EActionComboBoxPrivate EActionComboBoxPrivate;

struct _EActionComboBox {
	GtkComboBox parent;
	EActionComboBoxPrivate *priv;
};

struct _EActionComboBoxClass {
	GtkComboBoxClass parent_class;
};

GType		e_action_combo_box_get_type	(void);
GtkWidget *	e_action_combo_box_new		(void);
GtkWidget *	e_action_combo_box_new_with_action
						(GtkRadioAction *action);
GtkRadioAction *e_action_combo_box_get_action	(EActionComboBox *combo_box);
void		e_action_combo_box_set_action	(EActionComboBox *combo_box,
						 GtkRadioAction *action);
gint		e_action_combo_box_get_current_value
						(EActionComboBox *combo_box);
void		e_action_combo_box_set_current_value
						(EActionComboBox *combo_box,
						 gint current_value);
void		e_action_combo_box_add_separator_before
						(EActionComboBox *combo_box,
						 gint action_value);
void		e_action_combo_box_add_separator_after
						(EActionComboBox *combo_box,
						 gint action_value);

G_END_DECLS

#endif /* E_ACTION_COMBO_BOX_H */
