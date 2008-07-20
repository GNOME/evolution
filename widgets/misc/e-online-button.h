/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-online-button.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_ONLINE_BUTTON_H
#define E_ONLINE_BUTTON_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_ONLINE_BUTTON \
	(e_online_button_get_type ())
#define E_ONLINE_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ONLINE_BUTTON, EOnlineButton))
#define E_ONLINE_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ONLINE_BUTTON, EOnlineButtonClass))
#define E_IS_ONLINE_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ONLINE_BUTTON))
#define E_IS_ONLINE_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ONLINE_BUTTON))
#define E_ONLINE_BUTTON_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ONLINE_BUTTON, EOnlineButtonClass))

G_BEGIN_DECLS

typedef struct _EOnlineButton EOnlineButton;
typedef struct _EOnlineButtonClass EOnlineButtonClass;
typedef struct _EOnlineButtonPrivate EOnlineButtonPrivate;

struct _EOnlineButton {
	GtkButton parent;
	EOnlineButtonPrivate *priv;
};

struct _EOnlineButtonClass {
	GtkButtonClass parent_class;
};

GType		e_online_button_get_type	(void);
GtkWidget *	e_online_button_new		(void);
gboolean	e_online_button_get_online	(EOnlineButton *button);
void		e_online_button_set_online	(EOnlineButton *button,
						 gboolean online);

G_END_DECLS

#endif /* E_ONLINE_BUTTON_H */
