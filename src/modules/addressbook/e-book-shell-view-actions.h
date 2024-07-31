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

#include <shell/e-shell-view.h>

/* Address Book Actions */
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_COPY(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-copy")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-delete")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_MOVE(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-move")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_PRINT(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-print")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_PRINT_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-print-preview")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_PROPERTIES(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-properties")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_REFRESH(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-refresh")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_REFRESH_BACKEND(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-refresh-backend")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_RENAME(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-rename")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_SAVE_AS(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-save-as")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_STOP(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-stop")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_MAP(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-map")
#define E_SHELL_VIEW_ACTION_ADDRESS_BOOK_MAP_POPUP(view) \
	E_SHELL_VIEW_ACTION ((view), "address-book-map-popup")

/* Contact Actions */
#define E_SHELL_VIEW_ACTION_CONTACT_BULK_EDIT(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-bulk-edit")
#define E_SHELL_VIEW_ACTION_CONTACT_COPY(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-copy")
#define E_SHELL_VIEW_ACTION_CONTACT_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-delete")
#define E_SHELL_VIEW_ACTION_CONTACT_FIND(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-find")
#define E_SHELL_VIEW_ACTION_CONTACT_FORWARD(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-forward")
#define E_SHELL_VIEW_ACTION_CONTACT_MOVE(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-move")
#define E_SHELL_VIEW_ACTION_CONTACT_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-new")
#define E_SHELL_VIEW_ACTION_CONTACT_NEW_LIST(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-new-list")
#define E_SHELL_VIEW_ACTION_CONTACT_OPEN(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-open")
#define E_SHELL_VIEW_ACTION_CONTACT_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-preview")
#define E_SHELL_VIEW_ACTION_CONTACT_PREVIEW_SHOW_MAPS(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-preview-show-maps")
#define E_SHELL_VIEW_ACTION_CONTACT_PRINT(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-print")
#define E_SHELL_VIEW_ACTION_CONTACT_SAVE_AS(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-save-as")
#define E_SHELL_VIEW_ACTION_CONTACT_SEND_MESSAGE(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-send-message")
#define E_SHELL_VIEW_ACTION_CONTACT_VIEW_CLASSIC(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-view-classic")
#define E_SHELL_VIEW_ACTION_CONTACT_VIEW_VERTICAL(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-view-vertical")

/* Search Actions */
#define E_SHELL_VIEW_ACTION_CONTACT_SEARCH_ADVANCED_HIDDEN(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-search-advanced-hidden")
#define E_SHELL_VIEW_ACTION_CONTACT_SEARCH_ANY_FIELD_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-search-any-field-contains")
#define E_SHELL_VIEW_ACTION_CONTACT_SEARCH_EMAIL_BEGINS_WITH(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-search-email-begins-with")
#define E_SHELL_VIEW_ACTION_CONTACT_SEARCH_EMAIL_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-search-email-contains")
#define E_SHELL_VIEW_ACTION_CONTACT_SEARCH_NAME_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-search-name-contains")
#define E_SHELL_VIEW_ACTION_CONTACT_SEARCH_PHONE_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-search-phone-contains")

/* Sort Cards By Actions */
#define E_SHELL_VIEW_ACTION_CONTACT_CARDS_SORT_BY_MENU(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-cards-sort-by-menu")
#define E_SHELL_VIEW_ACTION_CONTACT_CARDS_SORT_BY_FILE_AS(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-cards-sort-by-file-as")
#define E_SHELL_VIEW_ACTION_CONTACT_CARDS_SORT_BY_GIVEN_NAME(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-cards-sort-by-given-name")
#define E_SHELL_VIEW_ACTION_CONTACT_CARDS_SORT_BY_FAMILY_NAME(view) \
	E_SHELL_VIEW_ACTION ((view), "contact-cards-sort-by-family-name")

#endif /* E_BOOK_SHELL_VIEW_ACTIONS_H */
