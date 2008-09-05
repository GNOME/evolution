/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-content.h
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

#ifndef E_SHELL_CONTENT_H
#define E_SHELL_CONTENT_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_CONTENT \
	(e_shell_content_get_type ())
#define E_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_CONTENT, EShellContent))
#define E_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_CONTENT, EShellContentClass))
#define E_IS_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_CONTENT))
#define E_IS_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_SHELL_CONTENT))
#define E_SHELL_CONTENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_CONTENT, EShellContentClass))

G_BEGIN_DECLS

typedef struct _EShellContent EShellContent;
typedef struct _EShellContentClass EShellContentClass;
typedef struct _EShellContentPrivate EShellContentPrivate;

struct _EShellContent {
	GtkBin parent;
	EShellContentPrivate *priv;
};

struct _EShellContentClass {
	GtkBinClass parent_class;
};

GType		e_shell_content_get_type	(void);
GtkWidget *	e_shell_content_new		(void);
GtkWidget *	e_shell_content_get_search_bar	(EShellContent *shell_content);

G_END_DECLS

#endif /* E_SHELL_CONTENT_H */
