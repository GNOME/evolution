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
#include <filter/rule-context.h>

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

/* Avoid including <e-shell-view.h> */
struct _EShellView;

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
GtkWidget *	e_shell_content_new		(struct _EShellView *shell_view);
struct _EShellView *
		e_shell_content_get_shell_view	(EShellContent *shell_content);
RuleContext *	e_shell_content_get_context	(EShellContent *shell_content);
void		e_shell_content_set_context	(EShellContent *shell_content,
						 RuleContext *context);
GtkRadioAction *e_shell_content_get_filter_action
						(EShellContent *shell_content);
void		e_shell_content_set_filter_action
						(EShellContent *shell_content,
						 GtkRadioAction *filter_action);
gint		e_shell_content_get_filter_value(EShellContent *shell_content);
void		e_shell_content_set_filter_value(EShellContent *shell_content,
						 gint filter_value);
gboolean	e_shell_content_get_filter_visible
						(EShellContent *shell_content);
void		e_shell_content_set_filter_visible
						(EShellContent *shell_content,
						 gboolean filter_visible);
GtkRadioAction *e_shell_content_get_search_action
						(EShellContent *shell_content);
void		e_shell_content_set_search_action
						(EShellContent *shell_content,
						 GtkRadioAction *search_action);
RuleContext *	e_shell_content_get_search_context
						(EShellContent *shell_content);
const gchar *	e_shell_content_get_search_text	(EShellContent *shell_content);
void		e_shell_content_set_search_text	(EShellContent *shell_content,
						 const gchar *search_text);
gint		e_shell_content_get_search_value(EShellContent *shell_content);
void		e_shell_content_set_search_value(EShellContent *shell_content,
						 gint search_value);
gboolean	e_shell_content_get_search_visible
						(EShellContent *shell_content);
void		e_shell_content_set_search_visible
						(EShellContent *shell_content,
						 gboolean search_visible);
GtkRadioAction *e_shell_content_get_scope_action(EShellContent *shell_content);
void		e_shell_content_set_scope_action(EShellContent *shell_content,
						 GtkRadioAction *scope_action);
gint		e_shell_content_get_scope_value	(EShellContent *shell_content);
void		e_shell_content_set_scope_value	(EShellContent *shell_content,
						 gint scope_value);
gboolean	e_shell_content_get_scope_visible
						(EShellContent *shell_content);
void		e_shell_content_set_scope_visible
						(EShellContent *shell_content,
						 gboolean scope_visible);
void		e_shell_content_save_search_dialog
						(EShellContent *shell_content,
						 const gchar *filename);

G_END_DECLS

#endif /* E_SHELL_CONTENT_H */
