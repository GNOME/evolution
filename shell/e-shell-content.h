/*
 * e-shell-content.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_SHELL_CONTENT_H
#define E_SHELL_CONTENT_H

#include <shell/e-shell-common.h>

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

	/* Methods */
	guint32		(*check_state)		(EShellContent *shell_content);
	void		(*focus_search_results)	(EShellContent *shell_content);
};

GType		e_shell_content_get_type	(void);
GtkWidget *	e_shell_content_new		(struct _EShellView *shell_view);
void		e_shell_content_set_searchbar	(EShellContent *shell_content,
						 GtkWidget *searchbar);
guint32		e_shell_content_check_state	(EShellContent *shell_content);
void		e_shell_content_focus_search_results
						(EShellContent *shell_content);
GtkWidget *	e_shell_content_get_alert_bar	(EShellContent *shell_content);
struct _EShellView *
		e_shell_content_get_shell_view	(EShellContent *shell_content);
void		e_shell_content_run_advanced_search_dialog
						(EShellContent *shell_content);
void		e_shell_content_run_edit_searches_dialog
						(EShellContent *shell_content);
void		e_shell_content_run_save_search_dialog
						(EShellContent *shell_content);

G_END_DECLS

#endif /* E_SHELL_CONTENT_H */
