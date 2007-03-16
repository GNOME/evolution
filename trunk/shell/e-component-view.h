/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 * e-component-view.h
 *
 * Copyright (C) 2004 Novell Inc.
 *
 * Author(s): Michael Zucchi <notzed@ximian.com>
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
 *
 */

#ifndef _E_COMPONENT_VIEW_H_
#define _E_COMPONENT_VIEW_H_

#include <bonobo/bonobo-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

struct _GtkWidget;

typedef struct _EComponentView        EComponentView;
typedef struct _EComponentViewPrivate EComponentViewPrivate;
typedef struct _EComponentViewClass   EComponentViewClass;

#include "Evolution.h"

#define E_TYPE_COMPONENT_VIEW			(e_component_view_get_type ())
#define E_COMPONENT_VIEW(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_COMPONENT_VIEW, EComponentView))
#define E_COMPONENT_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_COMPONENT_VIEW, EComponentViewClass))
#define E_IS_COMPONENT_VIEW(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_COMPONENT_VIEW))
#define E_IS_COMPONENT_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_COMPONENT_VIEW))

struct _EComponentView {
	BonoboObject parent;

	EComponentViewPrivate *priv;

	char *id;
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
EComponentView *e_component_view_new(GNOME_Evolution_ShellView shell_view, const char *id, struct _GtkWidget *side, struct _GtkWidget *view, struct _GtkWidget *status);
EComponentView *e_component_view_new_controls(GNOME_Evolution_ShellView parent, const char *id, struct _BonoboControl *side, struct _BonoboControl *view, struct _BonoboControl *statusbar);

void e_component_view_set_title(EComponentView *ecv, const char *title);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_COMPONENT_VIEW_H_ */

