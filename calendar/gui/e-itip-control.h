/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-itip-control.h
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 * Author: JP Rosevear
 */

#ifndef _E_ITIP_CONTROL_H_
#define _E_ITIP_CONTROL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_ITIP_CONTROL			(e_itip_control_get_type ())
#define E_ITIP_CONTROL(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_ITIP_CONTROL, EItipControl))
#define E_ITIP_CONTROL_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_ITIP_CONTROL, EItipControlClass))
#define E_IS_ITIP_CONTROL(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_ITIP_CONTROL))
#define E_IS_ITIP_CONTROL_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_ITIP_CONTROL))


typedef struct _EItipControl        EItipControl;
typedef struct _EItipControlPrivate EItipControlPrivate;
typedef struct _EItipControlClass   EItipControlClass;

struct _EItipControl {
	GtkVBox parent;

	EItipControlPrivate *priv;
};

struct _EItipControlClass {
	GtkVBoxClass parent_class;
};



GtkType      e_itip_control_get_type         (void);
GtkWidget *  e_itip_control_new              (void);
void         e_itip_control_set_data         (EItipControl *itip,
					      const gchar  *text);
gchar *      e_itip_control_get_data         (EItipControl *itip);
gint         e_itip_control_get_data_size    (EItipControl *itip);
void         e_itip_control_set_from_address (EItipControl *itip,
					      const gchar  *address);
const gchar *e_itip_control_get_from_address (EItipControl *itip);
void         e_itip_control_set_my_address   (EItipControl *itip,
					      const gchar  *address);
const gchar *e_itip_control_get_my_address   (EItipControl *itip);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_ITIP_CONTROL_H_ */
