/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
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
 *
 */

#ifndef E_CHARSETPICKER_H
#define E_CHARSETPICKER_H

#include <gtk/gtk.h>
#include <bonobo/bonobo-ui-component.h>

G_BEGIN_DECLS

GtkWidget *	e_charset_picker_new		(const char *default_charset);
char *		e_charset_picker_get_charset	(GtkWidget *picker);
char *		e_charset_picker_dialog		(const char *title,
						 const char *prompt,
						 const char *default_charset,
						 GtkWindow *parent);

void		e_charset_add_radio_actions	(GtkActionGroup *action_group,
						 const gchar *default_charset,
						 GCallback callback,
						 gpointer user_data);

void		e_charset_picker_bonobo_ui_populate
						(BonoboUIComponent *uic,
						 const char *path,
						 const char *default_charset,
						 BonoboUIListenerFn cb,
						 gpointer user_data);

G_END_DECLS

#endif /* E_CHARSETPICKER_H */
