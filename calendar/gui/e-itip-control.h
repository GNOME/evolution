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
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_ITIP_CONTROL_H_
#define _E_ITIP_CONTROL_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_ITIP_CONTROL			(e_itip_control_get_type ())
#define E_ITIP_CONTROL(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_ITIP_CONTROL, EItipControl))
#define E_ITIP_CONTROL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_ITIP_CONTROL, EItipControlClass))
#define E_IS_ITIP_CONTROL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_ITIP_CONTROL))
#define E_IS_ITIP_CONTROL_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_ITIP_CONTROL))


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



GType        e_itip_control_get_type         (void);
GtkWidget *  e_itip_control_new              (void);
void         e_itip_control_set_data         (EItipControl *itip,
					      const gchar  *text);
gchar *      e_itip_control_get_data         (EItipControl *itip);
gint         e_itip_control_get_data_size    (EItipControl *itip);
void         e_itip_control_set_from_address (EItipControl *itip,
					      const gchar  *address);
const gchar *e_itip_control_get_from_address (EItipControl *itip);
void         e_itip_control_set_view_only (EItipControl *itip,
					   gboolean view_only);
gboolean     e_itip_control_get_view_only (EItipControl *itip);
void         e_itip_control_set_delegator_address   (EItipControl *itip,
						    const gchar  *address);
const gchar *e_itip_control_get_delegator_address   (EItipControl *itip);
void         e_itip_control_set_delegator_name   (EItipControl *itip,
						 const gchar  *name);
const gchar *e_itip_control_get_delegator_name   (EItipControl *itip);
void         e_itip_control_set_calendar_uid (EItipControl *itip,
					      const gchar  *uid);
const gchar *e_itip_control_get_calendar_uid (EItipControl *itip);

G_END_DECLS

#endif /* _E_ITIP_CONTROL_H_ */
