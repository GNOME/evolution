/*
 * e-book-shell-view-private.h
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

#ifndef E_BOOK_SHELL_VIEW_PRIVATE_H
#define E_BOOK_SHELL_VIEW_PRIVATE_H

#include "e-book-shell-view.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <libebook/libebook.h>

#include "shell/e-shell-content.h"
#include "shell/e-shell-searchbar.h"
#include "shell/e-shell-sidebar.h"
#include "shell/e-shell-utils.h"

#include "addressbook/util/eab-book-util.h"
#include "addressbook/gui/contact-editor/e-contact-editor.h"
#include "addressbook/gui/contact-list-editor/e-contact-list-editor.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "addressbook/gui/widgets/e-addressbook-selector.h"
#include "addressbook/gui/widgets/e-contact-map-window.h"

#include "e-book-shell-backend.h"
#include "e-book-shell-content.h"
#include "e-book-shell-sidebar.h"
#include "e-book-shell-view-actions.h"

/* Shorthand, requires a variable named "shell_view". */
#define ACTION(name) \
	(E_SHELL_VIEW_ACTION_##name (shell_view))

/* ETable Specifications */
#define ETSPEC_FILENAME		"e-addressbook-view.etspec"

G_BEGIN_DECLS

/* List these in the order to be displayed.
 * Positive values are reserved for categories. */
enum {
	CONTACT_FILTER_ANY_CATEGORY = -2,
	CONTACT_FILTER_UNMATCHED = -1
};

/* List these in the order to be displayed. */
enum {
	CONTACT_SEARCH_ADVANCED = -1,
	CONTACT_SEARCH_NAME_CONTAINS,
	CONTACT_SEARCH_EMAIL_BEGINS_WITH,
	CONTACT_SEARCH_EMAIL_CONTAINS,
	CONTACT_SEARCH_PHONE_CONTAINS,
	CONTACT_SEARCH_ANY_FIELD_CONTAINS
};

struct _EBookShellViewPrivate {

	/* These are just for convenience. */
	EBookShellBackend *book_shell_backend;
	EBookShellContent *book_shell_content;
	EBookShellSidebar *book_shell_sidebar;

	EClientCache *client_cache;
	gulong backend_error_handler_id;

	ESourceRegistry *registry;
	gulong source_removed_handler_id;

	GHashTable *uid_to_view;

	/* Can track whether search changed while locked,
	 * but it is not usable at the moment. */
	gint search_locked;

	ESource *clicked_source;

	gchar *selected_source_uid;
};

void		e_book_shell_view_private_init
					(EBookShellView *book_shell_view);
void		e_book_shell_view_private_constructed
					(EBookShellView *book_shell_view);
void		e_book_shell_view_private_dispose
					(EBookShellView *book_shell_view);
void		e_book_shell_view_private_finalize
					(EBookShellView *book_shell_view);

/* Private Utilities */

void		e_book_shell_view_actions_init
					(EBookShellView *book_shell_view);
void		e_book_shell_view_update_search_filter
					(EBookShellView *book_shell_view);

G_END_DECLS

#endif /* E_BOOK_SHELL_VIEW_PRIVATE_H */
