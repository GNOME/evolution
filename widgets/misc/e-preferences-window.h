/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-preferences-window.h
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

#ifndef E_PREFERENCES_WINDOW_H
#define E_PREFERENCES_WINDOW_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_PREFERENCES_WINDOW \
	(e_preferences_window_get_type ())
#define E_PREFERENCES_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PREFERENCES_WINDOW, EPreferencesWindow))
#define E_PREFERENCES_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PREFERENCES_WINDOW, EPreferencesWindowClass))
#define E_IS_MULTI_CONFIG_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PREFERENCES_WINDOW))
#define E_IS_MULTI_CONFIG_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_PREFERENCES_WINDOW))
#define E_PREFERENCES_WINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_TYPE \
	((obj), E_TYPE_PREFERENCES_WINDOW, EPreferencesWindowClass))

G_BEGIN_DECLS

typedef struct _EPreferencesWindow EPreferencesWindow;
typedef struct _EPreferencesWindowClass EPreferencesWindowClass;
typedef struct _EPreferencesWindowPrivate EPreferencesWindowPrivate;

struct _EPreferencesWindow {
	GtkDialog parent;
	EPreferencesWindowPrivate *priv;
};

struct _EPreferencesWindowClass {
	GtkDialogClass parent_class;
};

GType		e_preferences_window_get_type	(void);
GtkWidget *	e_preferences_window_new	(void);
void		e_preferences_window_add_page	(EPreferencesWindow *window,
						 const gchar *page_name,
						 const gchar *icon_name,
						 const gchar *caption,
						 GtkWidget *widget,
						 gint sort_order);
void		e_preferences_window_show_page	(EPreferencesWindow *window,
						 const gchar *page_name);

G_END_DECLS

#endif /* E_PREFERENCES_WINDOW_H */
