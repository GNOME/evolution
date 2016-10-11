/*
 * e-book-shell-content.h
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

#ifndef E_BOOK_SHELL_CONTENT_H
#define E_BOOK_SHELL_CONTENT_H

#include <libebook/libebook.h>

#include <shell/e-shell-content.h>
#include <shell/e-shell-searchbar.h>
#include <shell/e-shell-view.h>

#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "eab-composer-util.h"

/* Standard GObject macros */
#define E_TYPE_BOOK_SHELL_CONTENT \
	(e_book_shell_content_get_type ())
#define E_BOOK_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BOOK_SHELL_CONTENT, EBookShellContent))
#define E_BOOK_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BOOK_SHELL_CONTENT, EBookShellContentClass))
#define E_IS_BOOK_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BOOK_SHELL_CONTENT))
#define E_IS_BOOK_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BOOK_SHELL_CONTENT))
#define E_BOOK_SHELL_CONTENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BOOK_SHELL_CONTENT, EBookShellContentClass))

G_BEGIN_DECLS

typedef struct _EBookShellContent EBookShellContent;
typedef struct _EBookShellContentClass EBookShellContentClass;
typedef struct _EBookShellContentPrivate EBookShellContentPrivate;

enum {
	E_BOOK_SHELL_CONTENT_SELECTION_SINGLE = 1 << 0,
	E_BOOK_SHELL_CONTENT_SELECTION_MULTIPLE = 1 << 1,
	E_BOOK_SHELL_CONTENT_SELECTION_HAS_EMAIL = 1 << 2,
	E_BOOK_SHELL_CONTENT_SELECTION_IS_CONTACT_LIST = 1 << 3,
	E_BOOK_SHELL_CONTENT_SOURCE_IS_BUSY = 1 << 4,
	E_BOOK_SHELL_CONTENT_SOURCE_IS_EDITABLE = 1 << 5
};

struct _EBookShellContent {
	EShellContent parent;
	EBookShellContentPrivate *priv;
};

struct _EBookShellContentClass {
	EShellContentClass parent_class;
};

GType		e_book_shell_content_get_type	(void);
void		e_book_shell_content_type_register
					(GTypeModule *type_module);
GtkWidget *	e_book_shell_content_new
					(EShellView *shell_view);
void		e_book_shell_content_insert_view
					(EBookShellContent *book_shell_content,
					 EAddressbookView *addressbook_view);
void		e_book_shell_content_remove_view
					(EBookShellContent *book_shell_content,
					 EAddressbookView *addressbook_view);
EAddressbookView *
		e_book_shell_content_get_current_view
					(EBookShellContent *book_shell_content);
void		e_book_shell_content_set_current_view
					(EBookShellContent *book_shell_content,
					 EAddressbookView *addressbook_view);
EContact *	e_book_shell_content_get_preview_contact
					(EBookShellContent *book_shell_content);
void		e_book_shell_content_set_preview_contact
					(EBookShellContent *book_shell_content,
					 EContact *preview_contact);
EPreviewPane *	e_book_shell_content_get_preview_pane
					(EBookShellContent *book_shell_content);
gboolean	e_book_shell_content_get_preview_visible
					(EBookShellContent *book_shell_content);
void		e_book_shell_content_set_preview_visible
					(EBookShellContent *book_shell_content,
					 gboolean preview_visible);
gboolean	e_book_shell_content_get_preview_show_maps
					(EBookShellContent *book_shell_content);
void		e_book_shell_content_set_preview_show_maps
					(EBookShellContent *book_shell_content,
					 gboolean show_maps);
EShellSearchbar *
		e_book_shell_content_get_searchbar
					(EBookShellContent *book_shell_content);

G_END_DECLS

#endif /* E_BOOK_SHELL_CONTENT_H */
