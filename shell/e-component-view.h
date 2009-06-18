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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_COMPONENT_VIEW_H_
#define _E_COMPONENT_VIEW_H_

#include <gtk/gtk.h>
#include <bonobo/bonobo-object.h>

G_BEGIN_DECLS

typedef struct _EComponentView        EComponentView;
typedef struct _EComponentViewPrivate EComponentViewPrivate;
typedef struct _EComponentViewClass   EComponentViewClass;

#include "Evolution.h"

#define E_TYPE_COMPONENT_VIEW			(e_component_view_get_type ())
#define E_COMPONENT_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_COMPONENT_VIEW, EComponentView))
#define E_COMPONENT_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_COMPONENT_VIEW, EComponentViewClass))
#define E_IS_COMPONENT_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_COMPONENT_VIEW))
#define E_IS_COMPONENT_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_COMPONENT_VIEW))

struct _EComponentView {
	BonoboObject parent;

	EComponentViewPrivate *priv;

	gchar *id;
	GNOME_Evolution_ShellView shell_view;

	struct _BonoboControl *side_control;
	struct _BonoboControl *view_control;
	struct _BonoboControl *statusbar_control;
};

struct _EComponentViewClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_ComponentView__epv epv;
};

GType           e_component_view_get_type(void);
EComponentView *e_component_view_new(GNOME_Evolution_ShellView shell_view, const gchar *id, GtkWidget *side, GtkWidget *view, GtkWidget *status);
EComponentView *e_component_view_new_controls(GNOME_Evolution_ShellView parent, const gchar *id, struct _BonoboControl *side, struct _BonoboControl *view, struct _BonoboControl *statusbar);

void e_component_view_set_title(EComponentView *ecv, const gchar *title);
void e_component_view_set_button_icon (EComponentView *ecv, const gchar *iconName);

G_END_DECLS

#endif /* _E_COMPONENT_VIEW_H_ */

