/*
 * e-book-shell-view.h
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

#ifndef E_BOOK_SHELL_VIEW_H
#define E_BOOK_SHELL_VIEW_H

#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_BOOK_SHELL_VIEW \
	(e_book_shell_view_get_type ())
#define E_BOOK_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BOOK_SHELL_VIEW, EBookShellView))
#define E_BOOK_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BOOK_SHELL_VIEW, EBookShellViewClass))
#define E_IS_BOOK_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BOOK_SHELL_VIEW))
#define E_IS_BOOK_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BOOK_SHELL_VIEW))
#define E_BOOK_SHELL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BOOK_SHELL_VIEW, EBookShellViewClass))

G_BEGIN_DECLS

typedef struct _EBookShellView EBookShellView;
typedef struct _EBookShellViewClass EBookShellViewClass;
typedef struct _EBookShellViewPrivate EBookShellViewPrivate;

struct _EBookShellView {
	EShellView parent;
	EBookShellViewPrivate *priv;
};

struct _EBookShellViewClass {
	EShellViewClass parent_class;
};

GType		e_book_shell_view_get_type	(void);
void		e_book_shell_view_type_register	(GTypeModule *type_module);

void		e_book_shell_view_disable_searching (EBookShellView *book_shell_view);
void		e_book_shell_view_enable_searching (EBookShellView *book_shell_view);
void		e_book_shell_view_open_list_editor_with_prefill
							(EShellView *shell_view,
							 EBookClient *destination_book);
ESource *	e_book_shell_view_get_clicked_source	(EShellView *shell_view);
void		e_book_shell_view_preselect_source_config
							(EShellView *shell_view,
							 GtkWidget *source_config);

G_END_DECLS

#endif /* E_BOOK_SHELL_VIEW_H */
