/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-bonobo-widget.h
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_BONOBO_WIDGET_H_
#define _E_BONOBO_WIDGET_H_

#include <bonobo/bonobo-widget.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_BONOBO_WIDGET			(e_bonobo_widget_get_type ())
#define E_BONOBO_WIDGET(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_BONOBO_WIDGET, EBonoboWidget))
#define E_BONOBO_WIDGET_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_BONOBO_WIDGET, EBonoboWidgetClass))
#define E_IS_BONOBO_WIDGET(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_BONOBO_WIDGET))
#define E_IS_BONOBO_WIDGET_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_BONOBO_WIDGET))


typedef struct _EBonoboWidget        EBonoboWidget;
typedef struct _EBonoboWidgetPrivate EBonoboWidgetPrivate;
typedef struct _EBonoboWidgetClass   EBonoboWidgetClass;

struct _EBonoboWidget {
	BonoboWidget parent;

	EBonoboWidgetPrivate *priv;
};

struct _EBonoboWidgetClass {
	BonoboWidgetClass parent_class;
};


#define E_BONOBO_WIDGET_TOPLEVEL_PROPERTY_ID "bonobo:toplevel"


GtkType        e_bonobo_widget_get_type                       (void);
EBonoboWidget *e_bonobo_widget_construct_control_from_objref  (EBonoboWidget      *widget,
							       Bonobo_Control      control,
							       Bonobo_UIContainer  uic);
EBonoboWidget *e_bonobo_widget_construct_control              (EBonoboWidget      *widget,
							       const char         *moniker,
							       Bonobo_UIContainer  uic);
GtkWidget     *e_bonobo_widget_new_control                    (const char         *moniker,
							       Bonobo_UIContainer  uic);
GtkWidget     *e_bonobo_widget_new_control_from_objref        (Bonobo_Control      control,
							       Bonobo_UIContainer  uic);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_BONOBO_WIDGET_H_ */
