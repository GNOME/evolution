/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view.h
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

#ifndef E_SHELL_VIEW_H
#define E_SHELL_VIEW_H

#include <e-shell-common.h>
#include <e-shell-window.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_VIEW \
	(e_shell_view_get_type ())
#define E_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_VIEW, EShellView))
#define E_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_VIEW, EShellViewClass))
#define E_IS_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_VIEW))
#define E_IS_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_VIEW))
#define E_SHELL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_VIEW, EShellViewClass))

G_BEGIN_DECLS

typedef struct _EShellView EShellView;
typedef struct _EShellViewClass EShellViewClass;
typedef struct _EShellViewPrivate EShellViewPrivate;

struct _EShellView {
	GObject parent;
	EShellViewPrivate *priv;
};

struct _EShellViewClass {
	GObjectClass parent_class;

	/* Initial GtkRadioAction values */
	const gchar *label;
	const gchar *icon_name;

	/* Subclasses should set this via the "class_data" field in
	 * the GTypeInfo they pass to g_type_module_register_type(). */
	GTypeModule *type_module;

	GtkWidget *	(*get_content_widget)	(EShellView *shell_view);
	GtkWidget *	(*get_sidebar_widget)	(EShellView *shell_view);
	GtkWidget *	(*get_status_widget)	(EShellView *shell_view);

	/* Signals */

	void		(*changed)		(EShellView *shell_view);
};

GType		e_shell_view_get_type		(void);
const gchar *	e_shell_view_get_name		(EShellView *shell_view);
const gchar *	e_shell_view_get_icon_name	(EShellView *shell_view);
void		e_shell_view_set_icon_name	(EShellView *shell_view,
						 const gchar *icon_name);
const gchar *	e_shell_view_get_primary_text	(EShellView *shell_view);
void		e_shell_view_set_primary_text	(EShellView *shell_view,
						 const gchar *primary_text);
const gchar *	e_shell_view_get_secondary_text	(EShellView *shell_view);
void		e_shell_view_set_secondary_text	(EShellView *shell_view,
						 const gchar *secondary_text);
const gchar *	e_shell_view_get_title		(EShellView *shell_view);
void		e_shell_view_set_title		(EShellView *shell_view,
						 const gchar *title);
EShellWindow *	e_shell_view_get_window		(EShellView *shell_view);
gboolean	e_shell_view_is_selected	(EShellView *shell_view);
gint		e_shell_view_get_page_num	(EShellView *shell_view);
GtkWidget *	e_shell_view_get_content_widget (EShellView *shell_view);
GtkWidget *	e_shell_view_get_sidebar_widget (EShellView *shell_view);
GtkWidget *	e_shell_view_get_status_widget	(EShellView *shell_view);
void		e_shell_view_changed		(EShellView *shell_view);

G_END_DECLS

#endif /* E_SHELL_VIEW_H */
