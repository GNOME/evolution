/* e-action-combo-box.h
 *
 * Copyright (C) 2008 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ACTION_COMBO_BOX_H
#define E_ACTION_COMBO_BOX_H

/* This is a GtkComboBox that is driven by a group of an EUIAction.
 * Just plug in an EUIAction and the widget will handle the rest.
 * (Based on GtkhtmlComboBox.) */

#include <gtk/gtk.h>
#include <e-util/e-ui-action.h>

/* Standard GObject macros */
#define E_TYPE_ACTION_COMBO_BOX \
	(e_action_combo_box_get_type ())
#define E_ACTION_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACTION_COMBO_BOX, EActionComboBox))
#define E_ACTION_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACTION_COMBO_BOX, EActionComboBoxClass))
#define E_IS_ACTION_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACTION_COMBO_BOX))
#define E_IS_ACTION_COMBO_BOX_CLASS(cls) \
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

GType		e_action_combo_box_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_action_combo_box_new		(void);
GtkWidget *	e_action_combo_box_new_with_action
						(EUIAction *action);
EUIAction *	e_action_combo_box_get_action	(EActionComboBox *combo_box);
void		e_action_combo_box_set_action	(EActionComboBox *combo_box,
						 EUIAction *action);
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
void		e_action_combo_box_update_model	(EActionComboBox *combo_box);
gboolean	e_action_combo_box_get_ellipsize_enabled
						(EActionComboBox *combo_box);
void		e_action_combo_box_set_ellipsize_enabled
						(EActionComboBox *combo_box,
						 gboolean enabled);

G_END_DECLS

#endif /* E_ACTION_COMBO_BOX_H */
