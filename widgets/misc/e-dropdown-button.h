/*
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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_DROPDOWN_BUTTON_H_
#define _E_DROPDOWN_BUTTON_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_DROPDOWN_BUTTON		  (e_dropdown_button_get_type ())
#define E_DROPDOWN_BUTTON(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_DROPDOWN_BUTTON, EDropdownButton))
#define E_DROPDOWN_BUTTON_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_DROPDOWN_BUTTON, EDropdownButtonClass))
#define E_IS_DROPDOWN_BUTTON(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_DROPDOWN_BUTTON))
#define E_IS_DROPDOWN_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_DROPDOWN_BUTTON))


typedef struct _EDropdownButton        EDropdownButton;
typedef struct _EDropdownButtonPrivate EDropdownButtonPrivate;
typedef struct _EDropdownButtonClass   EDropdownButtonClass;

struct _EDropdownButton {
	GtkToggleButton parent;

	EDropdownButtonPrivate *priv;
};

struct _EDropdownButtonClass {
	GtkToggleButtonClass parent_class;
};


GType      e_dropdown_button_get_type   (void);
void       e_dropdown_button_construct  (EDropdownButton *dropdown_button,
					 const gchar      *label_text,
					 GtkMenu         *menu);
GtkWidget *e_dropdown_button_new        (const gchar      *label_text,
					 GtkMenu         *menu);

G_END_DECLS

#endif /* _E_DROPDOWN_BUTTON_H_ */
