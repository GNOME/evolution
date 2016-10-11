/*
 * e-book-shell-view-actions.h
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

#ifndef E_BOOK_SHELL_VIEW_ACTIONS_H
#define E_BOOK_SHELL_VIEW_ACTIONS_H

#include <shell/e-shell-window-actions.h>

/* Address Book Actions */
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-copy")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-delete")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_MOVE(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-move")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-print")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_PRINT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-print-preview")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_PROPERTIES(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-properties")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_REFRESH(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-refresh")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_RENAME(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-rename")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_SAVE_AS(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-save-as")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_STOP(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-stop")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_MAP(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-map")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_POPUP_MAP(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-popup-map")

/* Contact Actions */
#define E_SHELL_WINDOW_ACTION_CONTACT_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-copy")
#define E_SHELL_WINDOW_ACTION_CONTACT_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-delete")
#define E_SHELL_WINDOW_ACTION_CONTACT_FIND(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-find")
#define E_SHELL_WINDOW_ACTION_CONTACT_FORWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-forward")
#define E_SHELL_WINDOW_ACTION_CONTACT_MOVE(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-move")
#define E_SHELL_WINDOW_ACTION_CONTACT_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-new")
#define E_SHELL_WINDOW_ACTION_CONTACT_NEW_LIST(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-new-list")
#define E_SHELL_WINDOW_ACTION_CONTACT_OPEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-open")
#define E_SHELL_WINDOW_ACTION_CONTACT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-preview")
#define E_SHELL_WINDOW_ACTION_CONTACT_PREVIEW_SHOW_MAPS(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-preview-show-maps")
#define E_SHELL_WINDOW_ACTION_CONTACT_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-print")
#define E_SHELL_WINDOW_ACTION_CONTACT_SAVE_AS(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-save-as")
#define E_SHELL_WINDOW_ACTION_CONTACT_SEND_MESSAGE(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-send-message")
#define E_SHELL_WINDOW_ACTION_CONTACT_VIEW_CLASSIC(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-view-classic")
#define E_SHELL_WINDOW_ACTION_CONTACT_VIEW_VERTICAL(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-view-vertical")

/* Search Actions */
#define E_SHELL_WINDOW_ACTION_CONTACT_SEARCH_ADVANCED_HIDDEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-search-advanced-hidden")
#define E_SHELL_WINDOW_ACTION_CONTACT_SEARCH_ANY_FIELD_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-search-any-field-contains")
#define E_SHELL_WINDOW_ACTION_CONTACT_SEARCH_EMAIL_BEGINS_WITH(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-search-email-begins-with")
#define E_SHELL_WINDOW_ACTION_CONTACT_SEARCH_NAME_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-search-name-contains")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_CONTACTS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "contacts")
#define E_SHELL_WINDOW_ACTION_GROUP_CONTACTS_FILTER(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "contacts-filter")

#endif /* E_BOOK_SHELL_VIEW_ACTIONS_H */
