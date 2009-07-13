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
 *		Harry Lu  <harry.lu@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EA_COMBO_BUTTON_H_
#define __EA_COMBO_BUTTON_H_

#include <gtk/gtk.h>
#include <misc/e-combo-button.h>

#define EA_TYPE_COMBO_BUTTON			(ea_combo_button_get_type ())
#define EA_COMBO_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_COMBO_BUTTON, EaComboButton))
#define EA_COMBO_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_COMBO_BUTTON, EaComboButtonClass))
#define EA_IS_COMBO_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_COMBO_BUTTON))
#define EA_IS_COMBO_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_COMBO_BUTTON))

typedef struct _EaComboButton EaComboButton;
typedef struct _EaComboButtonClass EaComboButtonClass;

struct _EaComboButton {
	GtkAccessible object;
};

struct _EaComboButtonClass {
	GtkAccessibleClass parent_class;
};

/* Standard Glib function */
GType      ea_combo_button_get_type  (void);
AtkObject *ea_combo_button_new       (GtkWidget *combo_button);

#endif /* ! __EA_COMBO_BUTTON_H_ */
