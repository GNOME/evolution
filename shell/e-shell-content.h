/*
 * e-shell-content.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/**
 * SECTION: e-shell-content
 * @short_description: the right side of the main window
 * @include: shell/e-shell-content.h
 **/

#ifndef E_SHELL_CONTENT_H
#define E_SHELL_CONTENT_H

#include <shell/e-shell-common.h>
#include <filter/e-filter-rule.h>
#include <filter/e-rule-context.h>

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

/* Avoid including <e-shell-view.h>, because it includes us! */
struct _EShellView;

typedef struct _EShellContent EShellContent;
typedef struct _EShellContentClass EShellContentClass;
typedef struct _EShellContentPrivate EShellContentPrivate;

/**
 * EShellContent:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellContent {
	GtkBin parent;
	EShellContentPrivate *priv;
};

struct _EShellContentClass {
	GtkBinClass parent_class;

	/* Factory Methods */
	ERuleContext *	(*new_search_context)	(void);

	guint32		(*check_state)		(EShellContent *shell_content);
};

GType		e_shell_content_get_type	(void);
GtkWidget *	e_shell_content_new		(struct _EShellView *shell_view);
guint32		e_shell_content_check_state	(EShellContent *shell_content);
struct _EShellView *
		e_shell_content_get_shell_view	(EShellContent *shell_content);
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
void		e_shell_content_add_filter_separator_before
						(EShellContent *shell_content,
						 gint action_value);
void		e_shell_content_add_filter_separator_after
						(EShellContent *shell_content,
						 gint action_value);
ERuleContext *	e_shell_content_get_search_context
						(EShellContent *shell_content);
const gchar *	e_shell_content_get_search_hint	(EShellContent *shell_content);
void		e_shell_content_set_search_hint	(EShellContent *shell_content,
						 const gchar *search_hint);
EFilterRule *	e_shell_content_get_search_rule	(EShellContent *shell_content);
void		e_shell_content_set_search_rule (EShellContent *shell_content,
						 EFilterRule *search_rule);
gchar *		e_shell_content_get_search_rule_as_string
						(EShellContent *shell_content);
const gchar *	e_shell_content_get_search_text	(EShellContent *shell_content);
void		e_shell_content_set_search_text	(EShellContent *shell_content,
						 const gchar *search_text);
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
void		e_shell_content_set_search_radio_action
						(EShellContent *shell_content,
						 GtkRadioAction *search_action);
GtkRadioAction *e_shell_content_get_search_radio_action
						(EShellContent *shell_content);
const gchar *	e_shell_content_get_view_id	(EShellContent *shell_content);
void		e_shell_content_set_view_id	(EShellContent *shell_content,
						 const gchar *view_id);
void		e_shell_content_run_advanced_search_dialog
						(EShellContent *shell_content);
void		e_shell_content_run_edit_searches_dialog
						(EShellContent *shell_content);
void		e_shell_content_run_save_search_dialog
						(EShellContent *shell_content);
void		e_shell_content_restore_state	(EShellContent *shell_content,
						 const gchar *group_name);

G_END_DECLS

#endif /* E_SHELL_CONTENT_H */
